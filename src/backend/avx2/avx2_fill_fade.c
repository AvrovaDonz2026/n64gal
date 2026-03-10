#include "avx2_internal.h"

static void vn_avx2_fill_u32_base(vn_u32* dst, vn_u32 count, vn_u32 value);
static void vn_avx2_blend_uniform_u32_base(vn_u32* dst, vn_u32 count, vn_u32 src, vn_u8 alpha);

#if VN_AVX2_ASM_FILL_AVAILABLE
VN_AVX2_TARGET_ATTR
static void vn_avx2_fill_u32_asm(vn_u32* dst, vn_u32 count, vn_u32 value) {
    size_t bulk_pixels;
    size_t remaining_pixels;
    vn_u32 i;
    vn_u32* ptr;

    if (dst == (vn_u32*)0 || count == 0u) {
        return;
    }

    bulk_pixels = (size_t)(count & ~7u);
    ptr = dst;
    if (bulk_pixels != 0u) {
        remaining_pixels = bulk_pixels;
        __asm__ __volatile__(
            "vbroadcastss %[fill], %%ymm0\n\t"
            "1:\n\t"
            "vmovdqu %%ymm0, (%[ptr])\n\t"
            "addq $32, %[ptr]\n\t"
            "subq $8, %[remaining]\n\t"
            "jnz 1b\n\t"
            "vzeroupper\n\t"
            : [ptr] "+r"(ptr), [remaining] "+r"(remaining_pixels)
            : [fill] "m"(value)
            : "cc", "memory", "ymm0");
    }

    i = (vn_u32)bulk_pixels;
    while (i < count) {
        dst[i] = value;
        i += 1u;
    }
}

static void vn_avx2_blend_uniform_u32_asm(vn_u32* dst, vn_u32 count, vn_u32 src, vn_u8 alpha) {
    vn_avx2_blend_uniform_u32_base(dst, count, src, alpha);
}
#else
static void vn_avx2_fill_u32_asm(vn_u32* dst, vn_u32 count, vn_u32 value) {
    vn_avx2_fill_u32_base(dst, count, value);
}

static void vn_avx2_blend_uniform_u32_asm(vn_u32* dst, vn_u32 count, vn_u32 src, vn_u8 alpha) {
    vn_avx2_blend_uniform_u32_base(dst, count, src, alpha);
}
#endif

static void vn_avx2_fill_u32(vn_u32* dst, vn_u32 count, vn_u32 value) {
    if (g_avx2_use_asm_fill != VN_FALSE) {
        vn_avx2_fill_u32_asm(dst, count, value);
        return;
    }
    vn_avx2_fill_u32_base(dst, count, value);
}

static void vn_avx2_blend_uniform_u32(vn_u32* dst, vn_u32 count, vn_u32 src, vn_u8 alpha) {
    if (g_avx2_use_asm_fill != VN_FALSE) {
        vn_avx2_blend_uniform_u32_asm(dst, count, src, alpha);
        return;
    }
    vn_avx2_blend_uniform_u32_base(dst, count, src, alpha);
}

#if VN_AVX2_IMPL_AVAILABLE
VN_AVX2_TARGET_ATTR
static void vn_avx2_fill_u32_base(vn_u32* dst, vn_u32 count, vn_u32 value) {
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
static void vn_avx2_blend_uniform_u32_base(vn_u32* dst, vn_u32 count, vn_u32 src, vn_u8 alpha) {
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
static void vn_avx2_fill_u32_base(vn_u32* dst, vn_u32 count, vn_u32 value) {
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

static void vn_avx2_blend_uniform_u32_base(vn_u32* dst, vn_u32 count, vn_u32 src, vn_u8 alpha) {
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

void vn_avx2_fill_rect_uniform_clipped(vn_i16 x,
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

void vn_avx2_clear_rect(vn_u8 gray, const VNRenderRect* clip_rect) {
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

void vn_avx2_fill_rect_uniform(vn_i16 x,
                               vn_i16 y,
                               vn_u16 w,
                               vn_u16 h,
                               vn_u32 color,
                               vn_u8 alpha) {
    vn_avx2_fill_rect_uniform_clipped(x, y, w, h, color, alpha, (const VNRenderRect*)0);
}
