#include <stdlib.h>
#include <string.h>

#include "vn_backend.h"
#include "vn_renderer.h"
#include "vn_error.h"

#include "../common/pixel_pipeline.h"
#include "../../core/build_config.h"

#if VN_AVX2_GNU_STYLE_IMPL
#include <immintrin.h>
#define VN_AVX2_IMPL_AVAILABLE 1
#define VN_AVX2_GNU_IMPL 1
#elif VN_AVX2_MSVC_STYLE_IMPL
#include <intrin.h>
#include <immintrin.h>
#define VN_AVX2_IMPL_AVAILABLE 1
#define VN_AVX2_MSVC_IMPL 1
#else
#define VN_AVX2_IMPL_AVAILABLE 0
#endif

static RendererConfig g_avx2_cfg;
static vn_u32* g_avx2_framebuffer = (vn_u32*)0;
static vn_u32 g_avx2_stride = 0u;
static vn_u32 g_avx2_height = 0u;
static vn_u32 g_avx2_pixels = 0u;
static vn_u8* g_avx2_u_lut = (vn_u8*)0;
static vn_u8* g_avx2_v_lut = (vn_u8*)0;
static vn_u32 g_avx2_u_lut_cap = 0u;
static vn_u32 g_avx2_v_lut_cap = 0u;
static int g_avx2_ready = VN_FALSE;

static vn_u32 vn_avx2_blend_rgb_local(vn_u32 dst, vn_u32 src, vn_u8 alpha);

static int vn_avx2_clip_rect_region(vn_i16 x,
                                    vn_i16 y,
                                    vn_u16 w,
                                    vn_u16 h,
                                    const VNRenderRect* clip_rect,
                                    vn_u32* out_x0,
                                    vn_u32* out_y0,
                                    vn_u32* out_x1,
                                    vn_u32* out_y1) {
    int x0;
    int y0;
    int x1;
    int y1;
    int clip_x0;
    int clip_y0;
    int clip_x1;
    int clip_y1;

    if (out_x0 == (vn_u32*)0 || out_y0 == (vn_u32*)0 || out_x1 == (vn_u32*)0 || out_y1 == (vn_u32*)0) {
        return VN_FALSE;
    }
    if (g_avx2_stride == 0u || g_avx2_height == 0u) {
        return VN_FALSE;
    }

    x0 = (int)x;
    y0 = (int)y;
    x1 = x0 + (int)w;
    y1 = y0 + (int)h;

    if (clip_rect != (const VNRenderRect*)0) {
        if (clip_rect->w == 0u || clip_rect->h == 0u) {
            return VN_FALSE;
        }
        clip_x0 = (int)clip_rect->x;
        clip_y0 = (int)clip_rect->y;
        clip_x1 = clip_x0 + (int)clip_rect->w;
        clip_y1 = clip_y0 + (int)clip_rect->h;
        if (x0 < clip_x0) {
            x0 = clip_x0;
        }
        if (y0 < clip_y0) {
            y0 = clip_y0;
        }
        if (x1 > clip_x1) {
            x1 = clip_x1;
        }
        if (y1 > clip_y1) {
            y1 = clip_y1;
        }
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > (int)g_avx2_stride) {
        x1 = (int)g_avx2_stride;
    }
    if (y1 > (int)g_avx2_height) {
        y1 = (int)g_avx2_height;
    }

    if (x0 >= x1 || y0 >= y1) {
        return VN_FALSE;
    }

    *out_x0 = (vn_u32)x0;
    *out_y0 = (vn_u32)y0;
    *out_x1 = (vn_u32)x1;
    *out_y1 = (vn_u32)y1;
    return VN_TRUE;
}

#if defined(VN_AVX2_GNU_IMPL)
#define VN_AVX2_TARGET_ATTR __attribute__((target("avx2")))
static int vn_avx2_runtime_supported(void) {
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") ? VN_TRUE : VN_FALSE;
}
#elif defined(VN_AVX2_MSVC_IMPL)
#define VN_AVX2_TARGET_ATTR
static int vn_avx2_runtime_supported(void) {
    int cpu_info[4];

    __cpuid(cpu_info, 0);
    if (cpu_info[0] < 7) {
        return VN_FALSE;
    }

    __cpuid(cpu_info, 1);
    if ((cpu_info[2] & (1 << 27)) == 0 ||
        (cpu_info[2] & (1 << 28)) == 0) {
        return VN_FALSE;
    }
    if ((_xgetbv(0) & 0x6u) != 0x6u) {
        return VN_FALSE;
    }

    __cpuidex(cpu_info, 7, 0);
    if ((cpu_info[1] & (1 << 5)) == 0) {
        return VN_FALSE;
    }
    return VN_TRUE;
}
#else
#define VN_AVX2_TARGET_ATTR
static int vn_avx2_runtime_supported(void) {
    return VN_FALSE;
}
#endif

#if VN_AVX2_IMPL_AVAILABLE
VN_AVX2_TARGET_ATTR
static void vn_avx2_fill_u32(vn_u32* dst, vn_u32 count, vn_u32 value) {
    __m256i vec;
    size_t misaligned_bytes;
    vn_u32 prefix;
    vn_u32 i;

    if (dst == (vn_u32*)0 || count == 0u) {
        return;
    }

    vec = _mm256_set1_epi32((int)value);
    i = 0u;
    misaligned_bytes = ((size_t)(void*)dst) & 31u;
    if (misaligned_bytes != 0u) {
        prefix = (vn_u32)((32u - misaligned_bytes) / sizeof(vn_u32));
        if (prefix > count) {
            prefix = count;
        }
        while (i < prefix) {
            dst[i] = value;
            i += 1u;
        }
    }
    while ((i + 8u) <= count) {
        _mm256_store_si256((__m256i*)(void*)(dst + i), vec);
        i += 8u;
    }
    while (i < count) {
        dst[i] = value;
        i += 1u;
    }
}

VN_AVX2_TARGET_ATTR
static void vn_avx2_blend_uniform_u32(vn_u32* dst, vn_u32 count, vn_u32 src, vn_u8 alpha) {
    __m256i mask_rb;
    __m256i mask_g;
    __m256i bias_rb;
    __m256i bias_g;
    __m256i one_rb;
    __m256i one_g;
    __m256i alpha_vec;
    __m256i inv_vec;
    __m256i src_rb;
    __m256i src_g;
    __m256i alpha_mask;
    size_t misaligned_bytes;
    vn_u32 prefix;
    vn_u32 i;

    if (dst == (vn_u32*)0 || count == 0u || alpha == 0u) {
        return;
    }
    if (alpha >= 255u) {
        vn_avx2_fill_u32(dst, count, src);
        return;
    }

    mask_rb = _mm256_set1_epi32((int)0x00FF00FFu);
    mask_g = _mm256_set1_epi32((int)0x0000FF00u);
    bias_rb = _mm256_set1_epi32((int)0x007F007Fu);
    bias_g = _mm256_set1_epi32((int)0x00007F00u);
    one_rb = _mm256_set1_epi32((int)0x00010001u);
    one_g = _mm256_set1_epi32((int)0x00000100u);
    alpha_vec = _mm256_set1_epi32((int)(vn_u32)alpha);
    inv_vec = _mm256_set1_epi32((int)(255u - (vn_u32)alpha));
    src_rb = _mm256_and_si256(_mm256_set1_epi32((int)src), mask_rb);
    src_g = _mm256_and_si256(_mm256_set1_epi32((int)src), mask_g);
    alpha_mask = _mm256_set1_epi32((int)0xFF000000u);
    i = 0u;
    misaligned_bytes = ((size_t)(void*)dst) & 31u;
    if (misaligned_bytes != 0u) {
        prefix = (vn_u32)((32u - misaligned_bytes) / sizeof(vn_u32));
        if (prefix > count) {
            prefix = count;
        }
        while (i < prefix) {
            dst[i] = vn_avx2_blend_rgb_local(dst[i], src, alpha);
            i += 1u;
        }
    }
    while ((i + 8u) <= count) {
        __m256i dst_vec;
        __m256i dst_rb;
        __m256i dst_g;
        __m256i rb;
        __m256i g;
        __m256i out_vec;

        dst_vec = _mm256_load_si256((const __m256i*)(const void*)(dst + i));
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

        out_vec = _mm256_or_si256(_mm256_or_si256(rb, g), alpha_mask);
        _mm256_store_si256((__m256i*)(void*)(dst + i), out_vec);
        i += 8u;
    }
    while (i < count) {
        dst[i] = vn_avx2_blend_rgb_local(dst[i], src, alpha);
        i += 1u;
    }
}
#else
static void vn_avx2_fill_u32(vn_u32* dst, vn_u32 count, vn_u32 value) {
    vn_u32 i;

    if (dst == (vn_u32*)0) {
        return;
    }
    i = 0u;
    while (i < count) {
        dst[i] = value;
        i += 1u;
    }
}

static void vn_avx2_blend_uniform_u32(vn_u32* dst, vn_u32 count, vn_u32 src, vn_u8 alpha) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || alpha == 0u) {
        return;
    }
    if (alpha >= 255u) {
        vn_avx2_fill_u32(dst, count, src);
        return;
    }

    i = 0u;
    while (i < count) {
        dst[i] = vn_avx2_blend_rgb_local(dst[i], src, alpha);
        i += 1u;
    }
}
#endif

static void vn_avx2_fill_rect_uniform_clipped(vn_i16 x,
                                              vn_i16 y,
                                              vn_u16 w,
                                              vn_u16 h,
                                              vn_u32 color,
                                              vn_u8 alpha,
                                              const VNRenderRect* clip_rect) {
    vn_u32 x0;
    vn_u32 y0;
    vn_u32 x1;
    vn_u32 y1;
    vn_u32 yy;

    if (g_avx2_framebuffer == (vn_u32*)0) {
        return;
    }
    if (alpha == 0u) {
        return;
    }
    if (vn_avx2_clip_rect_region(x, y, w, h, clip_rect, &x0, &y0, &x1, &y1) == VN_FALSE) {
        return;
    }

    for (yy = y0; yy < y1; ++yy) {
        vn_u32* row_ptr;

        row_ptr = g_avx2_framebuffer + yy * g_avx2_stride + x0;
        if (alpha >= 255u) {
            vn_avx2_fill_u32(row_ptr, x1 - x0, color);
        } else {
            vn_avx2_blend_uniform_u32(row_ptr, x1 - x0, color, alpha);
        }
    }
}


static void vn_avx2_clear_rect(vn_u8 gray, const VNRenderRect* clip_rect) {
    if (clip_rect == (const VNRenderRect*)0) {
        if (g_avx2_framebuffer == (vn_u32*)0 || g_avx2_pixels == 0u) {
            return;
        }
        vn_avx2_fill_u32(g_avx2_framebuffer, g_avx2_pixels, vn_pp_make_gray(gray));
        return;
    }
    vn_avx2_fill_rect_uniform_clipped(0,
                                      0,
                                      g_avx2_cfg.width,
                                      g_avx2_cfg.height,
                                      vn_pp_make_gray(gray),
                                      255u,
                                      clip_rect);
}

static void vn_avx2_fill_rect_uniform(vn_i16 x, vn_i16 y, vn_u16 w, vn_u16 h, vn_u32 color, vn_u8 alpha) {
    vn_avx2_fill_rect_uniform_clipped(x, y, w, h, color, alpha, (const VNRenderRect*)0);
}

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

static vn_u32 vn_avx2_blend_rgb_local(vn_u32 dst, vn_u32 src, vn_u8 alpha) {
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
    seed_xor_vec = _mm256_set1_epi32((int)params->seed_xor);
    checker_xor_vec = _mm256_set1_epi32((int)params->checker_xor);
    v8_vec = _mm256_set1_epi32((int)params->v8);

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

    r_vec = _mm256_add_epi32(r_vec, _mm256_set1_epi32(params->base_r));
    g_vec = _mm256_add_epi32(g_vec, _mm256_set1_epi32(params->base_g));
    b_vec = _mm256_add_epi32(b_vec, _mm256_set1_epi32(params->base_b));

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
                                 _mm256_set1_epi32(params->text_blue_bias));
    } else if (params->op == VN_OP_SPRITE) {
        b_vec = _mm256_add_epi32(b_vec,
                                 _mm256_set1_epi32(params->sprite_blue_bias));
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

static void vn_avx2_draw_textured_rect_clipped(const VNRenderOp* op,
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




static void vn_avx2_draw_textured_rect(const VNRenderOp* op) {
    vn_avx2_draw_textured_rect_clipped(op, (const VNRenderRect*)0);
}

static int avx2_init(const RendererConfig* cfg) {
    vn_u32 pixels;
    size_t u_lut_bytes;
    size_t v_lut_bytes;

    if (cfg == (const RendererConfig*)0 || cfg->width == 0u || cfg->height == 0u) {
        return VN_E_INVALID_ARG;
    }
    if (vn_avx2_runtime_supported() == VN_FALSE) {
        return VN_E_UNSUPPORTED;
    }

    pixels = (vn_u32)cfg->width * (vn_u32)cfg->height;
    if ((cfg->height != 0u) && (pixels / (vn_u32)cfg->height != (vn_u32)cfg->width)) {
        return VN_E_FORMAT;
    }

    g_avx2_framebuffer = (vn_u32*)malloc((size_t)pixels * sizeof(vn_u32));
    if (g_avx2_framebuffer == (vn_u32*)0) {
        return VN_E_NOMEM;
    }
    u_lut_bytes = (size_t)cfg->width * sizeof(vn_u8);
    v_lut_bytes = (size_t)cfg->height * sizeof(vn_u8);
    g_avx2_u_lut = (vn_u8*)malloc(u_lut_bytes);
    g_avx2_v_lut = (vn_u8*)malloc(v_lut_bytes);
    if (g_avx2_u_lut == (vn_u8*)0 || g_avx2_v_lut == (vn_u8*)0) {
        if (g_avx2_u_lut != (vn_u8*)0) {
            free(g_avx2_u_lut);
        }
        if (g_avx2_v_lut != (vn_u8*)0) {
            free(g_avx2_v_lut);
        }
        g_avx2_u_lut = (vn_u8*)0;
        g_avx2_v_lut = (vn_u8*)0;
        free(g_avx2_framebuffer);
        g_avx2_framebuffer = (vn_u32*)0;
        return VN_E_NOMEM;
    }
    (void)memset(g_avx2_framebuffer, 0, (size_t)pixels * sizeof(vn_u32));

    g_avx2_cfg = *cfg;
    g_avx2_stride = (vn_u32)cfg->width;
    g_avx2_height = (vn_u32)cfg->height;
    g_avx2_pixels = pixels;
    g_avx2_u_lut_cap = (vn_u32)cfg->width;
    g_avx2_v_lut_cap = (vn_u32)cfg->height;
    g_avx2_ready = VN_TRUE;
    return VN_OK;
}

static void avx2_shutdown(void) {
    if (g_avx2_framebuffer != (vn_u32*)0) {
        free(g_avx2_framebuffer);
    }
    if (g_avx2_u_lut != (vn_u8*)0) {
        free(g_avx2_u_lut);
    }
    if (g_avx2_v_lut != (vn_u8*)0) {
        free(g_avx2_v_lut);
    }
    g_avx2_framebuffer = (vn_u32*)0;
    g_avx2_u_lut = (vn_u8*)0;
    g_avx2_v_lut = (vn_u8*)0;
    g_avx2_stride = 0u;
    g_avx2_height = 0u;
    g_avx2_pixels = 0u;
    g_avx2_u_lut_cap = 0u;
    g_avx2_v_lut_cap = 0u;
    g_avx2_cfg.width = 0u;
    g_avx2_cfg.height = 0u;
    g_avx2_cfg.flags = 0u;
    g_avx2_ready = VN_FALSE;
}

static void avx2_begin_frame(void) {
}

static int avx2_submit_ops(const VNRenderOp* ops, vn_u32 op_count) {
    vn_u32 i;

    if (g_avx2_ready == VN_FALSE) {
        return VN_E_RENDER_STATE;
    }
    if (ops == (const VNRenderOp*)0 && op_count != 0u) {
        return VN_E_INVALID_ARG;
    }

    for (i = 0u; i < op_count; ++i) {
        const VNRenderOp* op;
        op = &ops[i];
        if (op->op == VN_OP_CLEAR) {
            vn_avx2_clear_rect(op->alpha, (const VNRenderRect*)0);
        } else if (op->op == VN_OP_SPRITE || op->op == VN_OP_TEXT) {
            vn_avx2_draw_textured_rect(op);
        } else if (op->op == VN_OP_FADE) {
            vn_avx2_fill_rect_uniform(0, 0, g_avx2_cfg.width, g_avx2_cfg.height, 0xFF000000u, op->alpha);
        } else {
            return VN_E_FORMAT;
        }
    }
    return VN_OK;
}

static int avx2_submit_ops_dirty(const VNRenderOp* ops,
                                 vn_u32 op_count,
                                 const VNRenderDirtySubmit* dirty_submit) {
    const VNRenderOp* clear_op;
    vn_u32 rect_index;

    if (g_avx2_ready == VN_FALSE) {
        return VN_E_RENDER_STATE;
    }
    if (dirty_submit == (const VNRenderDirtySubmit*)0) {
        return VN_E_INVALID_ARG;
    }
    if (ops == (const VNRenderOp*)0 && op_count != 0u) {
        return VN_E_INVALID_ARG;
    }
    if (dirty_submit->rect_count != 0u && dirty_submit->rects == (const VNRenderRect*)0) {
        return VN_E_INVALID_ARG;
    }
    if (dirty_submit->width != g_avx2_cfg.width || dirty_submit->height != g_avx2_cfg.height) {
        return VN_E_INVALID_ARG;
    }
    if (dirty_submit->full_redraw != 0u || op_count == 0u) {
        return avx2_submit_ops(ops, op_count);
    }
    if (dirty_submit->rect_count == 0u) {
        return VN_OK;
    }
    if (ops[0].op != VN_OP_CLEAR) {
        return avx2_submit_ops(ops, op_count);
    }

    clear_op = &ops[0];
    for (rect_index = 0u; rect_index < dirty_submit->rect_count; ++rect_index) {
        const VNRenderRect* clip_rect;
        vn_u32 i;

        clip_rect = &dirty_submit->rects[rect_index];
        vn_avx2_clear_rect(clear_op->alpha, clip_rect);
        for (i = 1u; i < op_count; ++i) {
            const VNRenderOp* op;
            op = &ops[i];
            if (op->op == VN_OP_SPRITE || op->op == VN_OP_TEXT) {
                vn_avx2_draw_textured_rect_clipped(op, clip_rect);
            } else if (op->op == VN_OP_FADE) {
                vn_avx2_fill_rect_uniform_clipped(0,
                                                  0,
                                                  g_avx2_cfg.width,
                                                  g_avx2_cfg.height,
                                                  0xFF000000u,
                                                  op->alpha,
                                                  clip_rect);
            } else if (op->op != VN_OP_CLEAR) {
                return VN_E_FORMAT;
            }
        }
    }
    return VN_OK;
}

static void avx2_end_frame(void) {
}

static void avx2_query_caps(VNBackendCaps* out_caps) {
    if (out_caps == (VNBackendCaps*)0) {
        return;
    }
    out_caps->has_simd = 1u;
    out_caps->has_lut_blend = 0u;
    out_caps->has_tmem_cache = 0u;
}

static const VNRenderBackend g_avx2_backend = {
    "avx2",
    VN_ARCH_AVX2,
    avx2_init,
    avx2_shutdown,
    avx2_begin_frame,
    avx2_submit_ops,
    avx2_end_frame,
    avx2_query_caps,
    avx2_submit_ops_dirty
};

int vn_register_avx2_backend(void) {
    return vn_backend_register(&g_avx2_backend);
}

vn_u32 vn_avx2_backend_debug_frame_crc32(void) {
    if (g_avx2_ready == VN_FALSE) {
        return 0u;
    }
    return vn_pp_frame_crc32(g_avx2_framebuffer, g_avx2_pixels);
}

vn_u32 vn_avx2_backend_debug_copy_framebuffer(vn_u32* out_pixels, vn_u32 pixel_count) {
    if (g_avx2_ready == VN_FALSE || out_pixels == (vn_u32*)0 || pixel_count < g_avx2_pixels) {
        return 0u;
    }
    (void)memcpy(out_pixels, g_avx2_framebuffer, (size_t)g_avx2_pixels * sizeof(vn_u32));
    return g_avx2_pixels;
}
