#include "avx2_internal.h"

static void vn_avx2_build_coord_lut(vn_u8* out_lut, vn_u32 count, vn_u32 local_start, vn_u16 extent) {
    vn_u32 i;
    vn_u32 denom;
    vn_u32 value;

    if (out_lut == (vn_u8*)0 || count == 0u) {
        return;
    }
    if (extent <= 1u) {
        for (i = 0u; i < count; ++i) {
            out_lut[i] = 0u;
        }
        return;
    }

    denom = (vn_u32)extent - 1u;
    value = local_start * 255u;
    for (i = 0u; i < count; ++i) {
        vn_u32 q;
        q = value / denom;
        if (q > 255u) {
            q = 255u;
        }
        out_lut[i] = (vn_u8)q;
        value += 255u;
    }
}

/* Mirror the pixel pipeline hot path locally so the AVX2 backend can keep its textured rows
 * inside one translation unit instead of paying per-pixel cross-TU helper calls. */
static int vn_avx2_clamp_u8_int(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

static vn_u32 vn_avx2_hash32(vn_u32 x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

vn_u32 vn_avx2_blend_rgb_local(vn_u32 dst, vn_u32 src, vn_u8 alpha) {
    vn_u32 inv;
    vn_u32 dr;
    vn_u32 dg;
    vn_u32 db;
    vn_u32 sr;
    vn_u32 sg;
    vn_u32 sb;
    vn_u32 rr;
    vn_u32 rg;
    vn_u32 rb;

    if (alpha >= 255u) {
        return src;
    }
    if (alpha == 0u) {
        return dst;
    }

    inv = (vn_u32)(255u - alpha);
    dr = (dst >> 16) & 0xFFu;
    dg = (dst >> 8) & 0xFFu;
    db = dst & 0xFFu;
    sr = (src >> 16) & 0xFFu;
    sg = (src >> 8) & 0xFFu;
    sb = src & 0xFFu;

    rr = (sr * (vn_u32)alpha + dr * inv + 127u) / 255u;
    rg = (sg * (vn_u32)alpha + dg * inv + 127u) / 255u;
    rb = (sb * (vn_u32)alpha + db * inv + 127u) / 255u;

    return (vn_u32)(0xFF000000u | (rr << 16) | (rg << 8) | rb);
}

typedef struct VN_AVX2TexturedRowParams {
    vn_u32 seed_xor;
    vn_u32 checker_xor;
    vn_u32 v8;
    int base_r;
    int base_g;
    int base_b;
    int text_blue_bias;
    int sprite_blue_bias;
    vn_u8 op;
#if VN_AVX2_IMPL_AVAILABLE
    int seed_xor_lanes[8];
    int checker_xor_lanes[8];
    int v8_lanes[8];
    int base_r_lanes[8];
    int base_g_lanes[8];
    int base_b_lanes[8];
    int text_blue_bias_lanes[8];
    int sprite_blue_bias_lanes[8];
#endif
} VN_AVX2TexturedRowParams;

static void vn_avx2_init_textured_row_params(VN_AVX2TexturedRowParams* params,
                                             vn_u32 v8,
                                             vn_u16 tex_id,
                                             vn_u8 layer,
                                             vn_u8 flags,
                                             vn_u8 op) {
    if (params == (VN_AVX2TexturedRowParams*)0) {
        return;
    }

    params->v8 = (v8 & 0xFFu);
    params->seed_xor = ((vn_u32)tex_id * 2654435761u) ^ params->v8 ^ ((vn_u32)tex_id << 16);
    params->checker_xor = ((params->v8 >> 5) & 0xFFu) ^ ((vn_u32)tex_id & 7u);
    params->base_r = (int)layer * 7;
    params->base_g = (int)layer * 5;
    params->base_b = (int)layer * 3;
    if ((flags & 1u) != 0u) {
        params->base_g += 14;
    }
    if ((flags & 2u) != 0u) {
        params->base_b += 20;
    }
    if ((flags & 4u) != 0u) {
        params->base_r += 28;
        params->base_g -= 12;
    }
    if ((flags & 8u) != 0u) {
        params->base_r += 12;
        params->base_g += 12;
        params->base_b -= 8;
    }
    params->text_blue_bias = 24 + (int)layer * 6;
    params->sprite_blue_bias = 10;
    params->op = op;
#if VN_AVX2_IMPL_AVAILABLE
    {
        int lane;

        for (lane = 0; lane < 8; ++lane) {
            params->seed_xor_lanes[lane] = (int)params->seed_xor;
            params->checker_xor_lanes[lane] = (int)params->checker_xor;
            params->v8_lanes[lane] = (int)params->v8;
            params->base_r_lanes[lane] = params->base_r;
            params->base_g_lanes[lane] = params->base_g;
            params->base_b_lanes[lane] = params->base_b;
            params->text_blue_bias_lanes[lane] = params->text_blue_bias;
            params->sprite_blue_bias_lanes[lane] = params->sprite_blue_bias;
        }
    }
#endif
}

static vn_u32 vn_avx2_sample_combine_texel(vn_u32 u8, const VN_AVX2TexturedRowParams* params) {
    vn_u32 seed;
    vn_u32 h;
    int r;
    int g;
    int b;

    if (params == (const VN_AVX2TexturedRowParams*)0) {
        return 0u;
    }

    u8 &= 0xFFu;
    seed = (u8 << 8) ^ params->seed_xor;
    h = vn_avx2_hash32(seed);

    r = (int)(h & 0xFFu);
    g = (int)((h >> 8) & 0xFFu);
    b = (int)((h >> 16) & 0xFFu);

    if ((((u8 >> 5) ^ params->checker_xor) & 1u) != 0u) {
        r += 24;
        g += 24;
        b += 24;
    } else if (((u8 + params->v8) & 0x20u) != 0u) {
        r -= 16;
        g -= 10;
        b -= 16;
    }

    r = vn_avx2_clamp_u8_int(r);
    g = vn_avx2_clamp_u8_int(g);
    b = vn_avx2_clamp_u8_int(b);

    r += params->base_r;
    g += params->base_g;
    b += params->base_b;

    if (params->op == VN_OP_TEXT) {
        int y;

        y = (r * 54 + g * 183 + b * 19) >> 8;
        r = y + 52;
        g = y + 44;
        b = y + params->text_blue_bias;
    } else if (params->op == VN_OP_SPRITE) {
        b += params->sprite_blue_bias;
    }

    r = vn_avx2_clamp_u8_int(r);
    g = vn_avx2_clamp_u8_int(g);
    b = vn_avx2_clamp_u8_int(b);
    return (vn_u32)(0xFF000000u | ((vn_u32)r << 16) | ((vn_u32)g << 8) | (vn_u32)b);
}

#if VN_AVX2_IMPL_AVAILABLE
VN_AVX2_TARGET_ATTR
static __m256i vn_avx2_sample_combine_chunk_u32x8(__m256i u_vec,
                                                  const VN_AVX2TexturedRowParams* params);
#endif

VN_AVX2_TARGET_ATTR
static void vn_avx2_build_textured_row_palette(vn_u32* palette,
                                               const VN_AVX2TexturedRowParams* params) {
    vn_u32 i;

    if (palette == (vn_u32*)0 || params == (const VN_AVX2TexturedRowParams*)0) {
        return;
    }

#if VN_AVX2_IMPL_AVAILABLE
    {
        __m256i u_vec;
        __m256i step_vec;

        u_vec = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
        step_vec = _mm256_set1_epi32(8);
        i = 0u;
        while ((i + 8u) <= 256u) {
            __m256i src_vec;

            src_vec = vn_avx2_sample_combine_chunk_u32x8(u_vec, params);
            _mm256_storeu_si256((__m256i*)(void*)(palette + i), src_vec);
            u_vec = _mm256_add_epi32(u_vec, step_vec);
            i += 8u;
        }
    }
#else
    i = 0u;
#endif
    while (i < 256u) {
        palette[i] = vn_avx2_sample_combine_texel(i, params);
        i += 1u;
    }
}

#if VN_AVX2_IMPL_AVAILABLE
VN_AVX2_TARGET_ATTR
static __m256i vn_avx2_load_u_lut8_u32x8(const vn_u8* u_lut) {
    __m128i idx8;

    idx8 = _mm_loadl_epi64((const __m128i*)(const void*)u_lut);
    return _mm256_cvtepu8_epi32(idx8);
}

VN_AVX2_TARGET_ATTR
static __m256i vn_avx2_hash32_u32x8(__m256i x) {
    __m256i mul0;
    __m256i mul1;

    mul0 = _mm256_set1_epi32((int)0x7FEB352Du);
    mul1 = _mm256_set1_epi32((int)0x846CA68Bu);
    x = _mm256_xor_si256(x, _mm256_srli_epi32(x, 16));
    x = _mm256_mullo_epi32(x, mul0);
    x = _mm256_xor_si256(x, _mm256_srli_epi32(x, 15));
    x = _mm256_mullo_epi32(x, mul1);
    x = _mm256_xor_si256(x, _mm256_srli_epi32(x, 16));
    return x;
}

VN_AVX2_TARGET_ATTR
static __m256i vn_avx2_sample_combine_chunk_u32x8(__m256i u_vec,
                                                  const VN_AVX2TexturedRowParams* params) {
    __m256i mask_ff;
    __m256i one_vec;
    __m256i alt_bit;
    __m256i alpha_mask;
    __m256i zero_vec;
    __m256i max_vec;
    __m256i seed_xor_vec;
    __m256i checker_xor_vec;
    __m256i v8_vec;
    __m256i hash_vec;
    __m256i checker_mask;
    __m256i alt_mask;
    __m256i r_vec;
    __m256i g_vec;
    __m256i b_vec;

    if (params == (const VN_AVX2TexturedRowParams*)0) {
        return _mm256_setzero_si256();
    }

    mask_ff = _mm256_set1_epi32((int)0xFFu);
    one_vec = _mm256_set1_epi32(1);
    alt_bit = _mm256_set1_epi32((int)0x20u);
    alpha_mask = _mm256_set1_epi32((int)0xFF000000u);
    zero_vec = _mm256_setzero_si256();
    max_vec = _mm256_set1_epi32(255);
    seed_xor_vec = _mm256_loadu_si256((const __m256i*)(const void*)params->seed_xor_lanes);
    checker_xor_vec = _mm256_loadu_si256((const __m256i*)(const void*)params->checker_xor_lanes);
    v8_vec = _mm256_loadu_si256((const __m256i*)(const void*)params->v8_lanes);

    u_vec = _mm256_and_si256(u_vec, mask_ff);
    hash_vec = _mm256_xor_si256(_mm256_slli_epi32(u_vec, 8), seed_xor_vec);
    hash_vec = vn_avx2_hash32_u32x8(hash_vec);

    r_vec = _mm256_and_si256(hash_vec, mask_ff);
    g_vec = _mm256_and_si256(_mm256_srli_epi32(hash_vec, 8), mask_ff);
    b_vec = _mm256_and_si256(_mm256_srli_epi32(hash_vec, 16), mask_ff);

    checker_mask = _mm256_and_si256(_mm256_xor_si256(_mm256_srli_epi32(u_vec, 5), checker_xor_vec), one_vec);
    checker_mask = _mm256_cmpeq_epi32(checker_mask, one_vec);
    alt_mask = _mm256_and_si256(_mm256_add_epi32(u_vec, v8_vec), alt_bit);
    alt_mask = _mm256_cmpeq_epi32(alt_mask, alt_bit);
    alt_mask = _mm256_andnot_si256(checker_mask, alt_mask);

    r_vec = _mm256_add_epi32(r_vec, _mm256_and_si256(checker_mask, _mm256_set1_epi32(24)));
    g_vec = _mm256_add_epi32(g_vec, _mm256_and_si256(checker_mask, _mm256_set1_epi32(24)));
    b_vec = _mm256_add_epi32(b_vec, _mm256_and_si256(checker_mask, _mm256_set1_epi32(24)));

    r_vec = _mm256_sub_epi32(r_vec, _mm256_and_si256(alt_mask, _mm256_set1_epi32(16)));
    g_vec = _mm256_sub_epi32(g_vec, _mm256_and_si256(alt_mask, _mm256_set1_epi32(10)));
    b_vec = _mm256_sub_epi32(b_vec, _mm256_and_si256(alt_mask, _mm256_set1_epi32(16)));

    r_vec = _mm256_max_epi32(r_vec, zero_vec);
    g_vec = _mm256_max_epi32(g_vec, zero_vec);
    b_vec = _mm256_max_epi32(b_vec, zero_vec);
    r_vec = _mm256_min_epi32(r_vec, max_vec);
    g_vec = _mm256_min_epi32(g_vec, max_vec);
    b_vec = _mm256_min_epi32(b_vec, max_vec);

    r_vec = _mm256_add_epi32(r_vec, _mm256_loadu_si256((const __m256i*)(const void*)params->base_r_lanes));
    g_vec = _mm256_add_epi32(g_vec, _mm256_loadu_si256((const __m256i*)(const void*)params->base_g_lanes));
    b_vec = _mm256_add_epi32(b_vec, _mm256_loadu_si256((const __m256i*)(const void*)params->base_b_lanes));

    if (params->op == VN_OP_TEXT) {
        __m256i y_vec;

        y_vec = _mm256_mullo_epi32(r_vec, _mm256_set1_epi32(54));
        y_vec = _mm256_add_epi32(y_vec,
                                 _mm256_mullo_epi32(g_vec, _mm256_set1_epi32(183)));
        y_vec = _mm256_add_epi32(y_vec,
                                 _mm256_mullo_epi32(b_vec, _mm256_set1_epi32(19)));
        y_vec = _mm256_srli_epi32(y_vec, 8);
        r_vec = _mm256_add_epi32(y_vec, _mm256_set1_epi32(52));
        g_vec = _mm256_add_epi32(y_vec, _mm256_set1_epi32(44));
        b_vec = _mm256_add_epi32(y_vec,
                                 _mm256_loadu_si256((const __m256i*)(const void*)params->text_blue_bias_lanes));
    } else if (params->op == VN_OP_SPRITE) {
        b_vec = _mm256_add_epi32(b_vec,
                                 _mm256_loadu_si256((const __m256i*)(const void*)params->sprite_blue_bias_lanes));
    }

    r_vec = _mm256_max_epi32(r_vec, zero_vec);
    g_vec = _mm256_max_epi32(g_vec, zero_vec);
    b_vec = _mm256_max_epi32(b_vec, zero_vec);
    r_vec = _mm256_min_epi32(r_vec, max_vec);
    g_vec = _mm256_min_epi32(g_vec, max_vec);
    b_vec = _mm256_min_epi32(b_vec, max_vec);

    return _mm256_or_si256(alpha_mask,
                           _mm256_or_si256(_mm256_slli_epi32(r_vec, 16),
                                           _mm256_or_si256(_mm256_slli_epi32(g_vec, 8),
                                                           b_vec)));
}

VN_AVX2_TARGET_ATTR
static __m256i vn_avx2_sample_combine_u_lut8(const vn_u8* u_lut,
                                             const VN_AVX2TexturedRowParams* params) {
    return vn_avx2_sample_combine_chunk_u32x8(vn_avx2_load_u_lut8_u32x8(u_lut), params);
}

VN_AVX2_TARGET_ATTR
static __m256i vn_avx2_blend_rgb_chunk_u32x8(__m256i dst_vec,
                                             __m256i src_vec,
                                             vn_u8 alpha) {
    __m256i mask_rb;
    __m256i mask_g;
    __m256i bias_rb;
    __m256i bias_g;
    __m256i one_rb;
    __m256i one_g;
    __m256i alpha_vec;
    __m256i inv_vec;
    __m256i alpha_mask;
    __m256i src_rb;
    __m256i src_g;
    __m256i dst_rb;
    __m256i dst_g;
    __m256i rb;
    __m256i g;

    if (alpha >= 255u) {
        return src_vec;
    }
    if (alpha == 0u) {
        return dst_vec;
    }

    mask_rb = _mm256_set1_epi32((int)0x00FF00FFu);
    mask_g = _mm256_set1_epi32((int)0x0000FF00u);
    bias_rb = _mm256_set1_epi32((int)0x007F007Fu);
    bias_g = _mm256_set1_epi32((int)0x00007F00u);
    one_rb = _mm256_set1_epi32((int)0x00010001u);
    one_g = _mm256_set1_epi32((int)0x00000100u);
    alpha_vec = _mm256_set1_epi32((int)(vn_u32)alpha);
    inv_vec = _mm256_set1_epi32((int)(255u - (vn_u32)alpha));
    alpha_mask = _mm256_set1_epi32((int)0xFF000000u);
    src_rb = _mm256_and_si256(src_vec, mask_rb);
    src_g = _mm256_and_si256(src_vec, mask_g);
    dst_rb = _mm256_and_si256(dst_vec, mask_rb);
    dst_g = _mm256_and_si256(dst_vec, mask_g);

    rb = _mm256_add_epi32(_mm256_mullo_epi32(src_rb, alpha_vec),
                          _mm256_mullo_epi32(dst_rb, inv_vec));
    rb = _mm256_add_epi32(rb, bias_rb);
    rb = _mm256_add_epi32(rb,
                          _mm256_add_epi32(one_rb,
                                           _mm256_and_si256(_mm256_srli_epi32(rb, 8), mask_rb)));
    rb = _mm256_and_si256(_mm256_srli_epi32(rb, 8), mask_rb);

    g = _mm256_add_epi32(_mm256_mullo_epi32(src_g, alpha_vec),
                         _mm256_mullo_epi32(dst_g, inv_vec));
    g = _mm256_add_epi32(g, bias_g);
    g = _mm256_add_epi32(g,
                         _mm256_add_epi32(one_g,
                                          _mm256_and_si256(_mm256_srli_epi32(g, 8), mask_g)));
    g = _mm256_and_si256(_mm256_srli_epi32(g, 8), mask_g);

    return _mm256_or_si256(_mm256_or_si256(rb, g), alpha_mask);
}

VN_AVX2_TARGET_ATTR
static __m256i vn_avx2_gather_palette8(const vn_u8* u_lut, const vn_u32* palette) {
    __m256i idx32;

    idx32 = vn_avx2_load_u_lut8_u32x8(u_lut);
    return _mm256_i32gather_epi32((const int*)(const void*)palette, idx32, 4);
}

VN_AVX2_TARGET_ATTR
static void vn_avx2_sample_texels_palette_row(vn_u32* dst,
                                              const vn_u8* u_lut,
                                              vn_u32 count,
                                              const vn_u32* palette) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || palette == (const vn_u32*)0) {
        return;
    }

    i = 0u;
    while ((i + 8u) <= count) {
        __m256i src_vec;

        src_vec = vn_avx2_gather_palette8(u_lut + i, palette);
        _mm256_storeu_si256((__m256i*)(void*)(dst + i), src_vec);
        i += 8u;
    }
    while (i < count) {
        dst[i] = palette[(vn_u32)u_lut[i]];
        i += 1u;
    }
}

VN_AVX2_TARGET_ATTR
static void vn_avx2_sample_blend_texels_palette_row(vn_u32* dst,
                                                    const vn_u8* u_lut,
                                                    vn_u32 count,
                                                    const vn_u32* palette,
                                                    vn_u8 alpha) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || palette == (const vn_u32*)0 || alpha == 0u) {
        return;
    }
    if (alpha >= 255u) {
        vn_avx2_sample_texels_palette_row(dst, u_lut, count, palette);
        return;
    }

    i = 0u;
    while ((i + 8u) <= count) {
        __m256i src_vec;
        __m256i dst_vec;
        __m256i out_vec;

        src_vec = vn_avx2_gather_palette8(u_lut + i, palette);
        dst_vec = _mm256_loadu_si256((const __m256i*)(const void*)(dst + i));
        out_vec = vn_avx2_blend_rgb_chunk_u32x8(dst_vec, src_vec, alpha);
        _mm256_storeu_si256((__m256i*)(void*)(dst + i), out_vec);
        i += 8u;
    }
    while (i < count) {
        vn_u32 src;

        src = palette[(vn_u32)u_lut[i]];
        dst[i] = vn_avx2_blend_rgb_local(dst[i], src, alpha);
        i += 1u;
    }
}
#else
static void vn_avx2_sample_texels_palette_row(vn_u32* dst,
                                              const vn_u8* u_lut,
                                              vn_u32 count,
                                              const vn_u32* palette) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || palette == (const vn_u32*)0) {
        return;
    }

    for (i = 0u; i < count; ++i) {
        dst[i] = palette[(vn_u32)u_lut[i]];
    }
}

static void vn_avx2_sample_blend_texels_palette_row(vn_u32* dst,
                                                    const vn_u8* u_lut,
                                                    vn_u32 count,
                                                    const vn_u32* palette,
                                                    vn_u8 alpha) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || palette == (const vn_u32*)0 || alpha == 0u) {
        return;
    }
    if (alpha >= 255u) {
        vn_avx2_sample_texels_palette_row(dst, u_lut, count, palette);
        return;
    }

    for (i = 0u; i < count; ++i) {
        vn_u32 src;

        src = palette[(vn_u32)u_lut[i]];
        dst[i] = vn_avx2_blend_rgb_local(dst[i], src, alpha);
    }
}
#endif

VN_AVX2_TARGET_ATTR
static void vn_avx2_sample_texels_row(vn_u32* dst,
                                      const vn_u8* u_lut,
                                      vn_u32 count,
                                      const VN_AVX2TexturedRowParams* params) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || params == (const VN_AVX2TexturedRowParams*)0) {
        return;
    }

#if VN_AVX2_IMPL_AVAILABLE
    i = 0u;
    while ((i + 8u) <= count) {
        __m256i src_vec;

        src_vec = vn_avx2_sample_combine_u_lut8(u_lut + i, params);
        _mm256_storeu_si256((__m256i*)(void*)(dst + i), src_vec);
        i += 8u;
    }
#else
    i = 0u;
#endif
    while (i < count) {
        dst[i] = vn_avx2_sample_combine_texel((vn_u32)u_lut[i], params);
        i += 1u;
    }
}

VN_AVX2_TARGET_ATTR
static void vn_avx2_sample_blend_texels_row(vn_u32* dst,
                                            const vn_u8* u_lut,
                                            vn_u32 count,
                                            const VN_AVX2TexturedRowParams* params,
                                            vn_u8 alpha) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || params == (const VN_AVX2TexturedRowParams*)0) {
        return;
    }
    if (alpha == 0u) {
        return;
    }
    if (alpha >= 255u) {
        vn_avx2_sample_texels_row(dst, u_lut, count, params);
        return;
    }

#if VN_AVX2_IMPL_AVAILABLE
    i = 0u;
    while ((i + 8u) <= count) {
        __m256i src_vec;
        __m256i dst_vec;
        __m256i out_vec;

        src_vec = vn_avx2_sample_combine_u_lut8(u_lut + i, params);
        dst_vec = _mm256_loadu_si256((const __m256i*)(const void*)(dst + i));
        out_vec = vn_avx2_blend_rgb_chunk_u32x8(dst_vec, src_vec, alpha);
        _mm256_storeu_si256((__m256i*)(void*)(dst + i), out_vec);
        i += 8u;
    }
#else
    i = 0u;
#endif
    while (i < count) {
        vn_u32 src;

        src = vn_avx2_sample_combine_texel((vn_u32)u_lut[i], params);
        dst[i] = vn_avx2_blend_rgb_local(dst[i], src, alpha);
        i += 1u;
    }
}

static int vn_avx2_should_use_row_palette(vn_u32 vis_w,
                                           vn_u32 vis_h) {
    (void)vis_h;
    return (vis_w > 256u ? VN_TRUE : VN_FALSE);
}

void vn_avx2_draw_textured_rect_clipped(const VNRenderOp* op,
                                               const VNRenderRect* clip_rect) {
    vn_u32 x0;
    vn_u32 y0;
    vn_u32 x1;
    vn_u32 y1;
    vn_u32 vis_w;
    vn_u32 vis_h;
    int local_x_start_i;
    int local_y_start_i;
    vn_u32 local_x_start;
    vn_u32 local_y_start;
    vn_u32 row_rel;
    int use_row_palette;
    int have_row_palette;
    vn_u32 cached_v8;
    vn_u32 row_palette[256];
    VN_AVX2TexturedRowParams params;

    if (op == (const VNRenderOp*)0 || g_avx2_framebuffer == (vn_u32*)0) {
        return;
    }
    if (op->alpha == 0u) {
        return;
    }
    if (vn_avx2_clip_rect_region(op->x, op->y, op->w, op->h, clip_rect, &x0, &y0, &x1, &y1) == VN_FALSE) {
        return;
    }

    vis_w = x1 - x0;
    vis_h = y1 - y0;
    if (vis_w == 0u || vis_h == 0u) {
        return;
    }
    if (g_avx2_u_lut == (vn_u8*)0 || g_avx2_v_lut == (vn_u8*)0) {
        return;
    }
    if (vis_w > g_avx2_u_lut_cap || vis_h > g_avx2_v_lut_cap) {
        return;
    }

    local_x_start_i = (int)x0 - (int)op->x;
    local_y_start_i = (int)y0 - (int)op->y;
    if (local_x_start_i < 0 || local_y_start_i < 0) {
        return;
    }
    local_x_start = (vn_u32)local_x_start_i;
    local_y_start = (vn_u32)local_y_start_i;

    vn_avx2_build_coord_lut(g_avx2_u_lut, vis_w, local_x_start, op->w);
    vn_avx2_build_coord_lut(g_avx2_v_lut, vis_h, local_y_start, op->h);

    use_row_palette = vn_avx2_should_use_row_palette(vis_w, vis_h);
    have_row_palette = VN_FALSE;
    cached_v8 = 0u;

    for (row_rel = 0u; row_rel < vis_h; ++row_rel) {
        vn_u32 yy;
        vn_u32 row_off;
        vn_u32 v8;
        vn_u32* row_ptr;

        yy = y0 + row_rel;
        row_off = yy * g_avx2_stride;
        v8 = (vn_u32)g_avx2_v_lut[row_rel];
        row_ptr = g_avx2_framebuffer + row_off + x0;

        if (use_row_palette != VN_FALSE) {
            if (have_row_palette == VN_FALSE || v8 != cached_v8) {
                vn_avx2_init_textured_row_params(&params,
                                                 v8,
                                                 op->tex_id,
                                                 op->layer,
                                                 op->flags,
                                                 op->op);
                vn_avx2_build_textured_row_palette(row_palette, &params);
                cached_v8 = v8;
                have_row_palette = VN_TRUE;
            }

            if (op->alpha >= 255u) {
                vn_avx2_sample_texels_palette_row(row_ptr,
                                                  g_avx2_u_lut,
                                                  vis_w,
                                                  row_palette);
            } else {
                vn_avx2_sample_blend_texels_palette_row(row_ptr,
                                                        g_avx2_u_lut,
                                                        vis_w,
                                                        row_palette,
                                                        op->alpha);
            }
            continue;
        }

        vn_avx2_init_textured_row_params(&params,
                                         v8,
                                         op->tex_id,
                                         op->layer,
                                         op->flags,
                                         op->op);

        if (op->alpha >= 255u) {
            vn_avx2_sample_texels_row(row_ptr,
                                      g_avx2_u_lut,
                                      vis_w,
                                      &params);
        } else {
            vn_avx2_sample_blend_texels_row(row_ptr,
                                            g_avx2_u_lut,
                                            vis_w,
                                            &params,
                                            op->alpha);
        }
    }
}




void vn_avx2_draw_textured_rect(const VNRenderOp* op) {
    vn_avx2_draw_textured_rect_clipped(op, (const VNRenderRect*)0);
}

