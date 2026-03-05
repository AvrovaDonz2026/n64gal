#include "vn_backend.h"
#include "vn_renderer.h"
#include "vn_error.h"

static RendererConfig g_scalar_cfg;

static int scalar_init(const RendererConfig* cfg) {
    if (cfg == (const RendererConfig*)0) {
        return VN_E_INVALID_ARG;
    }
    g_scalar_cfg = *cfg;
    return VN_OK;
}

static void scalar_shutdown(void) {
    g_scalar_cfg.width = 0;
    g_scalar_cfg.height = 0;
    g_scalar_cfg.flags = 0;
}

static void scalar_begin_frame(void) {
}

static int scalar_submit_ops(const VNRenderOp* ops, vn_u32 op_count) {
    vn_u32 i;
    if (ops == (const VNRenderOp*)0 && op_count != 0u) {
        return VN_E_INVALID_ARG;
    }

    for (i = 0; i < op_count; ++i) {
        if (ops[i].op == 0u) {
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
    out_caps->has_simd = 0;
    out_caps->has_lut_blend = 0;
    out_caps->has_tmem_cache = 0;
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
