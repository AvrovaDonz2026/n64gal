#include <stdio.h>

#include "vn_backend.h"
#include "vn_error.h"

static int dummy_init(const RendererConfig* cfg) {
    (void)cfg;
    return VN_OK;
}

static void dummy_shutdown(void) {
}

static void dummy_begin_frame(void) {
}

static int dummy_submit(const VNRenderOp* ops, vn_u32 op_count) {
    (void)ops;
    (void)op_count;
    return VN_OK;
}

static void dummy_end_frame(void) {
}

static void dummy_caps(VNBackendCaps* caps) {
    if (caps != (VNBackendCaps*)0) {
        caps->has_simd = 0;
        caps->has_lut_blend = 0;
        caps->has_tmem_cache = 0;
    }
}

static const VNRenderBackend g_dummy_scalar = {
    "dummy-scalar",
    VN_ARCH_SCALAR,
    dummy_init,
    dummy_shutdown,
    dummy_begin_frame,
    dummy_submit,
    dummy_end_frame,
    dummy_caps
};

int main(void) {
    const VNRenderBackend* picked;
    int rc;

    vn_backend_reset_registry();
    rc = vn_backend_register(&g_dummy_scalar);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "register failed rc=%d\n", rc);
        return 1;
    }

    picked = vn_backend_select(VN_ARCH_MASK_SCALAR);
    if (picked == (const VNRenderBackend*)0 || picked->arch_tag != VN_ARCH_SCALAR) {
        (void)fprintf(stderr, "select failed\n");
        return 1;
    }

    (void)printf("test_backend_registry ok\n");
    return 0;
}
