#include <stdlib.h>
#include <string.h>

#include "vn_backend.h"
#include "vn_renderer.h"
#include "vn_error.h"

#include "../common/pixel_pipeline.h"

static RendererConfig g_scalar_cfg;
static vn_u32* g_scalar_framebuffer = (vn_u32*)0;
static vn_u32 g_scalar_stride = 0u;
static vn_u32 g_scalar_height = 0u;
static vn_u32 g_scalar_pixels = 0u;
static vn_u8* g_scalar_u_lut = (vn_u8*)0;
static vn_u8* g_scalar_v_lut = (vn_u8*)0;
static vn_u32 g_scalar_u_lut_cap = 0u;
static vn_u32 g_scalar_v_lut_cap = 0u;
static int g_scalar_ready = VN_FALSE;

static int vn_scalar_clip_rect_region(vn_i16 x,
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
    if (g_scalar_stride == 0u || g_scalar_height == 0u) {
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
    if (x1 > (int)g_scalar_stride) {
        x1 = (int)g_scalar_stride;
    }
    if (y1 > (int)g_scalar_height) {
        y1 = (int)g_scalar_height;
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

static void vn_scalar_fill_u32(vn_u32* dst, vn_u32 count, vn_u32 value) {
    vn_u32 i;

    i = 0u;
    while (i < count) {
        dst[i] = value;
        i += 1u;
    }
}

static void vn_scalar_fill_rect_uniform_clipped(vn_i16 x,
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

    if (g_scalar_framebuffer == (vn_u32*)0) {
        return;
    }
    if (vn_scalar_clip_rect_region(x, y, w, h, clip_rect, &x0, &y0, &x1, &y1) == VN_FALSE) {
        return;
    }

    if (alpha >= 255u) {
        for (yy = y0; yy < y1; ++yy) {
            vn_u32* row_ptr;
            row_ptr = g_scalar_framebuffer + yy * g_scalar_stride + x0;
            vn_scalar_fill_u32(row_ptr, x1 - x0, color);
        }
        return;
    }

    for (yy = y0; yy < y1; ++yy) {
        vn_u32 xx;
        vn_u32 row_off;
        row_off = yy * g_scalar_stride;
        for (xx = x0; xx < x1; ++xx) {
            vn_u32 idx;
            idx = row_off + xx;
            g_scalar_framebuffer[idx] = vn_pp_blend_rgb(g_scalar_framebuffer[idx], color, alpha);
        }
    }
}

static void vn_scalar_clear_rect(vn_u8 gray, const VNRenderRect* clip_rect) {
    if (clip_rect == (const VNRenderRect*)0) {
        if (g_scalar_framebuffer == (vn_u32*)0 || g_scalar_pixels == 0u) {
            return;
        }
        vn_scalar_fill_u32(g_scalar_framebuffer, g_scalar_pixels, vn_pp_make_gray(gray));
        return;
    }
    vn_scalar_fill_rect_uniform_clipped(0, 0, g_scalar_cfg.width, g_scalar_cfg.height, vn_pp_make_gray(gray), 255u, clip_rect);
}

static void vn_scalar_fill_rect_uniform(vn_i16 x, vn_i16 y, vn_u16 w, vn_u16 h, vn_u32 color, vn_u8 alpha) {
    vn_scalar_fill_rect_uniform_clipped(x, y, w, h, color, alpha, (const VNRenderRect*)0);
}

static void vn_scalar_build_coord_lut(vn_u8* out_lut, vn_u32 count, vn_u32 local_start, vn_u16 extent) {
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

static void vn_scalar_draw_textured_rect_clipped(const VNRenderOp* op, const VNRenderRect* clip_rect) {
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

    if (op == (const VNRenderOp*)0 || g_scalar_framebuffer == (vn_u32*)0) {
        return;
    }
    if (op->alpha == 0u) {
        return;
    }
    if (vn_scalar_clip_rect_region(op->x, op->y, op->w, op->h, clip_rect, &x0, &y0, &x1, &y1) == VN_FALSE) {
        return;
    }

    vis_w = x1 - x0;
    vis_h = y1 - y0;
    if (vis_w == 0u || vis_h == 0u) {
        return;
    }
    if (g_scalar_u_lut == (vn_u8*)0 || g_scalar_v_lut == (vn_u8*)0) {
        return;
    }
    if (vis_w > g_scalar_u_lut_cap || vis_h > g_scalar_v_lut_cap) {
        return;
    }

    local_x_start_i = (int)x0 - (int)op->x;
    local_y_start_i = (int)y0 - (int)op->y;
    if (local_x_start_i < 0 || local_y_start_i < 0) {
        return;
    }
    local_x_start = (vn_u32)local_x_start_i;
    local_y_start = (vn_u32)local_y_start_i;

    vn_scalar_build_coord_lut(g_scalar_u_lut, vis_w, local_x_start, op->w);
    vn_scalar_build_coord_lut(g_scalar_v_lut, vis_h, local_y_start, op->h);

    for (row_rel = 0u; row_rel < vis_h; ++row_rel) {
        vn_u32 yy;
        vn_u32 row_off;
        vn_u32 col_rel;
        vn_u32 v8;

        yy = y0 + row_rel;
        row_off = yy * g_scalar_stride;
        v8 = (vn_u32)g_scalar_v_lut[row_rel];
        for (col_rel = 0u; col_rel < vis_w; ++col_rel) {
            vn_u32 xx;
            vn_u32 idx;
            vn_u32 u8;
            vn_u32 texel;
            vn_u32 color;

            xx = x0 + col_rel;
            idx = row_off + xx;
            u8 = (vn_u32)g_scalar_u_lut[col_rel];
            texel = vn_pp_sample_texel(op->tex_id, u8, v8);
            color = vn_pp_combine_texel(texel, op->layer, op->flags, op->op);

            if (op->alpha >= 255u) {
                g_scalar_framebuffer[idx] = color;
            } else {
                g_scalar_framebuffer[idx] = vn_pp_blend_rgb(g_scalar_framebuffer[idx], color, op->alpha);
            }
        }
    }
}

static void vn_scalar_draw_textured_rect(const VNRenderOp* op) {
    vn_scalar_draw_textured_rect_clipped(op, (const VNRenderRect*)0);
}

static int scalar_init(const RendererConfig* cfg) {
    vn_u32 pixels;
    size_t u_lut_bytes;
    size_t v_lut_bytes;

    if (cfg == (const RendererConfig*)0 || cfg->width == 0u || cfg->height == 0u) {
        return VN_E_INVALID_ARG;
    }

    pixels = (vn_u32)cfg->width * (vn_u32)cfg->height;
    if ((cfg->height != 0u) && (pixels / (vn_u32)cfg->height != (vn_u32)cfg->width)) {
        return VN_E_FORMAT;
    }

    g_scalar_framebuffer = (vn_u32*)malloc((size_t)pixels * sizeof(vn_u32));
    if (g_scalar_framebuffer == (vn_u32*)0) {
        return VN_E_NOMEM;
    }
    u_lut_bytes = (size_t)cfg->width * sizeof(vn_u8);
    v_lut_bytes = (size_t)cfg->height * sizeof(vn_u8);
    g_scalar_u_lut = (vn_u8*)malloc(u_lut_bytes);
    g_scalar_v_lut = (vn_u8*)malloc(v_lut_bytes);
    if (g_scalar_u_lut == (vn_u8*)0 || g_scalar_v_lut == (vn_u8*)0) {
        if (g_scalar_u_lut != (vn_u8*)0) {
            free(g_scalar_u_lut);
        }
        if (g_scalar_v_lut != (vn_u8*)0) {
            free(g_scalar_v_lut);
        }
        g_scalar_u_lut = (vn_u8*)0;
        g_scalar_v_lut = (vn_u8*)0;
        free(g_scalar_framebuffer);
        g_scalar_framebuffer = (vn_u32*)0;
        return VN_E_NOMEM;
    }
    (void)memset(g_scalar_framebuffer, 0, (size_t)pixels * sizeof(vn_u32));

    g_scalar_cfg = *cfg;
    g_scalar_stride = (vn_u32)cfg->width;
    g_scalar_height = (vn_u32)cfg->height;
    g_scalar_pixels = pixels;
    g_scalar_u_lut_cap = (vn_u32)cfg->width;
    g_scalar_v_lut_cap = (vn_u32)cfg->height;
    g_scalar_ready = VN_TRUE;
    return VN_OK;
}

static void scalar_shutdown(void) {
    if (g_scalar_framebuffer != (vn_u32*)0) {
        free(g_scalar_framebuffer);
    }
    if (g_scalar_u_lut != (vn_u8*)0) {
        free(g_scalar_u_lut);
    }
    if (g_scalar_v_lut != (vn_u8*)0) {
        free(g_scalar_v_lut);
    }
    g_scalar_framebuffer = (vn_u32*)0;
    g_scalar_u_lut = (vn_u8*)0;
    g_scalar_v_lut = (vn_u8*)0;
    g_scalar_stride = 0u;
    g_scalar_height = 0u;
    g_scalar_pixels = 0u;
    g_scalar_u_lut_cap = 0u;
    g_scalar_v_lut_cap = 0u;
    g_scalar_cfg.width = 0u;
    g_scalar_cfg.height = 0u;
    g_scalar_cfg.flags = 0u;
    g_scalar_ready = VN_FALSE;
}

static void scalar_begin_frame(void) {
}

static int scalar_submit_ops(const VNRenderOp* ops, vn_u32 op_count) {
    vn_u32 i;

    if (g_scalar_ready == VN_FALSE) {
        return VN_E_RENDER_STATE;
    }
    if (ops == (const VNRenderOp*)0 && op_count != 0u) {
        return VN_E_INVALID_ARG;
    }

    for (i = 0u; i < op_count; ++i) {
        const VNRenderOp* op;
        op = &ops[i];
        if (op->op == VN_OP_CLEAR) {
            vn_scalar_clear_rect(op->alpha, (const VNRenderRect*)0);
        } else if (op->op == VN_OP_SPRITE || op->op == VN_OP_TEXT) {
            vn_scalar_draw_textured_rect(op);
        } else if (op->op == VN_OP_FADE) {
            vn_scalar_fill_rect_uniform(0, 0, g_scalar_cfg.width, g_scalar_cfg.height, 0xFF000000u, op->alpha);
        } else {
            return VN_E_FORMAT;
        }
    }
    return VN_OK;
}

static int scalar_submit_ops_dirty(const VNRenderOp* ops,
                                   vn_u32 op_count,
                                   const VNRenderDirtySubmit* dirty_submit) {
    const VNRenderOp* clear_op;
    vn_u32 rect_index;

    if (g_scalar_ready == VN_FALSE) {
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
    if (dirty_submit->width != g_scalar_cfg.width || dirty_submit->height != g_scalar_cfg.height) {
        return VN_E_INVALID_ARG;
    }
    if (dirty_submit->full_redraw != 0u || op_count == 0u) {
        return scalar_submit_ops(ops, op_count);
    }
    if (dirty_submit->rect_count == 0u) {
        return VN_OK;
    }
    if (ops[0].op != VN_OP_CLEAR) {
        return scalar_submit_ops(ops, op_count);
    }

    clear_op = &ops[0];
    for (rect_index = 0u; rect_index < dirty_submit->rect_count; ++rect_index) {
        const VNRenderRect* clip_rect;
        vn_u32 i;

        clip_rect = &dirty_submit->rects[rect_index];
        vn_scalar_clear_rect(clear_op->alpha, clip_rect);
        for (i = 1u; i < op_count; ++i) {
            const VNRenderOp* op;
            op = &ops[i];
            if (op->op == VN_OP_SPRITE || op->op == VN_OP_TEXT) {
                vn_scalar_draw_textured_rect_clipped(op, clip_rect);
            } else if (op->op == VN_OP_FADE) {
                vn_scalar_fill_rect_uniform_clipped(0,
                                                    0,
                                                    g_scalar_cfg.width,
                                                    g_scalar_cfg.height,
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

static void scalar_end_frame(void) {
}

static void scalar_query_caps(VNBackendCaps* out_caps) {
    if (out_caps == (VNBackendCaps*)0) {
        return;
    }
    out_caps->has_simd = 0u;
    out_caps->has_lut_blend = 0u;
    out_caps->has_tmem_cache = 0u;
}

static const VNRenderBackend g_scalar_backend = {
    "scalar",
    VN_ARCH_SCALAR,
    scalar_init,
    scalar_shutdown,
    scalar_begin_frame,
    scalar_submit_ops,
    scalar_end_frame,
    scalar_query_caps,
    scalar_submit_ops_dirty
};

int vn_register_scalar_backend(void) {
    return vn_backend_register(&g_scalar_backend);
}

vn_u32 vn_scalar_backend_debug_frame_crc32(void) {
    if (g_scalar_ready == VN_FALSE) {
        return 0u;
    }
    return vn_pp_frame_crc32(g_scalar_framebuffer, g_scalar_pixels);
}

vn_u32 vn_scalar_backend_debug_copy_framebuffer(vn_u32* out_pixels, vn_u32 pixel_count) {
    if (g_scalar_ready == VN_FALSE || out_pixels == (vn_u32*)0 || pixel_count < g_scalar_pixels) {
        return 0u;
    }
    (void)memcpy(out_pixels, g_scalar_framebuffer, (size_t)g_scalar_pixels * sizeof(vn_u32));
    return g_scalar_pixels;
}
