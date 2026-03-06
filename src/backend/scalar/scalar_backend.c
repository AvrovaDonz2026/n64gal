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
static int g_scalar_ready = VN_FALSE;

static int vn_scalar_clip_rect(vn_i16 x, vn_i16 y, vn_u16 w, vn_u16 h, vn_u32* out_x0, vn_u32* out_y0, vn_u32* out_x1, vn_u32* out_y1) {
    int x0;
    int y0;
    int x1;
    int y1;

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

static void vn_scalar_clear_frame(vn_u8 gray) {
    if (g_scalar_framebuffer == (vn_u32*)0 || g_scalar_pixels == 0u) {
        return;
    }
    vn_scalar_fill_u32(g_scalar_framebuffer, g_scalar_pixels, vn_pp_make_gray(gray));
}

static void vn_scalar_fill_rect_uniform(vn_i16 x, vn_i16 y, vn_u16 w, vn_u16 h, vn_u32 color, vn_u8 alpha) {
    vn_u32 x0;
    vn_u32 y0;
    vn_u32 x1;
    vn_u32 y1;
    vn_u32 yy;

    if (g_scalar_framebuffer == (vn_u32*)0) {
        return;
    }
    if (vn_scalar_clip_rect(x, y, w, h, &x0, &y0, &x1, &y1) == VN_FALSE) {
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

static vn_u32 vn_scalar_tex_coord(vn_u32 local, vn_u16 extent) {
    vn_u32 denom;

    if (extent <= 1u) {
        return 0u;
    }
    denom = (vn_u32)extent - 1u;
    return (local * 255u) / denom;
}

static void vn_scalar_draw_textured_rect(const VNRenderOp* op) {
    vn_u32 x0;
    vn_u32 y0;
    vn_u32 x1;
    vn_u32 y1;
    vn_u32 yy;

    if (op == (const VNRenderOp*)0 || g_scalar_framebuffer == (vn_u32*)0) {
        return;
    }
    if (vn_scalar_clip_rect(op->x, op->y, op->w, op->h, &x0, &y0, &x1, &y1) == VN_FALSE) {
        return;
    }

    for (yy = y0; yy < y1; ++yy) {
        vn_u32 xx;
        vn_u32 row_off;
        row_off = yy * g_scalar_stride;
        for (xx = x0; xx < x1; ++xx) {
            vn_u32 idx;
            int local_x_i;
            int local_y_i;
            vn_u32 local_x;
            vn_u32 local_y;
            vn_u32 u8;
            vn_u32 v8;
            vn_u32 texel;
            vn_u32 color;

            idx = row_off + xx;
            local_x_i = (int)xx - (int)op->x;
            local_y_i = (int)yy - (int)op->y;
            if (local_x_i < 0 || local_y_i < 0) {
                continue;
            }
            local_x = (vn_u32)local_x_i;
            local_y = (vn_u32)local_y_i;
            u8 = vn_scalar_tex_coord(local_x, op->w);
            v8 = vn_scalar_tex_coord(local_y, op->h);
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

static int scalar_init(const RendererConfig* cfg) {
    vn_u32 pixels;

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
    (void)memset(g_scalar_framebuffer, 0, (size_t)pixels * sizeof(vn_u32));

    g_scalar_cfg = *cfg;
    g_scalar_stride = (vn_u32)cfg->width;
    g_scalar_height = (vn_u32)cfg->height;
    g_scalar_pixels = pixels;
    g_scalar_ready = VN_TRUE;
    return VN_OK;
}

static void scalar_shutdown(void) {
    if (g_scalar_framebuffer != (vn_u32*)0) {
        free(g_scalar_framebuffer);
    }
    g_scalar_framebuffer = (vn_u32*)0;
    g_scalar_stride = 0u;
    g_scalar_height = 0u;
    g_scalar_pixels = 0u;
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
            vn_scalar_clear_frame(op->alpha);
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
    scalar_query_caps
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
