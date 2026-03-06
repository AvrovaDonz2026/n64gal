#include <stdio.h>
#include <string.h>

#include "vn_renderer.h"

vn_u32 vn_scalar_backend_debug_frame_crc32(void);
vn_u32 vn_avx2_backend_debug_frame_crc32(void);

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

static int render_one(vn_u32 flags, vn_u32* out_crc) {
    RendererConfig cfg;
    VNRenderOp ops[8];
    vn_u32 op_count;
    int rc;
    const char* name;

    if (out_crc == (vn_u32*)0) {
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
    } else if ((flags & VN_RENDERER_FLAG_FORCE_AVX2) != 0u) {
        if (strcmp(name, "avx2") == 0) {
            *out_crc = vn_avx2_backend_debug_frame_crc32();
        } else if (strcmp(name, "scalar") == 0) {
            *out_crc = 0u;
            renderer_shutdown();
            return 2;
        } else {
            (void)fprintf(stderr, "forced avx2 unexpected backend=%s\n", name);
            renderer_shutdown();
            return 1;
        }
    } else {
        *out_crc = 0u;
    }

    renderer_shutdown();
    return 0;
}

int main(void) {
    vn_u32 scalar_crc;
    vn_u32 avx2_crc;
    int rc;

    scalar_crc = 0u;
    avx2_crc = 0u;

    rc = render_one(VN_RENDERER_FLAG_FORCE_SCALAR, &scalar_crc);
    if (rc != 0) {
        return 1;
    }
    if (scalar_crc == 0u) {
        (void)fprintf(stderr, "scalar crc missing\n");
        return 1;
    }

    rc = render_one(VN_RENDERER_FLAG_FORCE_AVX2, &avx2_crc);
    if (rc == 2) {
        (void)printf("test_backend_consistency skipped (no avx2 support)\n");
        return 0;
    }
    if (rc != 0) {
        return 1;
    }

    if (avx2_crc == 0u) {
        (void)fprintf(stderr, "avx2 crc missing\n");
        return 1;
    }

    if (scalar_crc != avx2_crc) {
        (void)fprintf(stderr, "backend crc mismatch scalar=0x%08X avx2=0x%08X\n",
                      (unsigned int)scalar_crc,
                      (unsigned int)avx2_crc);
        return 1;
    }

    (void)printf("test_backend_consistency ok crc=0x%08X\n", (unsigned int)scalar_crc);
    return 0;
}
