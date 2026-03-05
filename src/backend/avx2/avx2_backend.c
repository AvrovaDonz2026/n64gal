#include "vn_backend.h"
#include "vn_renderer.h"
#include "vn_error.h"

static int avx2_init(const RendererConfig* cfg) {
    (void)cfg;
    return VN_E_UNSUPPORTED;
}

static void avx2_shutdown(void) {
}

static void avx2_begin_frame(void) {
}

static int avx2_submit_ops(const VNRenderOp* ops, vn_u32 op_count) {
    (void)ops;
    (void)op_count;
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
