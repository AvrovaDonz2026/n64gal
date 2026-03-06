#include <stdlib.h>
#include <string.h>

#include "vn_backend.h"
#include "vn_renderer.h"
#include "vn_error.h"

#include "../common/pixel_pipeline.h"
#include "../../core/build_config.h"

#if VN_RVV_IMPL_AVAILABLE
#include <riscv_vector.h>
#endif

static int vn_rvv_runtime_supported(void) {
#if VN_RVV_IMPL_AVAILABLE
    return VN_TRUE;
#else
    return VN_FALSE;
#endif
}

static RendererConfig g_rvv_cfg;
static vn_u32* g_rvv_framebuffer = (vn_u32*)0;
static vn_u32 g_rvv_stride = 0u;
static vn_u32 g_rvv_height = 0u;
static vn_u32 g_rvv_pixels = 0u;
static vn_u32* g_rvv_row_colors = (vn_u32*)0;
static vn_u32 g_rvv_row_colors_cap = 0u;
static vn_u32* g_rvv_u_lut = (vn_u32*)0;
static vn_u32* g_rvv_v_lut = (vn_u32*)0;
static vn_u32 g_rvv_u_lut_cap = 0u;
static vn_u32 g_rvv_v_lut_cap = 0u;
static int g_rvv_ready = VN_FALSE;

static int vn_rvv_clip_rect(vn_i16 x, vn_i16 y, vn_u16 w, vn_u16 h, vn_u32* out_x0, vn_u32* out_y0, vn_u32* out_x1, vn_u32* out_y1) {
    int x0;
    int y0;
    int x1;
    int y1;

    if (out_x0 == (vn_u32*)0 || out_y0 == (vn_u32*)0 || out_x1 == (vn_u32*)0 || out_y1 == (vn_u32*)0) {
        return VN_FALSE;
    }
    if (g_rvv_stride == 0u || g_rvv_height == 0u) {
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
    if (x1 > (int)g_rvv_stride) {
        x1 = (int)g_rvv_stride;
    }
    if (y1 > (int)g_rvv_height) {
        y1 = (int)g_rvv_height;
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

#if VN_RVV_IMPL_AVAILABLE
static void vn_rvv_fill_u32(vn_u32* dst, vn_u32 count, vn_u32 value) {
    vn_u32 i;

    i = 0u;
    while (i < count) {
        size_t vl;
        vuint32m4_t vec;

        vl = __riscv_vsetvl_e32m4((size_t)(count - i));
        vec = __riscv_vmv_v_x_u32m4(value, vl);
        __riscv_vse32_v_u32m4(dst + i, vec, vl);
        i += (vn_u32)vl;
    }
}

static vuint32m4_t vn_rvv_div255_round_u32(vuint32m4_t value, size_t vl) {
    vuint32m4_t bias;
    vuint32m4_t hi;

    bias = __riscv_vadd_vx_u32m4(value, 128u, vl);
    hi = __riscv_vsrl_vx_u32m4(bias, (size_t)8, vl);
    bias = __riscv_vadd_vv_u32m4(bias, hi, vl);
    return __riscv_vsrl_vx_u32m4(bias, (size_t)8, vl);
}

static vuint32m4_t vn_rvv_hash32(vuint32m4_t value, size_t vl) {
    value = __riscv_vxor_vv_u32m4(value, __riscv_vsrl_vx_u32m4(value, (size_t)16, vl), vl);
    value = __riscv_vmul_vx_u32m4(value, 0x7FEB352Du, vl);
    value = __riscv_vxor_vv_u32m4(value, __riscv_vsrl_vx_u32m4(value, (size_t)15, vl), vl);
    value = __riscv_vmul_vx_u32m4(value, 0x846CA68Bu, vl);
    value = __riscv_vxor_vv_u32m4(value, __riscv_vsrl_vx_u32m4(value, (size_t)16, vl), vl);
    return value;
}

static void vn_rvv_sample_texels_row(vn_u32* colors, const vn_u32* u_lut, vn_u32 count, vn_u32 v8, vn_u16 tex_id, vn_u8 layer, vn_u8 flags, vn_u8 op) {
    vn_u32 i;
    vn_u32 tex_mul;
    vn_u32 tex_hi;
    vn_u32 tex_lo3;
    vn_u32 v5;
    int layer_r;
    int layer_g;
    int layer_b;
    int text_blue_bias;

    tex_mul = ((vn_u32)tex_id * 2654435761u);
    tex_hi = ((vn_u32)tex_id << 16);
    tex_lo3 = ((vn_u32)tex_id & 7u);
    v8 &= 0xFFu;
    v5 = (v8 >> 5) & 0xFFu;
    layer_r = (int)layer * 7;
    layer_g = (int)layer * 5;
    layer_b = (int)layer * 3;
    text_blue_bias = 24 + (int)layer * 6;

    i = 0u;
    while (i < count) {
        size_t vl;
        vuint32m4_t u;
        vuint32m4_t seed;
        vuint32m4_t h;
        vuint32m4_t ur;
        vuint32m4_t ug;
        vuint32m4_t ub;
        vuint32m4_t checker;
        vuint32m4_t alt;
        vbool8_t mask_checker;
        vbool8_t mask_alt;
        vint32m4_t r;
        vint32m4_t g;
        vint32m4_t b;
        vint32m4_t r_dark;
        vint32m4_t g_dark;
        vint32m4_t b_dark;
        vint32m4_t r_bright;
        vint32m4_t g_bright;
        vint32m4_t b_bright;
        vuint32m4_t out;

        vl = __riscv_vsetvl_e32m4((size_t)(count - i));
        u = __riscv_vle32_v_u32m4(u_lut + i, vl);
        u = __riscv_vand_vx_u32m4(u, 0xFFu, vl);

        seed = __riscv_vsll_vx_u32m4(u, (size_t)8, vl);
        seed = __riscv_vxor_vx_u32m4(seed, tex_mul, vl);
        seed = __riscv_vxor_vx_u32m4(seed, v8, vl);
        seed = __riscv_vxor_vx_u32m4(seed, tex_hi, vl);
        h = vn_rvv_hash32(seed, vl);

        ur = __riscv_vand_vx_u32m4(h, 0xFFu, vl);
        ug = __riscv_vsrl_vx_u32m4(h, (size_t)8, vl);
        ug = __riscv_vand_vx_u32m4(ug, 0xFFu, vl);
        ub = __riscv_vsrl_vx_u32m4(h, (size_t)16, vl);
        ub = __riscv_vand_vx_u32m4(ub, 0xFFu, vl);

        checker = __riscv_vsrl_vx_u32m4(u, (size_t)5, vl);
        checker = __riscv_vxor_vx_u32m4(checker, v5, vl);
        checker = __riscv_vxor_vx_u32m4(checker, tex_lo3, vl);
        checker = __riscv_vand_vx_u32m4(checker, 1u, vl);
        mask_checker = __riscv_vmsne_vx_u32m4_b8(checker, 0u, vl);

        alt = __riscv_vadd_vx_u32m4(u, v8, vl);
        alt = __riscv_vand_vx_u32m4(alt, 0x20u, vl);
        mask_alt = __riscv_vmsne_vx_u32m4_b8(alt, 0u, vl);

        r = __riscv_vreinterpret_v_u32m4_i32m4(ur);
        g = __riscv_vreinterpret_v_u32m4_i32m4(ug);
        b = __riscv_vreinterpret_v_u32m4_i32m4(ub);

        r_dark = __riscv_vsub_vx_i32m4(r, 16, vl);
        g_dark = __riscv_vsub_vx_i32m4(g, 10, vl);
        b_dark = __riscv_vsub_vx_i32m4(b, 16, vl);
        r_bright = __riscv_vadd_vx_i32m4(r, 24, vl);
        g_bright = __riscv_vadd_vx_i32m4(g, 24, vl);
        b_bright = __riscv_vadd_vx_i32m4(b, 24, vl);

        r = __riscv_vmerge_vvm_i32m4(r, r_dark, mask_alt, vl);
        g = __riscv_vmerge_vvm_i32m4(g, g_dark, mask_alt, vl);
        b = __riscv_vmerge_vvm_i32m4(b, b_dark, mask_alt, vl);
        r = __riscv_vmerge_vvm_i32m4(r, r_bright, mask_checker, vl);
        g = __riscv_vmerge_vvm_i32m4(g, g_bright, mask_checker, vl);
        b = __riscv_vmerge_vvm_i32m4(b, b_bright, mask_checker, vl);

        r = __riscv_vmax_vx_i32m4(r, 0, vl);
        r = __riscv_vmin_vx_i32m4(r, 255, vl);
        g = __riscv_vmax_vx_i32m4(g, 0, vl);
        g = __riscv_vmin_vx_i32m4(g, 255, vl);
        b = __riscv_vmax_vx_i32m4(b, 0, vl);
        b = __riscv_vmin_vx_i32m4(b, 255, vl);

        r = __riscv_vadd_vx_i32m4(r, layer_r, vl);
        g = __riscv_vadd_vx_i32m4(g, layer_g, vl);
        b = __riscv_vadd_vx_i32m4(b, layer_b, vl);

        if ((flags & 1u) != 0u) {
            g = __riscv_vadd_vx_i32m4(g, 14, vl);
        }
        if ((flags & 2u) != 0u) {
            b = __riscv_vadd_vx_i32m4(b, 20, vl);
        }
        if ((flags & 4u) != 0u) {
            r = __riscv_vadd_vx_i32m4(r, 28, vl);
            g = __riscv_vsub_vx_i32m4(g, 12, vl);
        }
        if ((flags & 8u) != 0u) {
            r = __riscv_vadd_vx_i32m4(r, 12, vl);
            g = __riscv_vadd_vx_i32m4(g, 12, vl);
            b = __riscv_vsub_vx_i32m4(b, 8, vl);
        }

        if (op == VN_OP_TEXT) {
            vint32m4_t y;

            y = __riscv_vmul_vx_i32m4(r, 54, vl);
            y = __riscv_vadd_vv_i32m4(y, __riscv_vmul_vx_i32m4(g, 183, vl), vl);
            y = __riscv_vadd_vv_i32m4(y, __riscv_vmul_vx_i32m4(b, 19, vl), vl);
            y = __riscv_vsra_vx_i32m4(y, (size_t)8, vl);
            r = __riscv_vadd_vx_i32m4(y, 52, vl);
            g = __riscv_vadd_vx_i32m4(y, 44, vl);
            b = __riscv_vadd_vx_i32m4(y, text_blue_bias, vl);
        } else if (op == VN_OP_SPRITE) {
            b = __riscv_vadd_vx_i32m4(b, 10, vl);
        }

        r = __riscv_vmax_vx_i32m4(r, 0, vl);
        r = __riscv_vmin_vx_i32m4(r, 255, vl);
        g = __riscv_vmax_vx_i32m4(g, 0, vl);
        g = __riscv_vmin_vx_i32m4(g, 255, vl);
        b = __riscv_vmax_vx_i32m4(b, 0, vl);
        b = __riscv_vmin_vx_i32m4(b, 255, vl);

        ur = __riscv_vreinterpret_v_i32m4_u32m4(r);
        ug = __riscv_vreinterpret_v_i32m4_u32m4(g);
        ub = __riscv_vreinterpret_v_i32m4_u32m4(b);
        ur = __riscv_vsll_vx_u32m4(ur, (size_t)16, vl);
        ug = __riscv_vsll_vx_u32m4(ug, (size_t)8, vl);
        out = __riscv_vor_vv_u32m4(ur, ug, vl);
        out = __riscv_vor_vv_u32m4(out, ub, vl);
        out = __riscv_vadd_vx_u32m4(out, 0xFF000000u, vl);

        __riscv_vse32_v_u32m4(colors + i, out, vl);
        i += (vn_u32)vl;
    }
}

static void vn_rvv_blend_u32(vn_u32* dst, const vn_u32* src, vn_u32 count, vn_u8 alpha) {
    vn_u32 i;
    vn_u32 inv;

    inv = (vn_u32)(255u - alpha);
    i = 0u;
    while (i < count) {
        size_t vl;
        vuint32m4_t dst_px;
        vuint32m4_t src_px;
        vuint32m4_t dr;
        vuint32m4_t dg;
        vuint32m4_t db;
        vuint32m4_t sr;
        vuint32m4_t sg;
        vuint32m4_t sb;
        vuint32m4_t rr;
        vuint32m4_t rg;
        vuint32m4_t rb;
        vuint32m4_t out;

        vl = __riscv_vsetvl_e32m4((size_t)(count - i));
        dst_px = __riscv_vle32_v_u32m4(dst + i, vl);
        src_px = __riscv_vle32_v_u32m4(src + i, vl);

        dr = __riscv_vsrl_vx_u32m4(dst_px, (size_t)16, vl);
        dr = __riscv_vand_vx_u32m4(dr, 0xFFu, vl);
        dg = __riscv_vsrl_vx_u32m4(dst_px, (size_t)8, vl);
        dg = __riscv_vand_vx_u32m4(dg, 0xFFu, vl);
        db = __riscv_vand_vx_u32m4(dst_px, 0xFFu, vl);

        sr = __riscv_vsrl_vx_u32m4(src_px, (size_t)16, vl);
        sr = __riscv_vand_vx_u32m4(sr, 0xFFu, vl);
        sg = __riscv_vsrl_vx_u32m4(src_px, (size_t)8, vl);
        sg = __riscv_vand_vx_u32m4(sg, 0xFFu, vl);
        sb = __riscv_vand_vx_u32m4(src_px, 0xFFu, vl);

        rr = __riscv_vmul_vx_u32m4(dr, inv, vl);
        rr = __riscv_vadd_vv_u32m4(rr, __riscv_vmul_vx_u32m4(sr, (vn_u32)alpha, vl), vl);
        rr = vn_rvv_div255_round_u32(rr, vl);

        rg = __riscv_vmul_vx_u32m4(dg, inv, vl);
        rg = __riscv_vadd_vv_u32m4(rg, __riscv_vmul_vx_u32m4(sg, (vn_u32)alpha, vl), vl);
        rg = vn_rvv_div255_round_u32(rg, vl);

        rb = __riscv_vmul_vx_u32m4(db, inv, vl);
        rb = __riscv_vadd_vv_u32m4(rb, __riscv_vmul_vx_u32m4(sb, (vn_u32)alpha, vl), vl);
        rb = vn_rvv_div255_round_u32(rb, vl);

        rr = __riscv_vsll_vx_u32m4(rr, (size_t)16, vl);
        rg = __riscv_vsll_vx_u32m4(rg, (size_t)8, vl);
        out = __riscv_vor_vv_u32m4(rr, rg, vl);
        out = __riscv_vor_vv_u32m4(out, rb, vl);
        out = __riscv_vadd_vx_u32m4(out, 0xFF000000u, vl);

        __riscv_vse32_v_u32m4(dst + i, out, vl);
        i += (vn_u32)vl;
    }
}

static void vn_rvv_blend_u32_uniform(vn_u32* dst, vn_u32 count, vn_u32 color, vn_u8 alpha) {
    vn_u32 i;
    vn_u32 src_r_prod;
    vn_u32 src_g_prod;
    vn_u32 src_b_prod;
    vn_u32 inv;

    inv = (vn_u32)(255u - alpha);
    src_r_prod = ((color >> 16) & 0xFFu) * (vn_u32)alpha;
    src_g_prod = ((color >> 8) & 0xFFu) * (vn_u32)alpha;
    src_b_prod = (color & 0xFFu) * (vn_u32)alpha;

    i = 0u;
    while (i < count) {
        size_t vl;
        vuint32m4_t dst_px;
        vuint32m4_t dr;
        vuint32m4_t dg;
        vuint32m4_t db;
        vuint32m4_t rr;
        vuint32m4_t rg;
        vuint32m4_t rb;
        vuint32m4_t out;

        vl = __riscv_vsetvl_e32m4((size_t)(count - i));
        dst_px = __riscv_vle32_v_u32m4(dst + i, vl);

        dr = __riscv_vsrl_vx_u32m4(dst_px, (size_t)16, vl);
        dr = __riscv_vand_vx_u32m4(dr, 0xFFu, vl);
        dg = __riscv_vsrl_vx_u32m4(dst_px, (size_t)8, vl);
        dg = __riscv_vand_vx_u32m4(dg, 0xFFu, vl);
        db = __riscv_vand_vx_u32m4(dst_px, 0xFFu, vl);

        rr = __riscv_vmul_vx_u32m4(dr, inv, vl);
        rr = __riscv_vadd_vx_u32m4(rr, src_r_prod, vl);
        rr = vn_rvv_div255_round_u32(rr, vl);

        rg = __riscv_vmul_vx_u32m4(dg, inv, vl);
        rg = __riscv_vadd_vx_u32m4(rg, src_g_prod, vl);
        rg = vn_rvv_div255_round_u32(rg, vl);

        rb = __riscv_vmul_vx_u32m4(db, inv, vl);
        rb = __riscv_vadd_vx_u32m4(rb, src_b_prod, vl);
        rb = vn_rvv_div255_round_u32(rb, vl);

        rr = __riscv_vsll_vx_u32m4(rr, (size_t)16, vl);
        rg = __riscv_vsll_vx_u32m4(rg, (size_t)8, vl);
        out = __riscv_vor_vv_u32m4(rr, rg, vl);
        out = __riscv_vor_vv_u32m4(out, rb, vl);
        out = __riscv_vadd_vx_u32m4(out, 0xFF000000u, vl);

        __riscv_vse32_v_u32m4(dst + i, out, vl);
        i += (vn_u32)vl;
    }
}
#else
static void vn_rvv_fill_u32(vn_u32* dst, vn_u32 count, vn_u32 value) {
    vn_u32 i;

    i = 0u;
    while (i < count) {
        dst[i] = value;
        i += 1u;
    }
}

static void vn_rvv_sample_texels_row(vn_u32* colors, const vn_u32* u_lut, vn_u32 count, vn_u32 v8, vn_u16 tex_id, vn_u8 layer, vn_u8 flags, vn_u8 op) {
    vn_u32 i;

    i = 0u;
    while (i < count) {
        colors[i] = vn_pp_combine_texel(vn_pp_sample_texel(tex_id, u_lut[i], v8), layer, flags, op);
        i += 1u;
    }
}

static void vn_rvv_blend_u32(vn_u32* dst, const vn_u32* src, vn_u32 count, vn_u8 alpha) {
    vn_u32 i;

    i = 0u;
    while (i < count) {
        dst[i] = vn_pp_blend_rgb(dst[i], src[i], alpha);
        i += 1u;
    }
}

static void vn_rvv_blend_u32_uniform(vn_u32* dst, vn_u32 count, vn_u32 color, vn_u8 alpha) {
    vn_u32 i;

    i = 0u;
    while (i < count) {
        dst[i] = vn_pp_blend_rgb(dst[i], color, alpha);
        i += 1u;
    }
}
#endif

static void vn_rvv_clear_frame(vn_u8 gray) {
    if (g_rvv_framebuffer == (vn_u32*)0 || g_rvv_pixels == 0u) {
        return;
    }
    vn_rvv_fill_u32(g_rvv_framebuffer, g_rvv_pixels, vn_pp_make_gray(gray));
}

static void vn_rvv_fill_rect_uniform(vn_i16 x, vn_i16 y, vn_u16 w, vn_u16 h, vn_u32 color, vn_u8 alpha) {
    vn_u32 x0;
    vn_u32 y0;
    vn_u32 x1;
    vn_u32 y1;
    vn_u32 yy;

    if (g_rvv_framebuffer == (vn_u32*)0) {
        return;
    }
    if (vn_rvv_clip_rect(x, y, w, h, &x0, &y0, &x1, &y1) == VN_FALSE) {
        return;
    }

    if (alpha >= 255u) {
        for (yy = y0; yy < y1; ++yy) {
            vn_u32* row_ptr;
            row_ptr = g_rvv_framebuffer + yy * g_rvv_stride + x0;
            vn_rvv_fill_u32(row_ptr, x1 - x0, color);
        }
        return;
    }

    for (yy = y0; yy < y1; ++yy) {
        vn_u32* row_ptr;
        row_ptr = g_rvv_framebuffer + yy * g_rvv_stride + x0;
        /* Vectorize fade and translucent solid fills on the hot full-width path. */
        vn_rvv_blend_u32_uniform(row_ptr, x1 - x0, color, alpha);
    }
}

static void vn_rvv_build_coord_lut(vn_u32* out_lut, vn_u32 count, vn_u32 local_start, vn_u16 extent) {
    vn_u32 i;
    vn_u32 denom;
    vn_u32 value;

    if (out_lut == (vn_u32*)0 || count == 0u) {
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
        out_lut[i] = q;
        value += 255u;
    }
}

static void vn_rvv_draw_textured_rect(const VNRenderOp* op) {
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

    if (op == (const VNRenderOp*)0 || g_rvv_framebuffer == (vn_u32*)0) {
        return;
    }
    if (op->alpha == 0u) {
        return;
    }
    if (vn_rvv_clip_rect(op->x, op->y, op->w, op->h, &x0, &y0, &x1, &y1) == VN_FALSE) {
        return;
    }

    vis_w = x1 - x0;
    vis_h = y1 - y0;
    if (vis_w == 0u || vis_h == 0u) {
        return;
    }
    if (g_rvv_u_lut == (vn_u32*)0 || g_rvv_v_lut == (vn_u32*)0 || g_rvv_row_colors == (vn_u32*)0) {
        return;
    }
    if (vis_w > g_rvv_u_lut_cap || vis_h > g_rvv_v_lut_cap || vis_w > g_rvv_row_colors_cap) {
        return;
    }

    local_x_start_i = (int)x0 - (int)op->x;
    local_y_start_i = (int)y0 - (int)op->y;
    if (local_x_start_i < 0 || local_y_start_i < 0) {
        return;
    }
    local_x_start = (vn_u32)local_x_start_i;
    local_y_start = (vn_u32)local_y_start_i;

    vn_rvv_build_coord_lut(g_rvv_u_lut, vis_w, local_x_start, op->w);
    vn_rvv_build_coord_lut(g_rvv_v_lut, vis_h, local_y_start, op->h);

    for (row_rel = 0u; row_rel < vis_h; ++row_rel) {
        vn_u32 yy;
        vn_u32 row_off;
        vn_u32 v8;
        vn_u32* row_ptr;

        yy = y0 + row_rel;
        row_off = yy * g_rvv_stride;
        v8 = g_rvv_v_lut[row_rel];
        row_ptr = g_rvv_framebuffer + row_off + x0;

        if (op->alpha >= 255u) {
            /* Opaque textured rows can write the fused sample/combine result straight to the framebuffer. */
            vn_rvv_sample_texels_row(row_ptr,
                                     g_rvv_u_lut,
                                     vis_w,
                                     v8,
                                     op->tex_id,
                                     op->layer,
                                     op->flags,
                                     op->op);
        } else {
            /* Keep the translucent path batched so sample/combine and alpha blend still run per row. */
            vn_rvv_sample_texels_row(g_rvv_row_colors,
                                     g_rvv_u_lut,
                                     vis_w,
                                     v8,
                                     op->tex_id,
                                     op->layer,
                                     op->flags,
                                     op->op);
            /* Batch the framebuffer writeback so alpha blend and store run in RVV-width chunks. */
            vn_rvv_blend_u32(row_ptr, g_rvv_row_colors, vis_w, op->alpha);
        }
    }
}

static int rvv_init(const RendererConfig* cfg) {
    vn_u32 pixels;
    size_t row_colors_bytes;
    size_t u_lut_bytes;
    size_t v_lut_bytes;

    if (cfg == (const RendererConfig*)0 || cfg->width == 0u || cfg->height == 0u) {
        return VN_E_INVALID_ARG;
    }
    if (vn_rvv_runtime_supported() == VN_FALSE) {
        return VN_E_UNSUPPORTED;
    }

    pixels = (vn_u32)cfg->width * (vn_u32)cfg->height;
    if ((cfg->height != 0u) && (pixels / (vn_u32)cfg->height != (vn_u32)cfg->width)) {
        return VN_E_FORMAT;
    }

    g_rvv_framebuffer = (vn_u32*)malloc((size_t)pixels * sizeof(vn_u32));
    if (g_rvv_framebuffer == (vn_u32*)0) {
        return VN_E_NOMEM;
    }
    row_colors_bytes = (size_t)cfg->width * sizeof(vn_u32);
    u_lut_bytes = (size_t)cfg->width * sizeof(vn_u32);
    v_lut_bytes = (size_t)cfg->height * sizeof(vn_u32);
    g_rvv_row_colors = (vn_u32*)malloc(row_colors_bytes);
    g_rvv_u_lut = (vn_u32*)malloc(u_lut_bytes);
    g_rvv_v_lut = (vn_u32*)malloc(v_lut_bytes);
    if (g_rvv_row_colors == (vn_u32*)0 || g_rvv_u_lut == (vn_u32*)0 || g_rvv_v_lut == (vn_u32*)0) {
        if (g_rvv_row_colors != (vn_u32*)0) {
            free(g_rvv_row_colors);
        }
        if (g_rvv_u_lut != (vn_u32*)0) {
            free(g_rvv_u_lut);
        }
        if (g_rvv_v_lut != (vn_u32*)0) {
            free(g_rvv_v_lut);
        }
        g_rvv_row_colors = (vn_u32*)0;
        g_rvv_u_lut = (vn_u32*)0;
        g_rvv_v_lut = (vn_u32*)0;
        free(g_rvv_framebuffer);
        g_rvv_framebuffer = (vn_u32*)0;
        return VN_E_NOMEM;
    }
    (void)memset(g_rvv_framebuffer, 0, (size_t)pixels * sizeof(vn_u32));

    g_rvv_cfg = *cfg;
    g_rvv_stride = (vn_u32)cfg->width;
    g_rvv_height = (vn_u32)cfg->height;
    g_rvv_pixels = pixels;
    g_rvv_row_colors_cap = (vn_u32)cfg->width;
    g_rvv_u_lut_cap = (vn_u32)cfg->width;
    g_rvv_v_lut_cap = (vn_u32)cfg->height;
    g_rvv_ready = VN_TRUE;
    return VN_OK;
}

static void rvv_shutdown(void) {
    if (g_rvv_framebuffer != (vn_u32*)0) {
        free(g_rvv_framebuffer);
    }
    if (g_rvv_row_colors != (vn_u32*)0) {
        free(g_rvv_row_colors);
    }
    if (g_rvv_u_lut != (vn_u32*)0) {
        free(g_rvv_u_lut);
    }
    if (g_rvv_v_lut != (vn_u32*)0) {
        free(g_rvv_v_lut);
    }
    g_rvv_framebuffer = (vn_u32*)0;
    g_rvv_row_colors = (vn_u32*)0;
    g_rvv_u_lut = (vn_u32*)0;
    g_rvv_v_lut = (vn_u32*)0;
    g_rvv_stride = 0u;
    g_rvv_height = 0u;
    g_rvv_pixels = 0u;
    g_rvv_row_colors_cap = 0u;
    g_rvv_u_lut_cap = 0u;
    g_rvv_v_lut_cap = 0u;
    g_rvv_cfg.width = 0u;
    g_rvv_cfg.height = 0u;
    g_rvv_cfg.flags = 0u;
    g_rvv_ready = VN_FALSE;
}

static void rvv_begin_frame(void) {
}

static int rvv_submit_ops(const VNRenderOp* ops, vn_u32 op_count) {
    vn_u32 i;

    if (g_rvv_ready == VN_FALSE) {
        return VN_E_RENDER_STATE;
    }
    if (ops == (const VNRenderOp*)0 && op_count != 0u) {
        return VN_E_INVALID_ARG;
    }

    for (i = 0u; i < op_count; ++i) {
        const VNRenderOp* op;
        op = &ops[i];
        if (op->op == VN_OP_CLEAR) {
            vn_rvv_clear_frame(op->alpha);
        } else if (op->op == VN_OP_SPRITE || op->op == VN_OP_TEXT) {
            vn_rvv_draw_textured_rect(op);
        } else if (op->op == VN_OP_FADE) {
            vn_rvv_fill_rect_uniform(0, 0, g_rvv_cfg.width, g_rvv_cfg.height, 0xFF000000u, op->alpha);
        } else {
            return VN_E_FORMAT;
        }
    }
    return VN_OK;
}

static void rvv_end_frame(void) {
}

static void rvv_query_caps(VNBackendCaps* out_caps) {
    if (out_caps == (VNBackendCaps*)0) {
        return;
    }
    out_caps->has_simd = 1u;
    out_caps->has_lut_blend = 0u;
    out_caps->has_tmem_cache = 0u;
}

static const VNRenderBackend g_rvv_backend = {
    "rvv",
    VN_ARCH_RVV,
    rvv_init,
    rvv_shutdown,
    rvv_begin_frame,
    rvv_submit_ops,
    rvv_end_frame,
    rvv_query_caps
};

int vn_register_rvv_backend(void) {
    return vn_backend_register(&g_rvv_backend);
}

vn_u32 vn_rvv_backend_debug_frame_crc32(void) {
    if (g_rvv_ready == VN_FALSE) {
        return 0u;
    }
    return vn_pp_frame_crc32(g_rvv_framebuffer, g_rvv_pixels);
}

vn_u32 vn_rvv_backend_debug_copy_framebuffer(vn_u32* out_pixels, vn_u32 pixel_count) {
    if (g_rvv_ready == VN_FALSE || out_pixels == (vn_u32*)0 || pixel_count < g_rvv_pixels) {
        return 0u;
    }
    (void)memcpy(out_pixels, g_rvv_framebuffer, (size_t)g_rvv_pixels * sizeof(vn_u32));
    return g_rvv_pixels;
}
