#include <stdio.h>
#include <string.h>

#include "vn_runtime.h"
#include "vn_error.h"
#include "../../src/core/build_config.h"

vn_u32 vn_scalar_backend_debug_frame_crc32(void);
vn_u32 vn_avx2_backend_debug_frame_crc32(void);
vn_u32 vn_neon_backend_debug_frame_crc32(void);
vn_u32 vn_rvv_backend_debug_frame_crc32(void);

typedef struct {
    const char* scene_name;
    vn_u32 expected_crc;
} GoldenScene;

static const GoldenScene k_golden_scenes[] = {
    { "S0", 0x58C8928Bu },
    { "S1", 0x80D7F175u },
    { "S2", 0x587BC5A4u },
    { "S3", 0x0BC0160Fu }
};

static int should_try_backend(const char* backend_name) {
    if (backend_name == (const char*)0) {
        return 0;
    }
    if (strcmp(backend_name, "avx2") == 0) {
        return (VN_ARCH_X64 || VN_ARCH_X86) ? 1 : 0;
    }
    if (strcmp(backend_name, "neon") == 0) {
        return VN_ARCH_ARM64 ? 1 : 0;
    }
    if (strcmp(backend_name, "rvv") == 0) {
        return VN_ARCH_RISCV64 ? 1 : 0;
    }
    return 0;
}

static vn_u32 debug_crc_for_backend(const char* backend_name) {
    if (backend_name == (const char*)0) {
        return 0u;
    }
    if (strcmp(backend_name, "scalar") == 0) {
        return vn_scalar_backend_debug_frame_crc32();
    }
    if (strcmp(backend_name, "avx2") == 0) {
        return vn_avx2_backend_debug_frame_crc32();
    }
    if (strcmp(backend_name, "neon") == 0) {
        return vn_neon_backend_debug_frame_crc32();
    }
    if (strcmp(backend_name, "rvv") == 0) {
        return vn_rvv_backend_debug_frame_crc32();
    }
    return 0u;
}

static int run_scene_once(const char* scene_name,
                          const char* backend_name,
                          vn_u32* out_crc,
                          const char** out_actual_backend) {
    VNRunConfig cfg;
    VNRuntimeSession* session;
    VNRunResult result;
    int rc;

    if (scene_name == (const char*)0 ||
        backend_name == (const char*)0 ||
        out_crc == (vn_u32*)0 ||
        out_actual_backend == (const char**)0) {
        return VN_E_INVALID_ARG;
    }

    *out_crc = 0u;
    *out_actual_backend = "none";

    vn_run_config_init(&cfg);
    cfg.pack_path = "assets/demo/demo.vnpak";
    cfg.scene_name = scene_name;
    cfg.backend_name = backend_name;
    cfg.width = 600u;
    cfg.height = 800u;
    cfg.frames = 1u;
    cfg.dt_ms = 16u;
    cfg.emit_logs = 0u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.hold_on_end = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK) {
        return rc;
    }

    rc = vn_runtime_session_step(session, &result);
    if (rc == VN_OK) {
        *out_actual_backend = result.backend_name;
        *out_crc = debug_crc_for_backend(result.backend_name);
        if (*out_crc == 0u) {
            (void)fprintf(stderr,
                          "scene=%s backend=%s missing debug crc\n",
                          scene_name,
                          result.backend_name);
            rc = VN_E_RENDER_STATE;
        }
    }

    (void)vn_runtime_session_destroy(session);
    return rc;
}

static int check_optional_backend(const GoldenScene* golden,
                                  const char* backend_name,
                                  int* out_compared_count) {
    vn_u32 backend_crc;
    const char* actual_backend;
    int rc;

    if (golden == (const GoldenScene*)0 || backend_name == (const char*)0 || out_compared_count == (int*)0) {
        return 1;
    }
    if (should_try_backend(backend_name) == 0) {
        return 0;
    }

    backend_crc = 0u;
    actual_backend = "none";
    rc = run_scene_once(golden->scene_name, backend_name, &backend_crc, &actual_backend);
    if (rc == VN_E_UNSUPPORTED) {
        (void)printf("test_runtime_golden skipped scene=%s backend=%s\n",
                     golden->scene_name,
                     backend_name);
        return 0;
    }
    if (rc != VN_OK) {
        return 1;
    }
    if (strcmp(actual_backend, "scalar") == 0) {
        (void)printf("test_runtime_golden skipped scene=%s backend=%s fallback=%s\n",
                     golden->scene_name,
                     backend_name,
                     actual_backend);
        return 0;
    }
    if (strcmp(actual_backend, backend_name) != 0) {
        (void)fprintf(stderr,
                      "scene=%s requested=%s got=%s\n",
                      golden->scene_name,
                      backend_name,
                      actual_backend);
        return 1;
    }
    if (backend_crc != golden->expected_crc) {
        (void)fprintf(stderr,
                      "scene=%s backend=%s crc mismatch expected=0x%08X got=0x%08X\n",
                      golden->scene_name,
                      backend_name,
                      (unsigned int)golden->expected_crc,
                      (unsigned int)backend_crc);
        return 1;
    }

    *out_compared_count += 1;
    (void)printf("test_runtime_golden matched scene=%s backend=%s crc=0x%08X\n",
                 golden->scene_name,
                 backend_name,
                 (unsigned int)backend_crc);
    return 0;
}

int main(void) {
    unsigned int i;
    int compared_count;

    compared_count = 0;
    for (i = 0u; i < (unsigned int)(sizeof(k_golden_scenes) / sizeof(k_golden_scenes[0])); ++i) {
        vn_u32 scalar_crc;
        const char* actual_backend;
        int rc;

        scalar_crc = 0u;
        actual_backend = "none";
        rc = run_scene_once(k_golden_scenes[i].scene_name, "scalar", &scalar_crc, &actual_backend);
        if (rc != VN_OK) {
            (void)fprintf(stderr,
                          "scene=%s scalar rc=%d\n",
                          k_golden_scenes[i].scene_name,
                          rc);
            return 1;
        }
        if (strcmp(actual_backend, "scalar") != 0) {
            (void)fprintf(stderr,
                          "scene=%s scalar requested got=%s\n",
                          k_golden_scenes[i].scene_name,
                          actual_backend);
            return 1;
        }
        if (scalar_crc != k_golden_scenes[i].expected_crc) {
            (void)fprintf(stderr,
                          "scene=%s scalar golden mismatch expected=0x%08X got=0x%08X\n",
                          k_golden_scenes[i].scene_name,
                          (unsigned int)k_golden_scenes[i].expected_crc,
                          (unsigned int)scalar_crc);
            return 1;
        }

        (void)printf("test_runtime_golden scalar scene=%s crc=0x%08X\n",
                     k_golden_scenes[i].scene_name,
                     (unsigned int)scalar_crc);

        if (check_optional_backend(&k_golden_scenes[i], "avx2", &compared_count) != 0) {
            return 1;
        }
        if (check_optional_backend(&k_golden_scenes[i], "neon", &compared_count) != 0) {
            return 1;
        }
        if (check_optional_backend(&k_golden_scenes[i], "rvv", &compared_count) != 0) {
            return 1;
        }
    }

    (void)printf("test_runtime_golden ok compared=%d\n", compared_count);
    return 0;
}
