#include <stdlib.h>
#include <string.h>

#include "vn_backend.h"
#include "vn_renderer.h"
#include "vn_error.h"

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#include <immintrin.h>
#define VN_AVX2_IMPL_AVAILABLE 1
#else
#define VN_AVX2_IMPL_AVAILABLE 0
#endif

static RendererConfig g_avx2_cfg;
static vn_u32* g_avx2_framebuffer = (vn_u32*)0;
static vn_u32 g_avx2_stride = 0u;
static vn_u32 g_avx2_height = 0u;
static vn_u32 g_avx2_pixels = 0u;
static int g_avx2_ready = VN_FALSE;

static vn_u32 vn_make_gray(vn_u8 gray) {
    return (vn_u32)(0xFF000000u | ((vn_u32)gray << 16) | ((vn_u32)gray << 8) | (vn_u32)gray);
}

static vn_u32 vn_make_color_from_tex(vn_u16 tex_id, vn_u8 layer_bias) {
    vn_u8 r;
    vn_u8 g;
    vn_u8 b;

    r = (vn_u8)((tex_id * 37u + layer_bias * 17u) & 0xFFu);
    g = (vn_u8)((tex_id * 73u + layer_bias * 29u + 64u) & 0xFFu);
    b = (vn_u8)((tex_id * 19u + layer_bias * 41u + 128u) & 0xFFu);
    return (vn_u32)(0xFF000000u | ((vn_u32)r << 16) | ((vn_u32)g << 8) | (vn_u32)b);
}

static vn_u32 vn_blend_rgb(vn_u32 dst, vn_u32 src, vn_u8 alpha) {
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

static int vn_clip_rect(vn_i16 x, vn_i16 y, vn_u16 w, vn_u16 h, vn_u32* out_x0, vn_u32* out_y0, vn_u32* out_x1, vn_u32* out_y1) {
    int x0;
    int y0;
    int x1;
    int y1;

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

#if VN_AVX2_IMPL_AVAILABLE
static int vn_avx2_runtime_supported(void) {
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") ? VN_TRUE : VN_FALSE;
}

__attribute__((target("avx2")))
static void vn_avx2_fill_u32(vn_u32* dst, vn_u32 count, vn_u32 value) {
    __m256i vec;
    vn_u32 i;

    vec = _mm256_set1_epi32((int)value);
    i = 0u;
    while ((i + 8u) <= count) {
        _mm256_storeu_si256((__m256i*)(void*)(dst + i), vec);
        i += 8u;
    }
    while (i < count) {
        dst[i] = value;
        i += 1u;
    }
}
#else
static int vn_avx2_runtime_supported(void) {
    return VN_FALSE;
}

static void vn_avx2_fill_u32(vn_u32* dst, vn_u32 count, vn_u32 value) {
    vn_u32 i;
    i = 0u;
    while (i < count) {
        dst[i] = value;
        i += 1u;
    }
}
#endif

static void vn_clear_frame(vn_u8 gray) {
    if (g_avx2_framebuffer == (vn_u32*)0 || g_avx2_pixels == 0u) {
        return;
    }
    vn_avx2_fill_u32(g_avx2_framebuffer, g_avx2_pixels, vn_make_gray(gray));
}

static void vn_fill_rect(vn_i16 x, vn_i16 y, vn_u16 w, vn_u16 h, vn_u32 color, vn_u8 alpha) {
    vn_u32 x0;
    vn_u32 y0;
    vn_u32 x1;
    vn_u32 y1;
    vn_u32 yy;

    if (g_avx2_framebuffer == (vn_u32*)0) {
        return;
    }
    if (vn_clip_rect(x, y, w, h, &x0, &y0, &x1, &y1) == VN_FALSE) {
        return;
    }

    if (alpha >= 255u) {
        for (yy = y0; yy < y1; ++yy) {
            vn_u32* row_ptr;
            row_ptr = g_avx2_framebuffer + yy * g_avx2_stride + x0;
            vn_avx2_fill_u32(row_ptr, x1 - x0, color);
        }
        return;
    }

    for (yy = y0; yy < y1; ++yy) {
        vn_u32 xx;
        vn_u32 row_off;
        row_off = yy * g_avx2_stride;
        for (xx = x0; xx < x1; ++xx) {
            vn_u32 idx;
            idx = row_off + xx;
            g_avx2_framebuffer[idx] = vn_blend_rgb(g_avx2_framebuffer[idx], color, alpha);
        }
    }
}

static int avx2_init(const RendererConfig* cfg) {
    vn_u32 pixels;

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
    (void)memset(g_avx2_framebuffer, 0, (size_t)pixels * sizeof(vn_u32));

    g_avx2_cfg = *cfg;
    g_avx2_stride = (vn_u32)cfg->width;
    g_avx2_height = (vn_u32)cfg->height;
    g_avx2_pixels = pixels;
    g_avx2_ready = VN_TRUE;
    return VN_OK;
}

static void avx2_shutdown(void) {
    if (g_avx2_framebuffer != (vn_u32*)0) {
        free(g_avx2_framebuffer);
    }
    g_avx2_framebuffer = (vn_u32*)0;
    g_avx2_stride = 0u;
    g_avx2_height = 0u;
    g_avx2_pixels = 0u;
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
            vn_clear_frame(op->alpha);
        } else if (op->op == VN_OP_SPRITE) {
            vn_u32 color;
            color = vn_make_color_from_tex(op->tex_id, (vn_u8)(op->layer * 11u + 40u));
            vn_fill_rect(op->x, op->y, op->w, op->h, color, op->alpha);
        } else if (op->op == VN_OP_TEXT) {
            vn_u32 color;
            color = vn_make_color_from_tex(op->tex_id, (vn_u8)(op->layer * 17u + 90u));
            vn_fill_rect(op->x, op->y, op->w, op->h, color, op->alpha);
        } else if (op->op == VN_OP_FADE) {
            vn_fill_rect(0, 0, g_avx2_cfg.width, g_avx2_cfg.height, 0xFF000000u, op->alpha);
        } else {
            return VN_E_FORMAT;
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
    avx2_query_caps
};

int vn_register_avx2_backend(void) {
    return vn_backend_register(&g_avx2_backend);
}
