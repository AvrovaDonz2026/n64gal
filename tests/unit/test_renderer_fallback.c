#include <stdio.h>
#include <string.h>

#include "vn_renderer.h"
#include "vn_error.h"

static int expect_backend(vn_u32 flags, const char* requested_name) {
    RendererConfig cfg;
    int rc;
    const char* name;

    cfg.width = 600u;
    cfg.height = 800u;
    cfg.flags = flags;

    rc = renderer_init(&cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "renderer_init failed rc=%d flags=0x%X\n", rc, (unsigned int)flags);
        return 1;
    }

    name = renderer_backend_name();
    if (strcmp(name, "scalar") != 0 && strcmp(name, requested_name) != 0) {
        (void)fprintf(stderr, "expected scalar/%s backend, got=%s\n", requested_name, name);
        renderer_shutdown();
        return 1;
    }

    renderer_shutdown();
    return 0;
}

int main(void) {
    if (expect_backend(VN_RENDERER_FLAG_FORCE_AVX2, "avx2") != 0) {
        return 1;
    }
    if (expect_backend(VN_RENDERER_FLAG_FORCE_NEON, "neon") != 0) {
        return 1;
    }
    if (expect_backend(VN_RENDERER_FLAG_FORCE_RVV, "rvv") != 0) {
        return 1;
    }

    (void)printf("test_renderer_fallback ok\n");
    return 0;
}
