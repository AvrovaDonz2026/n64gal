#include <stdio.h>

#include "vn_renderer.h"
#include "vn_error.h"

int vn_register_scalar_backend(void);
int vn_register_avx2_backend(void);

static RendererConfig g_cfg;
static int g_initialized = VN_FALSE;

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

int renderer_init(const RendererConfig* cfg) {
    vn_u32 arch_mask;
    const VNRenderBackend* be;
    int first_init_rc;
    int rc;

    if (cfg == (const RendererConfig*)0 || cfg->width == 0 || cfg->height == 0) {
        return VN_E_INVALID_ARG;
    }

    vn_backend_reset_registry();
    rc = vn_register_scalar_backend();
    if (rc != VN_OK) {
        return rc;
    }
    rc = vn_register_avx2_backend();
    if (rc != VN_OK) {
        (void)fprintf(stderr, "avx2 register skipped rc=%d\n", rc);
    }

    arch_mask = vn_renderer_arch_mask_from_flags(cfg->flags);
    be = vn_backend_select(arch_mask);
    if (be == (const VNRenderBackend*)0) {
        be = vn_backend_select(VN_ARCH_MASK_SCALAR);
    }
    if (be == (const VNRenderBackend*)0 || be->init == (int (*)(const RendererConfig*))0) {
        return VN_E_RENDER_STATE;
    }

    g_cfg = *cfg;
    first_init_rc = be->init(&g_cfg);
    if (first_init_rc != VN_OK) {
        if (be->arch_tag != VN_ARCH_SCALAR) {
            (void)fprintf(stderr, "backend init failed rc=%d backend=%s, fallback=scalar\n", first_init_rc, be->name);
            be = vn_backend_select(VN_ARCH_MASK_SCALAR);
            if (be == (const VNRenderBackend*)0 || be->init == (int (*)(const RendererConfig*))0) {
                return first_init_rc;
            }
            rc = be->init(&g_cfg);
            if (rc != VN_OK) {
                return rc;
            }
        } else {
            return first_init_rc;
        }
    }

    g_initialized = VN_TRUE;
    return VN_OK;
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
    if (be == (const VNRenderBackend*)0 || be->submit_ops == (int (*)(const VNRenderOp*, vn_u32))0) {
        return;
    }
    rc = be->submit_ops(ops, op_count);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "renderer_submit failed rc=%d backend=%s\n", rc, be->name);
    }
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
