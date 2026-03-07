#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vn_runtime.h"
#include "vn_error.h"
#include "../../src/core/build_config.h"

#define VN_GOLDEN_WIDTH 600u
#define VN_GOLDEN_HEIGHT 800u
#define VN_GOLDEN_PIXELS (VN_GOLDEN_WIDTH * VN_GOLDEN_HEIGHT)
#define VN_GOLDEN_OPTIONAL_MAX_MISMATCH_PERCENT 1u
#define VN_GOLDEN_OPTIONAL_MAX_CHANNEL_DIFF 8u

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

typedef struct {
    unsigned int max_mismatch_percent;
    unsigned int max_channel_diff;
} GoldenTolerancePolicy;

typedef struct {
    vn_u32 mismatch_count;
    unsigned int max_channel_diff;
    vn_u32 first_mismatch_index;
    vn_u32 first_expected_pixel;
    vn_u32 first_actual_pixel;
} PixelDiffStats;

static const GoldenScene k_golden_scenes[] = {
    { "S0", 0x58C8928Bu },
    { "S1", 0x80D7F175u },
    { "S2", 0x587BC5A4u },
    { "S3", 0x0BC0160Fu },
    { "S10", 0xC9A161B9u }
};

static const GoldenTolerancePolicy k_optional_backend_policy = {
    VN_GOLDEN_OPTIONAL_MAX_MISMATCH_PERCENT,
    VN_GOLDEN_OPTIONAL_MAX_CHANNEL_DIFF
};

static const char* golden_artifact_dir(void) {
    const char* value;

    value = getenv("VN_GOLDEN_ARTIFACT_DIR");
    if (value == (const char*)0 || value[0] == '\0') {
        return (const char*)0;
    }
    return value;
}

static int build_artifact_path(char* out_path,
                               size_t out_path_cap,
                               const char* file_name) {
    const char* dir;
    size_t dir_len;
    size_t file_len;

    if (out_path == (char*)0 || out_path_cap == 0u || file_name == (const char*)0) {
        return 1;
    }

    dir = golden_artifact_dir();
    if (dir == (const char*)0) {
        file_len = strlen(file_name);
        if (file_len + 1u > out_path_cap) {
            return 1;
        }
        memcpy(out_path, file_name, file_len + 1u);
        return 0;
    }

    dir_len = strlen(dir);
    file_len = strlen(file_name);
    if (dir_len + file_len + 2u > out_path_cap) {
        return 1;
    }

    memcpy(out_path, dir, dir_len);
    out_path[dir_len] = '/';
    memcpy(out_path + dir_len + 1u, file_name, file_len + 1u);
    return 0;
}

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

static double mismatch_percent_for_stats(const PixelDiffStats* stats,
                                         vn_u32 pixel_count) {
    if (stats == (const PixelDiffStats*)0 || pixel_count == 0u) {
        return 0.0;
    }
    return (100.0 * (double)stats->mismatch_count) / (double)pixel_count;
}

static void collect_pixel_diff_stats(const vn_u32* expected_pixels,
                                     const vn_u32* actual_pixels,
                                     vn_u32 pixel_count,
                                     PixelDiffStats* out_stats) {
    vn_u32 i;

    if (out_stats == (PixelDiffStats*)0) {
        return;
    }

    out_stats->mismatch_count = 0u;
    out_stats->max_channel_diff = 0u;
    out_stats->first_mismatch_index = pixel_count;
    out_stats->first_expected_pixel = 0u;
    out_stats->first_actual_pixel = 0u;

    if (expected_pixels == (const vn_u32*)0 ||
        actual_pixels == (const vn_u32*)0 ||
        pixel_count == 0u) {
        return;
    }

    for (i = 0u; i < pixel_count; ++i) {
        if (expected_pixels[i] != actual_pixels[i]) {
            unsigned int diff_r;
            unsigned int diff_g;
            unsigned int diff_b;
            unsigned int local_max;

            if (out_stats->mismatch_count == 0u) {
                out_stats->first_mismatch_index = i;
                out_stats->first_expected_pixel = expected_pixels[i];
                out_stats->first_actual_pixel = actual_pixels[i];
            }

            out_stats->mismatch_count += 1u;
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
            if (local_max > out_stats->max_channel_diff) {
                out_stats->max_channel_diff = local_max;
            }
        }
    }
}

static int pixel_diff_within_policy(const PixelDiffStats* stats,
                                    vn_u32 pixel_count,
                                    const GoldenTolerancePolicy* policy) {
    double mismatch_percent;

    if (stats == (const PixelDiffStats*)0 ||
        policy == (const GoldenTolerancePolicy*)0 ||
        pixel_count == 0u) {
        return 0;
    }
    if (stats->mismatch_count == 0u) {
        return 1;
    }

    mismatch_percent = mismatch_percent_for_stats(stats, pixel_count);
    if (mismatch_percent >= (double)policy->max_mismatch_percent) {
        return 0;
    }
    if (stats->max_channel_diff > policy->max_channel_diff) {
        return 0;
    }
    return 1;
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

static int write_diff_summary(const char* path,
                              const char* status,
                              const char* scene_name,
                              const char* backend_name,
                              vn_u32 expected_crc,
                              vn_u32 actual_crc,
                              const PixelDiffStats* stats,
                              vn_u32 pixel_count,
                              const GoldenTolerancePolicy* policy,
                              const char* expected_path,
                              const char* actual_path,
                              const char* diff_path) {
    FILE* fp;
    double mismatch_percent;
    vn_u32 first_x;
    vn_u32 first_y;

    if (path == (const char*)0 ||
        status == (const char*)0 ||
        scene_name == (const char*)0 ||
        backend_name == (const char*)0 ||
        stats == (const PixelDiffStats*)0 ||
        policy == (const GoldenTolerancePolicy*)0 ||
        expected_path == (const char*)0 ||
        actual_path == (const char*)0 ||
        diff_path == (const char*)0) {
        return 1;
    }

    fp = fopen(path, "wb");
    if (fp == (FILE*)0) {
        return 1;
    }

    mismatch_percent = mismatch_percent_for_stats(stats, pixel_count);
    (void)fprintf(fp, "status=%s\n", status);
    (void)fprintf(fp, "scene=%s\n", scene_name);
    (void)fprintf(fp, "backend=%s\n", backend_name);
    (void)fprintf(fp, "expected_crc=0x%08X\n", (unsigned int)expected_crc);
    (void)fprintf(fp, "actual_crc=0x%08X\n", (unsigned int)actual_crc);
    (void)fprintf(fp, "crc_match=%s\n", expected_crc == actual_crc ? "yes" : "no");
    (void)fprintf(fp, "pixel_count=%u\n", (unsigned int)pixel_count);
    (void)fprintf(fp, "mismatch_count=%u\n", (unsigned int)stats->mismatch_count);
    (void)fprintf(fp, "mismatch_percent=%.4f\n", mismatch_percent);
    (void)fprintf(fp, "max_channel_diff=%u\n", stats->max_channel_diff);
    (void)fprintf(fp,
                  "threshold_rule=mismatch_percent<%u max_channel_diff<=%u\n",
                  policy->max_mismatch_percent,
                  policy->max_channel_diff);
    if (stats->mismatch_count != 0u) {
        first_x = stats->first_mismatch_index % VN_GOLDEN_WIDTH;
        first_y = stats->first_mismatch_index / VN_GOLDEN_WIDTH;
        (void)fprintf(fp, "first_mismatch_x=%u\n", (unsigned int)first_x);
        (void)fprintf(fp, "first_mismatch_y=%u\n", (unsigned int)first_y);
        (void)fprintf(fp,
                      "first_expected_pixel=0x%08X\n",
                      (unsigned int)stats->first_expected_pixel);
        (void)fprintf(fp,
                      "first_actual_pixel=0x%08X\n",
                      (unsigned int)stats->first_actual_pixel);
    }
    (void)fprintf(fp, "expected_ppm=%s\n", expected_path);
    (void)fprintf(fp, "actual_ppm=%s\n", actual_path);
    (void)fprintf(fp, "diff_ppm=%s\n", diff_path);
    (void)fclose(fp);
    return 0;
}

static void emit_compare_artifacts(const char* scene_name,
                                   const char* backend_name,
                                   const vn_u32* expected_pixels,
                                   const vn_u32* actual_pixels,
                                   vn_u32 expected_crc,
                                   vn_u32 actual_crc,
                                   const PixelDiffStats* stats,
                                   const GoldenTolerancePolicy* policy,
                                   const char* status) {
    char expected_name[128];
    char actual_name[128];
    char diff_name[128];
    char summary_name[128];
    char expected_path[512];
    char actual_path[512];
    char diff_path[512];
    char summary_path[512];
    const char* summary_expected_path;
    const char* summary_actual_path;
    const char* summary_diff_path;

    if (scene_name == (const char*)0 ||
        backend_name == (const char*)0 ||
        expected_pixels == (const vn_u32*)0 ||
        actual_pixels == (const vn_u32*)0 ||
        stats == (const PixelDiffStats*)0 ||
        policy == (const GoldenTolerancePolicy*)0 ||
        status == (const char*)0) {
        return;
    }

    (void)sprintf(expected_name,
                  "test_runtime_golden_%s_%s_expected.ppm",
                  scene_name,
                  backend_name);
    (void)sprintf(actual_name,
                  "test_runtime_golden_%s_%s_actual.ppm",
                  scene_name,
                  backend_name);
    (void)sprintf(diff_name,
                  "test_runtime_golden_%s_%s_diff.ppm",
                  scene_name,
                  backend_name);
    (void)sprintf(summary_name,
                  "test_runtime_golden_%s_%s_summary.txt",
                  scene_name,
                  backend_name);
    if (build_artifact_path(expected_path, sizeof(expected_path), expected_name) != 0 ||
        build_artifact_path(actual_path, sizeof(actual_path), actual_name) != 0 ||
        build_artifact_path(diff_path, sizeof(diff_path), diff_name) != 0 ||
        build_artifact_path(summary_path, sizeof(summary_path), summary_name) != 0) {
        (void)fprintf(stderr,
                      "scene=%s backend=%s artifact path too long\n",
                      scene_name,
                      backend_name);
        return;
    }

    summary_expected_path = "not-written";
    summary_actual_path = "not-written";
    summary_diff_path = "not-written";

    if (stats->mismatch_count != 0u) {
        if (write_ppm_from_pixels(expected_path,
                                  expected_pixels,
                                  VN_GOLDEN_WIDTH,
                                  VN_GOLDEN_HEIGHT) == 0 &&
            write_ppm_from_pixels(actual_path,
                                  actual_pixels,
                                  VN_GOLDEN_WIDTH,
                                  VN_GOLDEN_HEIGHT) == 0 &&
            write_ppm_diff(diff_path,
                           expected_pixels,
                           actual_pixels,
                           VN_GOLDEN_WIDTH,
                           VN_GOLDEN_HEIGHT) == 0) {
            summary_expected_path = expected_path;
            summary_actual_path = actual_path;
            summary_diff_path = diff_path;
        } else {
            (void)fprintf(stderr,
                          "scene=%s backend=%s failed to write diff PPM artifacts\n",
                          scene_name,
                          backend_name);
        }
    }

    if (write_diff_summary(summary_path,
                           status,
                           scene_name,
                           backend_name,
                           expected_crc,
                           actual_crc,
                           stats,
                           VN_GOLDEN_PIXELS,
                           policy,
                           summary_expected_path,
                           summary_actual_path,
                           summary_diff_path) != 0) {
        (void)fprintf(stderr,
                      "scene=%s backend=%s failed to write diff summary %s\n",
                      scene_name,
                      backend_name,
                      summary_path);
        return;
    }

    (void)printf("test_runtime_golden wrote summary=%s\n", summary_path);
    if (stats->mismatch_count != 0u && strcmp(summary_expected_path, "not-written") != 0) {
        (void)printf("test_runtime_golden wrote artifacts expected=%s actual=%s diff=%s\n",
                     expected_path,
                     actual_path,
                     diff_path);
    }
}

static int run_scene_once(const char* scene_name,
                          const char* backend_name,
                          vn_u32 perf_flags,
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
    cfg.perf_flags = perf_flags;

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
    PixelDiffStats stats;
    double mismatch_percent;
    int within_tolerance;
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
                        VN_RUNTIME_PERF_DEFAULT_FLAGS,
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

    collect_pixel_diff_stats(expected_pixels,
                             actual_pixels,
                             VN_GOLDEN_PIXELS,
                             &stats);
    mismatch_percent = mismatch_percent_for_stats(&stats, VN_GOLDEN_PIXELS);
    within_tolerance = pixel_diff_within_policy(&stats,
                                                VN_GOLDEN_PIXELS,
                                                &k_optional_backend_policy);

    if (stats.mismatch_count == 0u) {
        if (backend_crc != golden->expected_crc) {
            emit_compare_artifacts(golden->scene_name,
                                   backend_name,
                                   expected_pixels,
                                   actual_pixels,
                                   golden->expected_crc,
                                   backend_crc,
                                   &stats,
                                   &k_optional_backend_policy,
                                   "failed");
            (void)fprintf(stderr,
                          "scene=%s backend=%s exact pixels but crc mismatch expected=0x%08X got=0x%08X\n",
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

    emit_compare_artifacts(golden->scene_name,
                           backend_name,
                           expected_pixels,
                           actual_pixels,
                           golden->expected_crc,
                           backend_crc,
                           &stats,
                           &k_optional_backend_policy,
                           within_tolerance != 0 ? "tolerated" : "failed");

    if (within_tolerance == 0) {
        (void)fprintf(stderr,
                      "scene=%s backend=%s pixel mismatch count=%u mismatch_percent=%.4f max_channel_diff=%u threshold=mismatch_percent<%u max_channel_diff<=%u\n",
                      golden->scene_name,
                      backend_name,
                      (unsigned int)stats.mismatch_count,
                      mismatch_percent,
                      stats.max_channel_diff,
                      k_optional_backend_policy.max_mismatch_percent,
                      k_optional_backend_policy.max_channel_diff);
        return 1;
    }

    *out_compared_count += 1;
    (void)printf("test_runtime_golden tolerated scene=%s backend=%s crc=0x%08X mismatch_count=%u mismatch_percent=%.4f max_channel_diff=%u\n",
                 golden->scene_name,
                 backend_name,
                 (unsigned int)backend_crc,
                 (unsigned int)stats.mismatch_count,
                 mismatch_percent,
                 stats.max_channel_diff);
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
                            VN_RUNTIME_PERF_DEFAULT_FLAGS,
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

        {
            vn_u32 dirty_crc;
            const char* dirty_backend;
            PixelDiffStats dirty_stats;

            dirty_crc = 0u;
            dirty_backend = "none";
            rc = run_scene_once(k_golden_scenes[i].scene_name,
                                "scalar",
                                VN_RUNTIME_PERF_DEFAULT_FLAGS | VN_RUNTIME_PERF_DIRTY_TILE,
                                &dirty_crc,
                                &dirty_backend,
                                backend_pixels,
                                VN_GOLDEN_PIXELS);
            if (rc != VN_OK) {
                (void)fprintf(stderr,
                              "scene=%s scalar dirty rc=%d\n",
                              k_golden_scenes[i].scene_name,
                              rc);
                exit_code = 1;
                break;
            }
            if (strcmp(dirty_backend, "scalar") != 0) {
                (void)fprintf(stderr,
                              "scene=%s scalar dirty requested got=%s\n",
                              k_golden_scenes[i].scene_name,
                              dirty_backend);
                exit_code = 1;
                break;
            }
            collect_pixel_diff_stats(scalar_pixels,
                                     backend_pixels,
                                     VN_GOLDEN_PIXELS,
                                     &dirty_stats);
            if (dirty_crc != scalar_crc || dirty_stats.mismatch_count != 0u) {
                emit_compare_artifacts(k_golden_scenes[i].scene_name,
                                       "scalar_dirty",
                                       scalar_pixels,
                                       backend_pixels,
                                       scalar_crc,
                                       dirty_crc,
                                       &dirty_stats,
                                       &k_optional_backend_policy,
                                       "failed");
                (void)fprintf(stderr,
                              "scene=%s scalar dirty mismatch baseline=0x%08X dirty=0x%08X mismatches=%u\n",
                              k_golden_scenes[i].scene_name,
                              (unsigned int)scalar_crc,
                              (unsigned int)dirty_crc,
                              (unsigned int)dirty_stats.mismatch_count);
                exit_code = 1;
                break;
            }
            (void)printf("test_runtime_golden scalar_dirty scene=%s crc=0x%08X\n",
                         k_golden_scenes[i].scene_name,
                         (unsigned int)dirty_crc);
        }

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
