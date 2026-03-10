#include <stdio.h>
#include <string.h>

#include "vn_renderer.h"
#include "vn_error.h"
#include "../../src/frontend/dirty_tiles.h"

vn_u32 vn_scalar_backend_debug_frame_crc32(void);
vn_u32 vn_avx2_backend_debug_frame_crc32(void);
vn_u32 vn_neon_backend_debug_frame_crc32(void);
vn_u32 vn_rvv_backend_debug_frame_crc32(void);

static void fill_frame_a(VNRenderOp* ops, vn_u32* out_count) {
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
    ops[0].alpha = 70u;
    ops[0].flags = 0u;

    ops[1].op = VN_OP_SPRITE;
    ops[1].layer = 1u;
    ops[1].tex_id = 21u;
    ops[1].x = 8;
    ops[1].y = 8;
    ops[1].w = 20u;
    ops[1].h = 20u;
    ops[1].alpha = 255u;
    ops[1].flags = 1u;

    ops[2].op = VN_OP_TEXT;
    ops[2].layer = 2u;
    ops[2].tex_id = 88u;
    ops[2].x = 24;
    ops[2].y = 20;
    ops[2].w = 24u;
    ops[2].h = 16u;
    ops[2].alpha = 220u;
    ops[2].flags = 9u;

    *out_count = 3u;
}

static void fill_frame_b(VNRenderOp* ops, vn_u32* out_count) {
    fill_frame_a(ops, out_count);
    ops[1].x = 16;
    ops[2].alpha = 192u;
}

static int build_dirty_submit(const VNRenderOp* first_ops,
                              const VNRenderOp* second_ops,
                              vn_u32 op_count,
                              VNDirtyPlan* out_plan,
                              vn_u32* bits,
                              vn_u32 bit_count) {
    VNDirtyPlannerState planner;
    int rc;

    if (out_plan == (VNDirtyPlan*)0 || bits == (vn_u32*)0) {
        return 1;
    }

    vn_dirty_planner_init(&planner, 64u, 64u, bits, bit_count);
    rc = vn_dirty_planner_build(&planner, first_ops, op_count, out_plan);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "initial dirty plan failed rc=%d\n", rc);
        return 1;
    }
    vn_dirty_planner_commit(&planner, first_ops, op_count);
    rc = vn_dirty_planner_build(&planner, second_ops, op_count, out_plan);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "second dirty plan failed rc=%d\n", rc);
        return 1;
    }
    if (out_plan->full_redraw != 0u || out_plan->dirty_rect_count == 0u) {
        (void)fprintf(stderr,
                      "expected partial dirty plan full=%u rects=%u\n",
                      (unsigned int)out_plan->full_redraw,
                      (unsigned int)out_plan->dirty_rect_count);
        return 1;
    }
    return 0;
}

static int active_crc(vn_u32* out_crc) {
    const char* backend_name;

    if (out_crc == (vn_u32*)0) {
        return 1;
    }

    backend_name = renderer_backend_name();
    if (strcmp(backend_name, "scalar") == 0) {
        *out_crc = vn_scalar_backend_debug_frame_crc32();
        return 0;
    }
    if (strcmp(backend_name, "avx2") == 0 || strcmp(backend_name, "avx2_asm") == 0) {
        *out_crc = vn_avx2_backend_debug_frame_crc32();
        return 0;
    }
    if (strcmp(backend_name, "neon") == 0) {
        *out_crc = vn_neon_backend_debug_frame_crc32();
        return 0;
    }
    if (strcmp(backend_name, "rvv") == 0) {
        *out_crc = vn_rvv_backend_debug_frame_crc32();
        return 0;
    }

    (void)fprintf(stderr, "unsupported debug backend=%s\n", backend_name);
    return 1;
}

static int run_sequence(vn_u32 flags,
                        const VNRenderOp* first_ops,
                        const VNRenderOp* second_ops,
                        vn_u32 op_count,
                        const VNRenderDirtySubmit* dirty_submit,
                        int use_dirty,
                        vn_u32* out_crc,
                        char* out_backend,
                        size_t out_backend_cap) {
    RendererConfig cfg;
    int rc;
    const char* backend_name;

    if (out_crc == (vn_u32*)0) {
        return 1;
    }

    cfg.width = 64u;
    cfg.height = 64u;
    cfg.flags = flags;

    rc = renderer_init(&cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "renderer_init failed rc=%d flags=0x%X\n", rc, (unsigned int)flags);
        return 1;
    }

    backend_name = renderer_backend_name();
    if (out_backend != (char*)0 && out_backend_cap != 0u) {
        (void)strncpy(out_backend, backend_name, out_backend_cap - 1u);
        out_backend[out_backend_cap - 1u] = '\0';
    }

    renderer_begin_frame();
    renderer_submit(first_ops, op_count);
    renderer_end_frame();

    renderer_begin_frame();
    if (use_dirty != 0) {
        renderer_submit_dirty(second_ops, op_count, dirty_submit);
    } else {
        renderer_submit(second_ops, op_count);
    }
    renderer_end_frame();

    if (active_crc(out_crc) != 0) {
        renderer_shutdown();
        return 1;
    }

    renderer_shutdown();
    return 0;
}

static int compare_dirty_backend(vn_u32 flags,
                                 const char* expected_backend,
                                 const VNRenderOp* frame_a,
                                 const VNRenderOp* frame_b,
                                 const VNRenderDirtySubmit* dirty_submit,
                                 int* out_compared_count) {
    char actual_backend[16];
    vn_u32 full_crc;
    vn_u32 dirty_crc;

    if (expected_backend == (const char*)0 ||
        frame_a == (const VNRenderOp*)0 ||
        frame_b == (const VNRenderOp*)0 ||
        dirty_submit == (const VNRenderDirtySubmit*)0 ||
        out_compared_count == (int*)0) {
        return 1;
    }

    if (run_sequence(flags,
                     frame_a,
                     frame_b,
                     3u,
                     dirty_submit,
                     0,
                     &full_crc,
                     actual_backend,
                     sizeof(actual_backend)) != 0) {
        return 1;
    }

    if (strcmp(actual_backend, expected_backend) != 0) {
        if (strcmp(actual_backend, "scalar") == 0) {
            (void)printf("test_renderer_dirty_submit skipped backend=%s fallback=%s\n",
                         expected_backend,
                         actual_backend);
            return 0;
        }
        (void)fprintf(stderr,
                      "test_renderer_dirty_submit requested=%s got=%s\n",
                      expected_backend,
                      actual_backend);
        return 1;
    }

    if (run_sequence(flags,
                     frame_a,
                     frame_b,
                     3u,
                     dirty_submit,
                     1,
                     &dirty_crc,
                     (char*)0,
                     0u) != 0) {
        return 1;
    }
    if (full_crc != dirty_crc) {
        (void)fprintf(stderr,
                      "%s dirty submit mismatch full=0x%08X dirty=0x%08X\n",
                      expected_backend,
                      (unsigned int)full_crc,
                      (unsigned int)dirty_crc);
        return 1;
    }

    *out_compared_count += 1;
    (void)printf("test_renderer_dirty_submit matched backend=%s crc=0x%08X\n",
                 expected_backend,
                 (unsigned int)dirty_crc);
    return 0;
}

int main(void) {
    VNRenderOp frame_a[4];
    VNRenderOp frame_b[4];
    VNDirtyPlan plan;
    VNRenderDirtySubmit dirty_submit;
    vn_u32 bits[8];
    vn_u32 bit_count;
    int compared_count;

    fill_frame_a(frame_a, &bit_count);
    fill_frame_b(frame_b, &bit_count);

    bit_count = vn_dirty_word_count(64u, 64u);
    if (bit_count == 0u || bit_count > 8u) {
        (void)fprintf(stderr, "unexpected bit_count=%u\n", (unsigned int)bit_count);
        return 1;
    }
    (void)memset(bits, 0, sizeof(bits));
    if (build_dirty_submit(frame_a, frame_b, 3u, &plan, bits, bit_count) != 0) {
        return 1;
    }

    dirty_submit.width = plan.width;
    dirty_submit.height = plan.height;
    dirty_submit.rect_count = plan.dirty_rect_count;
    dirty_submit.full_redraw = plan.full_redraw;
    dirty_submit.rects = plan.rects;

    compared_count = 0;
    if (compare_dirty_backend(VN_RENDERER_FLAG_FORCE_SCALAR,
                              "scalar",
                              frame_a,
                              frame_b,
                              &dirty_submit,
                              &compared_count) != 0) {
        return 1;
    }
    if (compare_dirty_backend(VN_RENDERER_FLAG_FORCE_AVX2,
                              "avx2",
                              frame_a,
                              frame_b,
                              &dirty_submit,
                              &compared_count) != 0) {
        return 1;
    }
    if (compare_dirty_backend(VN_RENDERER_FLAG_FORCE_AVX2_ASM,
                              "avx2_asm",
                              frame_a,
                              frame_b,
                              &dirty_submit,
                              &compared_count) != 0) {
        return 1;
    }
    if (compare_dirty_backend(VN_RENDERER_FLAG_FORCE_NEON,
                              "neon",
                              frame_a,
                              frame_b,
                              &dirty_submit,
                              &compared_count) != 0) {
        return 1;
    }
    if (compare_dirty_backend(VN_RENDERER_FLAG_FORCE_RVV,
                              "rvv",
                              frame_a,
                              frame_b,
                              &dirty_submit,
                              &compared_count) != 0) {
        return 1;
    }

    (void)printf("test_renderer_dirty_submit ok compared=%d\n", compared_count);
    return 0;
}
