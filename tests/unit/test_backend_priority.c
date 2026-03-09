#include <stdio.h>
#include <string.h>

#include "vn_renderer.h"
#include "vn_error.h"
#include "../../src/core/build_config.h"

#if VN_ARCH_X64 || VN_ARCH_X86

static int init_backend(vn_u32 flags, char* out_name, size_t out_name_cap) {
    RendererConfig cfg;
    int rc;
    const char* name;

    if (out_name == (char*)0 || out_name_cap == 0u) {
        return 1;
    }

    cfg.width = 600u;
    cfg.height = 800u;
    cfg.flags = flags;

    rc = renderer_init(&cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "renderer_init failed rc=%d flags=0x%X\n", rc, (unsigned int)flags);
        return 1;
    }

    name = renderer_backend_name();
    (void)strncpy(out_name, name, out_name_cap - 1u);
    out_name[out_name_cap - 1u] = '\0';
    renderer_shutdown();
    return 0;
}

#endif

int main(void) {
#if !(VN_ARCH_X64 || VN_ARCH_X86)
    (void)printf("test_backend_priority skipped (non-x86 host)\n");
    return 0;
#else
    char scalar_name[16];
    char avx2_name[16];
    char simd_name[16];

    if (init_backend(VN_RENDERER_FLAG_FORCE_SCALAR, scalar_name, sizeof(scalar_name)) != 0) {
        return 1;
    }
    if (strcmp(scalar_name, "scalar") != 0) {
        (void)fprintf(stderr, "forced scalar expected backend=scalar got=%s\n", scalar_name);
        return 1;
    }

    if (init_backend(VN_RENDERER_FLAG_FORCE_AVX2, avx2_name, sizeof(avx2_name)) != 0) {
        return 1;
    }
    if (strcmp(avx2_name, "avx2") != 0 && strcmp(avx2_name, "scalar") != 0) {
        (void)fprintf(stderr, "forced avx2 expected backend=avx2/scalar got=%s\n", avx2_name);
        return 1;
    }

    if (init_backend(VN_RENDERER_FLAG_SIMD, simd_name, sizeof(simd_name)) != 0) {
        return 1;
    }
    if (strcmp(avx2_name, "avx2") == 0) {
        if (strcmp(simd_name, "avx2") != 0) {
            (void)fprintf(stderr, "simd auto expected backend=avx2 got=%s\n", simd_name);
            return 1;
        }
    } else {
        if (strcmp(simd_name, "scalar") != 0) {
            (void)fprintf(stderr, "simd auto expected scalar fallback got=%s\n", simd_name);
            return 1;
        }
    }

    (void)printf("test_backend_priority ok forced_scalar=%s forced_avx2=%s simd_auto=%s\n",
                 scalar_name,
                 avx2_name,
                 simd_name);
    return 0;
#endif
}

