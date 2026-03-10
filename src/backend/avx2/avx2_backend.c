#include "avx2_internal.h"

RendererConfig g_avx2_cfg;
vn_u32* g_avx2_framebuffer = (vn_u32*)0;
vn_u32 g_avx2_stride = 0u;
vn_u32 g_avx2_height = 0u;
vn_u32 g_avx2_pixels = 0u;
vn_u8* g_avx2_u_lut = (vn_u8*)0;
vn_u8* g_avx2_v_lut = (vn_u8*)0;
vn_u32 g_avx2_u_lut_cap = 0u;
vn_u32 g_avx2_v_lut_cap = 0u;
int g_avx2_ready = VN_FALSE;
int g_avx2_use_asm_fill = VN_FALSE;

int vn_avx2_clip_rect_region(vn_i16 x,
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
static int vn_avx2_runtime_supported(void) {
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") ? VN_TRUE : VN_FALSE;
}
#elif defined(VN_AVX2_MSVC_IMPL)
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
static int vn_avx2_runtime_supported(void) {
    return VN_FALSE;
}
#endif

static int avx2_init_common(const RendererConfig* cfg, int use_asm_fill) {
    vn_u32 pixels;
    size_t u_lut_bytes;
    size_t v_lut_bytes;

    g_avx2_use_asm_fill = VN_FALSE;

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
    if (use_asm_fill != VN_FALSE && VN_AVX2_ASM_FILL_AVAILABLE != 0) {
        g_avx2_use_asm_fill = VN_TRUE;
    }
    g_avx2_ready = VN_TRUE;
    return VN_OK;
}

static int avx2_init(const RendererConfig* cfg) {
    return avx2_init_common(cfg, VN_FALSE);
}

static int avx2_asm_init(const RendererConfig* cfg) {
    if (VN_AVX2_ASM_FILL_AVAILABLE == 0) {
        return VN_E_UNSUPPORTED;
    }
    return avx2_init_common(cfg, VN_TRUE);
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
    g_avx2_use_asm_fill = VN_FALSE;
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

static const VNRenderBackend g_avx2_asm_backend = {
    "avx2_asm",
    VN_ARCH_AVX2_ASM,
    avx2_asm_init,
    avx2_shutdown,
    avx2_begin_frame,
    avx2_submit_ops,
    avx2_end_frame,
    avx2_query_caps,
    avx2_submit_ops_dirty
};

int vn_register_avx2_asm_backend(void) {
    return vn_backend_register(&g_avx2_asm_backend);
}

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
