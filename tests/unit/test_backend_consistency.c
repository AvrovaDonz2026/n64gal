#include <stdio.h>
#include <string.h>

#include "vn_renderer.h"

vn_u32 vn_scalar_backend_debug_frame_crc32(void);
vn_u32 vn_avx2_backend_debug_frame_crc32(void);
vn_u32 vn_rvv_backend_debug_frame_crc32(void);

static void fill_ops(VNRenderOp* ops, vn_u32* out_count) {
    if (ops == (VNRenderOp*)0 || out_count == (vn_u32*)0) {
        return;
    }

    ops[0].op = VN_OP_CLEAR;
    ops[0].layer = 0u;
    ops[0].tex_id = 0u;
    ops[0].x = 0;
    ops[0].y = 0;
    ops[0].w = 0u;
    ops[0].h = 0u;
    ops[0].alpha = 18u;
    ops[0].flags = 0u;

    ops[1].op = VN_OP_SPRITE;
    ops[1].layer = 1u;
    ops[1].tex_id = 21u;
    ops[1].x = 40;
    ops[1].y = 60;
    ops[1].w = 220u;
    ops[1].h = 160u;
    ops[1].alpha = 255u;
    ops[1].flags = 1u;

    ops[2].op = VN_OP_TEXT;
    ops[2].layer = 2u;
    ops[2].tex_id = 111u;
    ops[2].x = -12;
    ops[2].y = 300;
    ops[2].w = 280u;
    ops[2].h = 48u;
    ops[2].alpha = 210u;
    ops[2].flags = 11u;

    ops[3].op = VN_OP_SPRITE;
    ops[3].layer = 1u;
    ops[3].tex_id = 33u;
    ops[3].x = 440;
    ops[3].y = 700;
    ops[3].w = 220u;
    ops[3].h = 180u;
    ops[3].alpha = 140u;
    ops[3].flags = 0u;

    ops[4].op = VN_OP_FADE;
    ops[4].layer = 3u;
    ops[4].tex_id = 0u;
    ops[4].x = 0;
    ops[4].y = 0;
    ops[4].w = 0u;
    ops[4].h = 0u;
    ops[4].alpha = 36u;
    ops[4].flags = 0u;

    *out_count = 5u;
}

static int render_one(vn_u32 flags, const char* expected_name, vn_u32* out_crc) {
    RendererConfig cfg;
    VNRenderOp ops[8];
    vn_u32 op_count;
    int rc;
    const char* name;

    if (expected_name == (const char*)0 || out_crc == (vn_u32*)0) {
        return 1;
    }

    cfg.width = 600u;
    cfg.height = 800u;
    cfg.flags = flags;

    fill_ops(ops, &op_count);

    rc = renderer_init(&cfg);
    if (rc != 0) {
        (void)fprintf(stderr, "renderer_init failed rc=%d flags=0x%X\n", rc, (unsigned int)flags);
        return 1;
    }

    renderer_begin_frame();
    renderer_submit(ops, op_count);
    renderer_end_frame();

    name = renderer_backend_name();
    if ((flags & VN_RENDERER_FLAG_FORCE_SCALAR) != 0u) {
        if (strcmp(name, "scalar") != 0) {
            (void)fprintf(stderr, "forced scalar mismatch backend=%s\n", name);
            renderer_shutdown();
            return 1;
        }
        *out_crc = vn_scalar_backend_debug_frame_crc32();
        renderer_shutdown();
        return 0;
    }

    if (strcmp(name, expected_name) == 0) {
        if (strcmp(expected_name, "avx2") == 0) {
            *out_crc = vn_avx2_backend_debug_frame_crc32();
        } else if (strcmp(expected_name, "rvv") == 0) {
            *out_crc = vn_rvv_backend_debug_frame_crc32();
        } else {
            (void)fprintf(stderr, "unsupported expected backend=%s\n", expected_name);
            renderer_shutdown();
            return 1;
        }
        renderer_shutdown();
        return 0;
    }

    if (strcmp(name, "scalar") == 0) {
        *out_crc = 0u;
        renderer_shutdown();
        return 2;
    }

    (void)fprintf(stderr, "forced %s unexpected backend=%s\n", expected_name, name);
    renderer_shutdown();
    return 1;
}

static int compare_backend(vn_u32 flags, const char* expected_name, const char* skip_reason, vn_u32 scalar_crc, int* out_compared) {
    vn_u32 backend_crc;
    int rc;

    if (expected_name == (const char*)0 || skip_reason == (const char*)0 || out_compared == (int*)0) {
        return 1;
    }

    backend_crc = 0u;
    rc = render_one(flags, expected_name, &backend_crc);
    if (rc == 2) {
        (void)printf("test_backend_consistency skipped (%s)\n", skip_reason);
        return 0;
    }
    if (rc != 0) {
        return 1;
    }
    if (backend_crc == 0u) {
        (void)fprintf(stderr, "%s crc missing\n", expected_name);
        return 1;
    }
    if (scalar_crc != backend_crc) {
        (void)fprintf(stderr, "backend crc mismatch scalar=0x%08X %s=0x%08X\n",
                      (unsigned int)scalar_crc,
                      expected_name,
                      (unsigned int)backend_crc);
        return 1;
    }

    *out_compared += 1;
    (void)printf("test_backend_consistency matched backend=%s crc=0x%08X\n",
                 expected_name,
                 (unsigned int)scalar_crc);
    return 0;
}

int main(void) {
    vn_u32 scalar_crc;
    int compared_count;

    scalar_crc = 0u;
    compared_count = 0;

    if (render_one(VN_RENDERER_FLAG_FORCE_SCALAR, "scalar", &scalar_crc) != 0) {
        return 1;
    }
    if (scalar_crc == 0u) {
        (void)fprintf(stderr, "scalar crc missing\n");
        return 1;
    }

    if (compare_backend(VN_RENDERER_FLAG_FORCE_AVX2, "avx2", "no avx2 support", scalar_crc, &compared_count) != 0) {
        return 1;
    }
    if (compare_backend(VN_RENDERER_FLAG_FORCE_RVV, "rvv", "no rvv support", scalar_crc, &compared_count) != 0) {
        return 1;
    }

    if (compared_count == 0) {
        (void)printf("test_backend_consistency skipped (no simd backend support)\n");
        return 0;
    }

    (void)printf("test_backend_consistency ok crc=0x%08X compared=%d\n",
                 (unsigned int)scalar_crc,
                 compared_count);
    return 0;
}
