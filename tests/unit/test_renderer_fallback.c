#include <stdio.h>
#include <string.h>

#include "vn_renderer.h"
#include "vn_error.h"

int main(void) {
    RendererConfig cfg;
    int rc;
    const char* name;

    cfg.width = 600u;
    cfg.height = 800u;
    cfg.flags = VN_RENDERER_FLAG_FORCE_AVX2;

    rc = renderer_init(&cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "renderer_init failed rc=%d\n", rc);
        return 1;
    }

    name = renderer_backend_name();
    if (strcmp(name, "scalar") != 0) {
        (void)fprintf(stderr, "expected scalar fallback, got=%s\n", name);
        renderer_shutdown();
        return 1;
    }

    renderer_shutdown();

    (void)printf("test_renderer_fallback ok\n");
    return 0;
}
