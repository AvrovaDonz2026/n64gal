#include <stdio.h>

#include "vn_renderer.h"
#include "vn_error.h"

int vn_register_scalar_backend(void);
int vn_register_avx2_backend(void);
int vn_register_neon_backend(void);
int vn_register_rvv_backend(void);

static RendererConfig g_cfg;
static int g_initialized = VN_FALSE;

static int vn_renderer_register_backends(void) {
    int rc;

    rc = vn_register_scalar_backend();
    if (rc != VN_OK) {
        return rc;
    }

    rc = vn_register_avx2_backend();
    if (rc != VN_OK) {
        (void)fprintf(stderr, "avx2 register skipped rc=%d\n", rc);
    }

    rc = vn_register_neon_backend();
    if (rc != VN_OK) {
        (void)fprintf(stderr, "neon register skipped rc=%d\n", rc);
    }

    rc = vn_register_rvv_backend();
    if (rc != VN_OK) {
        (void)fprintf(stderr, "rvv register skipped rc=%d\n", rc);
    }

    return VN_OK;
}

static int vn_renderer_try_backend(vn_u32 arch_mask, const RendererConfig* cfg) {
    const VNRenderBackend* be;
    int rc;

    be = vn_backend_select(arch_mask);
    if (be == (const VNRenderBackend*)0 || be->init == (int (*)(const RendererConfig*))0) {
        return VN_E_RENDER_STATE;
    }

    rc = be->init(cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "backend init failed rc=%d backend=%s\n", rc, be->name);
    }
    return rc;
}

static vn_u32 vn_renderer_arch_mask_from_flags(vn_u32 flags) {
    if ((flags & VN_RENDERER_FLAG_FORCE_AVX2) != 0u) {
        return VN_ARCH_MASK_AVX2;
    }
    if ((flags & VN_RENDERER_FLAG_FORCE_NEON) != 0u) {
        return VN_ARCH_MASK_NEON;
    }
    if ((flags & VN_RENDERER_FLAG_FORCE_RVV) != 0u) {
        return VN_ARCH_MASK_RVV;
    }
    if ((flags & VN_RENDERER_FLAG_FORCE_SCALAR) != 0u) {
        return VN_ARCH_MASK_SCALAR;
    }
    if ((flags & VN_RENDERER_FLAG_SIMD) != 0u) {
        return VN_ARCH_MASK_ALL;
    }
    return VN_ARCH_MASK_SCALAR;
}

static int renderer_backend_submit_full(const VNRenderBackend* be,
                                        const VNRenderOp* ops,
                                        vn_u32 op_count) {
    if (be == (const VNRenderBackend*)0 ||
        be->submit_ops == (int (*)(const VNRenderOp*, vn_u32))0) {
        return VN_E_RENDER_STATE;
    }
    return be->submit_ops(ops, op_count);
}

int renderer_init(const RendererConfig* cfg) {
    vn_u32 arch_mask;
    int rc;
    int forced_backend;

    if (cfg == (const RendererConfig*)0 || cfg->width == 0 || cfg->height == 0) {
        return VN_E_INVALID_ARG;
    }

    vn_backend_reset_registry();
    rc = vn_renderer_register_backends();
    if (rc != VN_OK) {
        return rc;
    }

    g_cfg = *cfg;
    arch_mask = vn_renderer_arch_mask_from_flags(cfg->flags);
    forced_backend = (arch_mask != VN_ARCH_MASK_ALL && arch_mask != VN_ARCH_MASK_SCALAR) ? VN_TRUE : VN_FALSE;

    if ((arch_mask & VN_ARCH_MASK_AVX2) != 0u) {
        rc = vn_renderer_try_backend(VN_ARCH_MASK_AVX2, &g_cfg);
        if (rc == VN_OK) {
            g_initialized = VN_TRUE;
            return VN_OK;
        }
        if (forced_backend != VN_FALSE) {
            rc = vn_renderer_try_backend(VN_ARCH_MASK_SCALAR, &g_cfg);
            if (rc == VN_OK) {
                g_initialized = VN_TRUE;
            }
            return rc;
        }
    }

    if ((arch_mask & VN_ARCH_MASK_NEON) != 0u) {
        rc = vn_renderer_try_backend(VN_ARCH_MASK_NEON, &g_cfg);
        if (rc == VN_OK) {
            g_initialized = VN_TRUE;
            return VN_OK;
        }
        if (forced_backend != VN_FALSE) {
            rc = vn_renderer_try_backend(VN_ARCH_MASK_SCALAR, &g_cfg);
            if (rc == VN_OK) {
                g_initialized = VN_TRUE;
            }
            return rc;
        }
    }

    if ((arch_mask & VN_ARCH_MASK_RVV) != 0u) {
        rc = vn_renderer_try_backend(VN_ARCH_MASK_RVV, &g_cfg);
        if (rc == VN_OK) {
            g_initialized = VN_TRUE;
            return VN_OK;
        }
        if (forced_backend != VN_FALSE) {
            rc = vn_renderer_try_backend(VN_ARCH_MASK_SCALAR, &g_cfg);
            if (rc == VN_OK) {
                g_initialized = VN_TRUE;
            }
            return rc;
        }
    }

    if ((arch_mask & VN_ARCH_MASK_SCALAR) != 0u || forced_backend == VN_FALSE) {
        rc = vn_renderer_try_backend(VN_ARCH_MASK_SCALAR, &g_cfg);
        if (rc == VN_OK) {
            g_initialized = VN_TRUE;
        }
        return rc;
    }

    return VN_E_RENDER_STATE;
}

void renderer_begin_frame(void) {
    const VNRenderBackend* be;
    if (g_initialized == VN_FALSE) {
        return;
    }
    be = vn_backend_get_active();
    if (be != (const VNRenderBackend*)0 && be->begin_frame != (void (*)(void))0) {
        be->begin_frame();
    }
}

void renderer_submit(const VNRenderOp* ops, vn_u32 op_count) {
    const VNRenderBackend* be;
    int rc;
    if (g_initialized == VN_FALSE) {
        return;
    }
    be = vn_backend_get_active();
    rc = renderer_backend_submit_full(be, ops, op_count);
    if (rc != VN_OK && be != (const VNRenderBackend*)0) {
        (void)fprintf(stderr, "renderer_submit failed rc=%d backend=%s\n", rc, be->name);
    }
}

void renderer_submit_dirty(const VNRenderOp* ops,
                           vn_u32 op_count,
                           const VNRenderDirtySubmit* dirty_submit) {
    const VNRenderBackend* be;
    int rc;

    if (g_initialized == VN_FALSE) {
        return;
    }
    be = vn_backend_get_active();
    if (be == (const VNRenderBackend*)0) {
        return;
    }
    if (dirty_submit == (const VNRenderDirtySubmit*)0) {
        renderer_submit(ops, op_count);
        return;
    }
    if (dirty_submit->full_redraw != 0u) {
        renderer_submit(ops, op_count);
        return;
    }
    if (dirty_submit->rect_count == 0u) {
        return;
    }
    if (be->submit_ops_dirty == (int (*)(const VNRenderOp*, vn_u32, const VNRenderDirtySubmit*))0) {
        renderer_submit(ops, op_count);
        return;
    }

    rc = be->submit_ops_dirty(ops, op_count, dirty_submit);
    if (rc == VN_OK) {
        return;
    }

    (void)fprintf(stderr,
                  "renderer_submit_dirty failed rc=%d backend=%s, falling back to full submit\n",
                  rc,
                  be->name);
    renderer_submit(ops, op_count);
}

void renderer_end_frame(void) {
    const VNRenderBackend* be;
    if (g_initialized == VN_FALSE) {
        return;
    }
    be = vn_backend_get_active();
    if (be != (const VNRenderBackend*)0 && be->end_frame != (void (*)(void))0) {
        be->end_frame();
    }
}

void renderer_shutdown(void) {
    const VNRenderBackend* be;
    if (g_initialized == VN_FALSE) {
        return;
    }
    be = vn_backend_get_active();
    if (be != (const VNRenderBackend*)0 && be->shutdown != (void (*)(void))0) {
        be->shutdown();
    }
    g_initialized = VN_FALSE;
}

const char* renderer_backend_name(void) {
    const VNRenderBackend* be;
    be = vn_backend_get_active();
    if (be == (const VNRenderBackend*)0 || be->name == (const char*)0) {
        return "none";
    }
    return be->name;
}
