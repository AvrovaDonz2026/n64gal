#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vn_runtime.h"
#include "vn_error.h"
#include "../../src/core/build_config.h"

#define VN_GOLDEN_WIDTH 600u
#define VN_GOLDEN_HEIGHT 800u
#define VN_GOLDEN_PIXELS (VN_GOLDEN_WIDTH * VN_GOLDEN_HEIGHT)

vn_u32 vn_scalar_backend_debug_frame_crc32(void);
vn_u32 vn_avx2_backend_debug_frame_crc32(void);
vn_u32 vn_neon_backend_debug_frame_crc32(void);
vn_u32 vn_rvv_backend_debug_frame_crc32(void);

vn_u32 vn_scalar_backend_debug_copy_framebuffer(vn_u32* out_pixels, vn_u32 pixel_count);
vn_u32 vn_avx2_backend_debug_copy_framebuffer(vn_u32* out_pixels, vn_u32 pixel_count);
vn_u32 vn_neon_backend_debug_copy_framebuffer(vn_u32* out_pixels, vn_u32 pixel_count);
vn_u32 vn_rvv_backend_debug_copy_framebuffer(vn_u32* out_pixels, vn_u32 pixel_count);

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

static vn_u32 debug_copy_for_backend(const char* backend_name,
                                     vn_u32* out_pixels,
                                     vn_u32 pixel_count) {
    if (backend_name == (const char*)0) {
        return 0u;
    }
    if (strcmp(backend_name, "scalar") == 0) {
        return vn_scalar_backend_debug_copy_framebuffer(out_pixels, pixel_count);
    }
    if (strcmp(backend_name, "avx2") == 0) {
        return vn_avx2_backend_debug_copy_framebuffer(out_pixels, pixel_count);
    }
    if (strcmp(backend_name, "neon") == 0) {
        return vn_neon_backend_debug_copy_framebuffer(out_pixels, pixel_count);
    }
    if (strcmp(backend_name, "rvv") == 0) {
        return vn_rvv_backend_debug_copy_framebuffer(out_pixels, pixel_count);
    }
    return 0u;
}

static unsigned int channel_abs_diff(vn_u32 a, vn_u32 b, unsigned int shift) {
    unsigned int va;
    unsigned int vb;

    va = (unsigned int)((a >> shift) & 0xFFu);
    vb = (unsigned int)((b >> shift) & 0xFFu);
    if (va >= vb) {
        return va - vb;
    }
    return vb - va;
}

static void build_diff_pixel(vn_u32 expected, vn_u32 actual, unsigned char* out_rgb) {
    if (out_rgb == (unsigned char*)0) {
        return;
    }
    out_rgb[0] = (unsigned char)channel_abs_diff(expected, actual, 16u);
    out_rgb[1] = (unsigned char)channel_abs_diff(expected, actual, 8u);
    out_rgb[2] = (unsigned char)channel_abs_diff(expected, actual, 0u);
}

static int write_ppm_from_pixels(const char* path,
                                 const vn_u32* pixels,
                                 vn_u32 width,
                                 vn_u32 height) {
    FILE* fp;
    vn_u32 i;
    vn_u32 count;

    if (path == (const char*)0 || pixels == (const vn_u32*)0 || width == 0u || height == 0u) {
        return 1;
    }
    fp = fopen(path, "wb");
    if (fp == (FILE*)0) {
        return 1;
    }
    (void)fprintf(fp, "P6\n%u %u\n255\n", (unsigned int)width, (unsigned int)height);
    count = width * height;
    for (i = 0u; i < count; ++i) {
        unsigned char rgb[3];
        rgb[0] = (unsigned char)((pixels[i] >> 16) & 0xFFu);
        rgb[1] = (unsigned char)((pixels[i] >> 8) & 0xFFu);
        rgb[2] = (unsigned char)(pixels[i] & 0xFFu);
        if (fwrite(rgb, 1u, 3u, fp) != 3u) {
            (void)fclose(fp);
            return 1;
        }
    }
    (void)fclose(fp);
    return 0;
}

static int write_ppm_diff(const char* path,
                          const vn_u32* expected_pixels,
                          const vn_u32* actual_pixels,
                          vn_u32 width,
                          vn_u32 height) {
    FILE* fp;
    vn_u32 i;
    vn_u32 count;

    if (path == (const char*)0 ||
        expected_pixels == (const vn_u32*)0 ||
        actual_pixels == (const vn_u32*)0 ||
        width == 0u ||
        height == 0u) {
        return 1;
    }
    fp = fopen(path, "wb");
    if (fp == (FILE*)0) {
        return 1;
    }
    (void)fprintf(fp, "P6\n%u %u\n255\n", (unsigned int)width, (unsigned int)height);
    count = width * height;
    for (i = 0u; i < count; ++i) {
        unsigned char diff_rgb[3];
        build_diff_pixel(expected_pixels[i], actual_pixels[i], diff_rgb);
        if (fwrite(diff_rgb, 1u, 3u, fp) != 3u) {
            (void)fclose(fp);
            return 1;
        }
    }
    (void)fclose(fp);
    return 0;
}

static void emit_diff_artifacts(const char* scene_name,
                                const char* backend_name,
                                const vn_u32* expected_pixels,
                                const vn_u32* actual_pixels) {
    char expected_path[128];
    char actual_path[128];
    char diff_path[128];

    if (scene_name == (const char*)0 ||
        backend_name == (const char*)0 ||
        expected_pixels == (const vn_u32*)0 ||
        actual_pixels == (const vn_u32*)0) {
        return;
    }

    (void)sprintf(expected_path,
                  "test_runtime_golden_%s_%s_expected.ppm",
                  scene_name,
                  backend_name);
    (void)sprintf(actual_path,
                  "test_runtime_golden_%s_%s_actual.ppm",
                  scene_name,
                  backend_name);
    (void)sprintf(diff_path,
                  "test_runtime_golden_%s_%s_diff.ppm",
                  scene_name,
                  backend_name);

    (void)write_ppm_from_pixels(expected_path, expected_pixels, VN_GOLDEN_WIDTH, VN_GOLDEN_HEIGHT);
    (void)write_ppm_from_pixels(actual_path, actual_pixels, VN_GOLDEN_WIDTH, VN_GOLDEN_HEIGHT);
    (void)write_ppm_diff(diff_path, expected_pixels, actual_pixels, VN_GOLDEN_WIDTH, VN_GOLDEN_HEIGHT);
    (void)printf("test_runtime_golden wrote artifacts expected=%s actual=%s diff=%s\n",
                 expected_path,
                 actual_path,
                 diff_path);
}

static int compare_exact_pixels(const char* scene_name,
                                const char* backend_name,
                                const vn_u32* expected_pixels,
                                const vn_u32* actual_pixels,
                                vn_u32 pixel_count) {
    vn_u32 i;
    vn_u32 mismatch_count;
    unsigned int max_channel_diff;

    if (scene_name == (const char*)0 ||
        backend_name == (const char*)0 ||
        expected_pixels == (const vn_u32*)0 ||
        actual_pixels == (const vn_u32*)0) {
        return 1;
    }

    mismatch_count = 0u;
    max_channel_diff = 0u;
    for (i = 0u; i < pixel_count; ++i) {
        if (expected_pixels[i] != actual_pixels[i]) {
            unsigned int diff_r;
            unsigned int diff_g;
            unsigned int diff_b;
            unsigned int local_max;

            mismatch_count += 1u;
            diff_r = channel_abs_diff(expected_pixels[i], actual_pixels[i], 16u);
            diff_g = channel_abs_diff(expected_pixels[i], actual_pixels[i], 8u);
            diff_b = channel_abs_diff(expected_pixels[i], actual_pixels[i], 0u);
            local_max = diff_r;
            if (diff_g > local_max) {
                local_max = diff_g;
            }
            if (diff_b > local_max) {
                local_max = diff_b;
            }
            if (local_max > max_channel_diff) {
                max_channel_diff = local_max;
            }
        }
    }

    if (mismatch_count == 0u) {
        return 0;
    }

    emit_diff_artifacts(scene_name, backend_name, expected_pixels, actual_pixels);
    (void)fprintf(stderr,
                  "scene=%s backend=%s pixel mismatch count=%u max_channel_diff=%u\n",
                  scene_name,
                  backend_name,
                  (unsigned int)mismatch_count,
                  max_channel_diff);
    return 1;
}

static int run_scene_once(const char* scene_name,
                          const char* backend_name,
                          vn_u32* out_crc,
                          const char** out_actual_backend,
                          vn_u32* out_pixels,
                          vn_u32 pixel_count) {
    VNRunConfig cfg;
    VNRuntimeSession* session;
    VNRunResult result;
    vn_u32 copied_count;
    int rc;

    if (scene_name == (const char*)0 ||
        backend_name == (const char*)0 ||
        out_crc == (vn_u32*)0 ||
        out_actual_backend == (const char**)0 ||
        out_pixels == (vn_u32*)0 ||
        pixel_count < VN_GOLDEN_PIXELS) {
        return VN_E_INVALID_ARG;
    }

    *out_crc = 0u;
    *out_actual_backend = "none";

    vn_run_config_init(&cfg);
    cfg.pack_path = "assets/demo/demo.vnpak";
    cfg.scene_name = scene_name;
    cfg.backend_name = backend_name;
    cfg.width = VN_GOLDEN_WIDTH;
    cfg.height = VN_GOLDEN_HEIGHT;
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
        copied_count = debug_copy_for_backend(result.backend_name, out_pixels, pixel_count);
        if (*out_crc == 0u || copied_count != VN_GOLDEN_PIXELS) {
            (void)fprintf(stderr,
                          "scene=%s backend=%s debug snapshot unavailable crc=0x%08X copied=%u\n",
                          scene_name,
                          result.backend_name,
                          (unsigned int)*out_crc,
                          (unsigned int)copied_count);
            rc = VN_E_RENDER_STATE;
        }
    }

    (void)vn_runtime_session_destroy(session);
    return rc;
}

static int check_optional_backend(const GoldenScene* golden,
                                  const char* backend_name,
                                  const vn_u32* expected_pixels,
                                  vn_u32* actual_pixels,
                                  int* out_compared_count) {
    vn_u32 backend_crc;
    const char* actual_backend;
    int rc;

    if (golden == (const GoldenScene*)0 ||
        backend_name == (const char*)0 ||
        expected_pixels == (const vn_u32*)0 ||
        actual_pixels == (vn_u32*)0 ||
        out_compared_count == (int*)0) {
        return 1;
    }
    if (should_try_backend(backend_name) == 0) {
        return 0;
    }

    backend_crc = 0u;
    actual_backend = "none";
    rc = run_scene_once(golden->scene_name,
                        backend_name,
                        &backend_crc,
                        &actual_backend,
                        actual_pixels,
                        VN_GOLDEN_PIXELS);
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
    if (compare_exact_pixels(golden->scene_name,
                             backend_name,
                             expected_pixels,
                             actual_pixels,
                             VN_GOLDEN_PIXELS) != 0) {
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
    vn_u32* scalar_pixels;
    vn_u32* backend_pixels;
    unsigned int i;
    int compared_count;
    int exit_code;

    scalar_pixels = (vn_u32*)malloc((size_t)VN_GOLDEN_PIXELS * sizeof(vn_u32));
    backend_pixels = (vn_u32*)malloc((size_t)VN_GOLDEN_PIXELS * sizeof(vn_u32));
    if (scalar_pixels == (vn_u32*)0 || backend_pixels == (vn_u32*)0) {
        (void)fprintf(stderr, "test_runtime_golden allocation failed\n");
        free(scalar_pixels);
        free(backend_pixels);
        return 1;
    }

    compared_count = 0;
    exit_code = 0;
    for (i = 0u; i < (unsigned int)(sizeof(k_golden_scenes) / sizeof(k_golden_scenes[0])); ++i) {
        vn_u32 scalar_crc;
        const char* actual_backend;
        int rc;

        scalar_crc = 0u;
        actual_backend = "none";
        rc = run_scene_once(k_golden_scenes[i].scene_name,
                            "scalar",
                            &scalar_crc,
                            &actual_backend,
                            scalar_pixels,
                            VN_GOLDEN_PIXELS);
        if (rc != VN_OK) {
            (void)fprintf(stderr,
                          "scene=%s scalar rc=%d\n",
                          k_golden_scenes[i].scene_name,
                          rc);
            exit_code = 1;
            break;
        }
        if (strcmp(actual_backend, "scalar") != 0) {
            (void)fprintf(stderr,
                          "scene=%s scalar requested got=%s\n",
                          k_golden_scenes[i].scene_name,
                          actual_backend);
            exit_code = 1;
            break;
        }
        if (scalar_crc != k_golden_scenes[i].expected_crc) {
            (void)fprintf(stderr,
                          "scene=%s scalar golden mismatch expected=0x%08X got=0x%08X\n",
                          k_golden_scenes[i].scene_name,
                          (unsigned int)k_golden_scenes[i].expected_crc,
                          (unsigned int)scalar_crc);
            exit_code = 1;
            break;
        }

        (void)printf("test_runtime_golden scalar scene=%s crc=0x%08X\n",
                     k_golden_scenes[i].scene_name,
                     (unsigned int)scalar_crc);

        if (check_optional_backend(&k_golden_scenes[i],
                                   "avx2",
                                   scalar_pixels,
                                   backend_pixels,
                                   &compared_count) != 0) {
            exit_code = 1;
            break;
        }
        if (check_optional_backend(&k_golden_scenes[i],
                                   "neon",
                                   scalar_pixels,
                                   backend_pixels,
                                   &compared_count) != 0) {
            exit_code = 1;
            break;
        }
        if (check_optional_backend(&k_golden_scenes[i],
                                   "rvv",
                                   scalar_pixels,
                                   backend_pixels,
                                   &compared_count) != 0) {
            exit_code = 1;
            break;
        }
    }

    if (exit_code == 0) {
        (void)printf("test_runtime_golden ok compared=%d\n", compared_count);
    }
    free(scalar_pixels);
    free(backend_pixels);
    return exit_code;
}
