#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vn_renderer.h"
#include "vn_error.h"
#include "../../src/core/platform.h"

#define VN_KERNEL_COUNT 6u

typedef struct {
    const char* name;
    VNRenderOp setup_ops[1];
    vn_u32 setup_op_count;
    VNRenderOp ops[1];
    vn_u32 op_count;
    vn_u32 pixels;
} VNKernelSpec;

typedef struct {
    const char* backend_name;
    vn_u16 width;
    vn_u16 height;
    vn_u32 iterations;
    vn_u32 warmup;
    const char* csv_path;
} VNBenchConfig;

static int vn_parse_u32(const char* text, vn_u32* out_value) {
    unsigned long value;
    char* end_ptr;

    if (text == (const char*)0 || out_value == (vn_u32*)0 || text[0] == '\0') {
        return VN_FALSE;
    }
    value = strtoul(text, &end_ptr, 10);
    if (end_ptr == text || *end_ptr != '\0') {
        return VN_FALSE;
    }
    if (value > 0xFFFFFFFFul) {
        return VN_FALSE;
    }
    *out_value = (vn_u32)value;
    return VN_TRUE;
}

static int vn_parse_resolution(const char* text, vn_u16* out_width, vn_u16* out_height) {
    const char* sep;
    size_t width_len;
    char width_buf[16];
    vn_u32 width_value;
    vn_u32 height_value;

    if (text == (const char*)0 || out_width == (vn_u16*)0 || out_height == (vn_u16*)0) {
        return VN_FALSE;
    }
    sep = strchr(text, 'x');
    if (sep == (const char*)0) {
        sep = strchr(text, 'X');
    }
    if (sep == (const char*)0) {
        return VN_FALSE;
    }
    width_len = (size_t)(sep - text);
    if (width_len == 0u || width_len >= sizeof(width_buf)) {
        return VN_FALSE;
    }
    (void)memcpy(width_buf, text, width_len);
    width_buf[width_len] = '\0';
    if (vn_parse_u32(width_buf, &width_value) == VN_FALSE) {
        return VN_FALSE;
    }
    if (vn_parse_u32(sep + 1, &height_value) == VN_FALSE) {
        return VN_FALSE;
    }
    if (width_value == 0u || width_value > 65535u || height_value == 0u || height_value > 65535u) {
        return VN_FALSE;
    }
    *out_width = (vn_u16)width_value;
    *out_height = (vn_u16)height_value;
    return VN_TRUE;
}

static void vn_fill_clear_op(VNRenderOp* op, vn_u8 gray) {
    if (op == (VNRenderOp*)0) {
        return;
    }
    op->op = VN_OP_CLEAR;
    op->layer = 0u;
    op->tex_id = 0u;
    op->x = 0;
    op->y = 0;
    op->w = 0u;
    op->h = 0u;
    op->alpha = gray;
    op->flags = 0u;
}

static void vn_fill_fade_op(VNRenderOp* op, vn_u8 alpha) {
    if (op == (VNRenderOp*)0) {
        return;
    }
    op->op = VN_OP_FADE;
    op->layer = 3u;
    op->tex_id = 0u;
    op->x = 0;
    op->y = 0;
    op->w = 0u;
    op->h = 0u;
    op->alpha = alpha;
    op->flags = 1u;
}

static void vn_fill_sprite_op(VNRenderOp* op,
                              vn_i16 x,
                              vn_i16 y,
                              vn_u16 w,
                              vn_u16 h,
                              vn_u8 alpha,
                              vn_u8 flags) {
    if (op == (VNRenderOp*)0) {
        return;
    }
    op->op = VN_OP_SPRITE;
    op->layer = 1u;
    op->tex_id = 11u;
    op->x = x;
    op->y = y;
    op->w = w;
    op->h = h;
    op->alpha = alpha;
    op->flags = flags;
}

static void vn_fill_text_op(VNRenderOp* op,
                            vn_i16 x,
                            vn_i16 y,
                            vn_u16 w,
                            vn_u16 h,
                            vn_u8 alpha,
                            vn_u8 flags) {
    if (op == (VNRenderOp*)0) {
        return;
    }
    op->op = VN_OP_TEXT;
    op->layer = 2u;
    op->tex_id = 101u;
    op->x = x;
    op->y = y;
    op->w = w;
    op->h = h;
    op->alpha = alpha;
    op->flags = flags;
}

static void vn_init_kernel_specs(VNKernelSpec* specs, vn_u16 width, vn_u16 height) {
    vn_u32 full_pixels;

    if (specs == (VNKernelSpec*)0) {
        return;
    }

    full_pixels = (vn_u32)width * (vn_u32)height;

    specs[0].name = "clear_full";
    specs[0].setup_op_count = 0u;
    vn_fill_clear_op(&specs[0].ops[0], 48u);
    specs[0].op_count = 1u;
    specs[0].pixels = full_pixels;

    specs[1].name = "fade_full_alpha160";
    vn_fill_clear_op(&specs[1].setup_ops[0], 48u);
    specs[1].setup_op_count = 1u;
    vn_fill_fade_op(&specs[1].ops[0], 160u);
    specs[1].op_count = 1u;
    specs[1].pixels = full_pixels;

    specs[2].name = "sprite_scene_opaque";
    vn_fill_clear_op(&specs[2].setup_ops[0], 48u);
    specs[2].setup_op_count = 1u;
    vn_fill_sprite_op(&specs[2].ops[0], 40, 110, 128u, 128u, 255u, 1u);
    specs[2].op_count = 1u;
    specs[2].pixels = 128u * 128u;

    specs[3].name = "text_scene_opaque";
    vn_fill_clear_op(&specs[3].setup_ops[0], 48u);
    specs[3].setup_op_count = 1u;
    vn_fill_text_op(&specs[3].ops[0], 24, 40, 320u, 36u, 255u, 3u);
    specs[3].op_count = 1u;
    specs[3].pixels = 320u * 36u;

    specs[4].name = "sprite_full_opaque";
    vn_fill_clear_op(&specs[4].setup_ops[0], 48u);
    specs[4].setup_op_count = 1u;
    vn_fill_sprite_op(&specs[4].ops[0], 0, 0, width, height, 255u, 1u);
    specs[4].op_count = 1u;
    specs[4].pixels = full_pixels;

    specs[5].name = "sprite_full_alpha180";
    vn_fill_clear_op(&specs[5].setup_ops[0], 48u);
    specs[5].setup_op_count = 1u;
    vn_fill_sprite_op(&specs[5].ops[0], 0, 0, width, height, 180u, 1u);
    specs[5].op_count = 1u;
    specs[5].pixels = full_pixels;
}

static int vn_backend_flags_from_name(const char* backend_name, vn_u32* out_flags) {
    if (backend_name == (const char*)0 || out_flags == (vn_u32*)0) {
        return VN_FALSE;
    }
    if (strcmp(backend_name, "scalar") == 0) {
        *out_flags = VN_RENDERER_FLAG_FORCE_SCALAR;
        return VN_TRUE;
    }
    if (strcmp(backend_name, "avx2") == 0) {
        *out_flags = VN_RENDERER_FLAG_FORCE_AVX2;
        return VN_TRUE;
    }
    if (strcmp(backend_name, "avx2_asm") == 0) {
        *out_flags = VN_RENDERER_FLAG_FORCE_AVX2_ASM;
        return VN_TRUE;
    }
    if (strcmp(backend_name, "neon") == 0) {
        *out_flags = VN_RENDERER_FLAG_FORCE_NEON;
        return VN_TRUE;
    }
    if (strcmp(backend_name, "rvv") == 0) {
        *out_flags = VN_RENDERER_FLAG_FORCE_RVV;
        return VN_TRUE;
    }
    if (strcmp(backend_name, "auto") == 0) {
        *out_flags = VN_RENDERER_FLAG_SIMD;
        return VN_TRUE;
    }
    return VN_FALSE;
}

static int vn_double_compare(const void* lhs, const void* rhs) {
    const double* a;
    const double* b;

    a = (const double*)lhs;
    b = (const double*)rhs;
    if (*a < *b) {
        return -1;
    }
    if (*a > *b) {
        return 1;
    }
    return 0;
}

static double vn_percentile_95(const double* samples, vn_u32 count) {
    double* sorted;
    unsigned long rank;
    double value;

    if (samples == (const double*)0 || count == 0u) {
        return 0.0;
    }

    sorted = (double*)malloc((size_t)count * sizeof(double));
    if (sorted == (double*)0) {
        return 0.0;
    }
    (void)memcpy(sorted, samples, (size_t)count * sizeof(double));
    qsort(sorted, (size_t)count, sizeof(double), vn_double_compare);

    rank = ((unsigned long)count * 95ul + 99ul) / 100ul;
    if (rank == 0ul) {
        rank = 1ul;
    }
    if (rank > (unsigned long)count) {
        rank = (unsigned long)count;
    }
    value = sorted[rank - 1ul];
    free(sorted);
    return value;
}

static int vn_run_setup_ops(const VNKernelSpec* spec) {
    if (spec == (const VNKernelSpec*)0 || spec->setup_op_count == 0u) {
        return VN_OK;
    }
    renderer_begin_frame();
    renderer_submit(spec->setup_ops, spec->setup_op_count);
    renderer_end_frame();
    return VN_OK;
}

static void vn_write_csv_header(FILE* fp) {
    if (fp == (FILE*)0) {
        return;
    }
    (void)fprintf(fp,
                  "kernel,backend,samples,warmup,width,height,pixels,avg_ms,p95_ms,min_ms,max_ms,mpix_per_s,host_os,host_arch,host_compiler\n");
}

static int vn_bench_kernel(FILE* fp,
                           const VNBenchConfig* cfg,
                           const VNKernelSpec* spec) {
    double* samples;
    double total_ms;
    double min_ms;
    double max_ms;
    double avg_ms;
    double p95_ms;
    double mpix_per_s;
    vn_u32 i;

    if (fp == (FILE*)0 || cfg == (const VNBenchConfig*)0 || spec == (const VNKernelSpec*)0) {
        return VN_E_INVALID_ARG;
    }
    if (cfg->iterations == 0u) {
        return VN_E_INVALID_ARG;
    }

    samples = (double*)malloc((size_t)cfg->iterations * sizeof(double));
    if (samples == (double*)0) {
        return VN_E_NOMEM;
    }

    for (i = 0u; i < cfg->warmup; ++i) {
        (void)vn_run_setup_ops(spec);
        renderer_begin_frame();
        renderer_submit(spec->ops, spec->op_count);
        renderer_end_frame();
    }

    total_ms = 0.0;
    min_ms = 0.0;
    max_ms = 0.0;
    for (i = 0u; i < cfg->iterations; ++i) {
        double t0;
        double t1;
        double dt;

        (void)vn_run_setup_ops(spec);
        t0 = vn_platform_now_ms();
        renderer_begin_frame();
        renderer_submit(spec->ops, spec->op_count);
        renderer_end_frame();
        t1 = vn_platform_now_ms();
        dt = t1 - t0;
        samples[i] = dt;
        total_ms += dt;
        if (i == 0u || dt < min_ms) {
            min_ms = dt;
        }
        if (i == 0u || dt > max_ms) {
            max_ms = dt;
        }
    }

    avg_ms = total_ms / (double)cfg->iterations;
    p95_ms = vn_percentile_95(samples, cfg->iterations);
    if (avg_ms > 0.0) {
        mpix_per_s = ((double)spec->pixels / avg_ms) / 1000.0;
    } else {
        mpix_per_s = 0.0;
    }

    (void)fprintf(fp,
                  "%s,%s,%u,%u,%u,%u,%u,%.6f,%.6f,%.6f,%.6f,%.6f,%s,%s,%s\n",
                  spec->name,
                  cfg->backend_name,
                  cfg->iterations,
                  cfg->warmup,
                  (unsigned int)cfg->width,
                  (unsigned int)cfg->height,
                  (unsigned int)spec->pixels,
                  avg_ms,
                  p95_ms,
                  min_ms,
                  max_ms,
                  mpix_per_s,
                  vn_platform_host_os_name(),
                  vn_platform_host_arch_name(),
                  vn_platform_host_compiler_name());
    (void)fprintf(stderr,
                  "[kernel-bench] backend=%s kernel=%s avg=%.6fms p95=%.6fms mpix_per_s=%.3f\n",
                  cfg->backend_name,
                  spec->name,
                  avg_ms,
                  p95_ms,
                  mpix_per_s);

    free(samples);
    return VN_OK;
}

static void vn_bench_config_init(VNBenchConfig* cfg) {
    if (cfg == (VNBenchConfig*)0) {
        return;
    }
    cfg->backend_name = "scalar";
    cfg->width = 600u;
    cfg->height = 800u;
    cfg->iterations = 128u;
    cfg->warmup = 16u;
    cfg->csv_path = (const char*)0;
}

static int vn_parse_args(int argc, char** argv, VNBenchConfig* cfg) {
    int i;

    if (cfg == (VNBenchConfig*)0) {
        return VN_E_INVALID_ARG;
    }

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--backend") == 0 && (i + 1) < argc) {
            cfg->backend_name = argv[i + 1];
            i += 1;
        } else if (strcmp(argv[i], "--resolution") == 0 && (i + 1) < argc) {
            if (vn_parse_resolution(argv[i + 1], &cfg->width, &cfg->height) == VN_FALSE) {
                return VN_E_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--iterations") == 0 && (i + 1) < argc) {
            if (vn_parse_u32(argv[i + 1], &cfg->iterations) == VN_FALSE || cfg->iterations == 0u) {
                return VN_E_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--warmup") == 0 && (i + 1) < argc) {
            if (vn_parse_u32(argv[i + 1], &cfg->warmup) == VN_FALSE) {
                return VN_E_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--csv") == 0 && (i + 1) < argc) {
            cfg->csv_path = argv[i + 1];
            i += 1;
        } else {
            return VN_E_INVALID_ARG;
        }
    }

    return VN_OK;
}

int main(int argc, char** argv) {
    VNBenchConfig cfg;
    VNKernelSpec specs[VN_KERNEL_COUNT];
    RendererConfig renderer_cfg;
    FILE* fp;
    vn_u32 flags;
    vn_u32 i;
    int rc;

    vn_bench_config_init(&cfg);
    rc = vn_parse_args(argc, argv, &cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr,
                      "usage: %s [--backend scalar|avx2|avx2_asm|neon|rvv|auto] [--resolution 600x800] [--iterations N] [--warmup N] [--csv path]\n",
                      argv[0]);
        return 2;
    }
    if (vn_backend_flags_from_name(cfg.backend_name, &flags) == VN_FALSE) {
        (void)fprintf(stderr, "unknown backend: %s\n", cfg.backend_name);
        return 2;
    }

    fp = stdout;
    if (cfg.csv_path != (const char*)0) {
        fp = fopen(cfg.csv_path, "w");
        if (fp == (FILE*)0) {
            (void)fprintf(stderr, "failed to open csv path: %s\n", cfg.csv_path);
            return 1;
        }
    }

    renderer_cfg.width = cfg.width;
    renderer_cfg.height = cfg.height;
    renderer_cfg.flags = flags;

    rc = renderer_init(&renderer_cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "renderer_init failed rc=%d backend=%s\n", rc, cfg.backend_name);
        if (fp != stdout) {
            (void)fclose(fp);
        }
        return 1;
    }

    vn_init_kernel_specs(specs, cfg.width, cfg.height);
    vn_write_csv_header(fp);
    for (i = 0u; i < VN_KERNEL_COUNT; ++i) {
        rc = vn_bench_kernel(fp, &cfg, &specs[i]);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "kernel bench failed rc=%d kernel=%s\n", rc, specs[i].name);
            renderer_shutdown();
            if (fp != stdout) {
                (void)fclose(fp);
            }
            return 1;
        }
    }

    renderer_shutdown();
    if (fp != stdout) {
        (void)fclose(fp);
    }
    return 0;
}
