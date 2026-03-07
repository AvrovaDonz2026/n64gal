#include <stdio.h>
#include <string.h>

#include "../../src/frontend/dirty_tiles.h"
#include "vn_backend.h"
#include "vn_error.h"

static void init_clear(VNRenderOp* op, vn_u8 gray) {
    op->op = VN_OP_CLEAR;
    op->layer = 0u;
    op->tex_id = 0u;
    op->x = 0;
    op->y = 0;
    op->w = 0u;
    op->h = 0u;
    op->alpha = gray;
    op->flags = 1u;
}

static void init_sprite(VNRenderOp* op, vn_i16 x, vn_i16 y, vn_u16 w, vn_u16 h, vn_u16 tex_id) {
    op->op = VN_OP_SPRITE;
    op->layer = 1u;
    op->tex_id = tex_id;
    op->x = x;
    op->y = y;
    op->w = w;
    op->h = h;
    op->alpha = 255u;
    op->flags = 0u;
}

static void init_text(VNRenderOp* op, vn_i16 x, vn_i16 y, vn_u16 w, vn_u16 h, vn_u16 tex_id) {
    op->op = VN_OP_TEXT;
    op->layer = 2u;
    op->tex_id = tex_id;
    op->x = x;
    op->y = y;
    op->w = w;
    op->h = h;
    op->alpha = 255u;
    op->flags = 0u;
}

static void init_fade(VNRenderOp* op, vn_u8 alpha) {
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

int main(void) {
    VNDirtyPlannerState state;
    VNDirtyPlan plan;
    VNRenderOp ops[3];
    VNRenderOp moved_ops[3];
    VNRenderOp fade_ops[4];
    VNRenderOp post_fade_ops[4];
    vn_u32 bits[8];
    vn_u32 word_count;
    int rc;

    word_count = vn_dirty_word_count(64u, 64u);
    if (word_count > 8u || word_count == 0u) {
        (void)fprintf(stderr, "unexpected word_count=%u\n", (unsigned int)word_count);
        return 1;
    }
    (void)memset(bits, 0, sizeof(bits));
    vn_dirty_planner_init(&state, 64u, 64u, bits, word_count);

    init_clear(&ops[0], 200u);
    init_sprite(&ops[1], 8, 8, 16u, 16u, 10u);
    init_text(&ops[2], 24, 32, 16u, 8u, 100u);

    rc = vn_dirty_planner_build(&state, ops, 3u, &plan);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "initial build failed rc=%d\n", rc);
        return 1;
    }
    if (plan.full_redraw == 0u || plan.dirty_rect_count != 1u || plan.dirty_tile_count != 64u) {
        (void)fprintf(stderr,
                      "expected initial full redraw tiles=%u rects=%u full=%u\n",
                      (unsigned int)plan.dirty_tile_count,
                      (unsigned int)plan.dirty_rect_count,
                      (unsigned int)plan.full_redraw);
        return 1;
    }

    vn_dirty_planner_commit(&state, ops, 3u);
    rc = vn_dirty_planner_build(&state, ops, 3u, &plan);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "repeat build failed rc=%d\n", rc);
        return 1;
    }
    if (plan.full_redraw != 0u || plan.dirty_tile_count != 0u || plan.dirty_rect_count != 0u) {
        (void)fprintf(stderr,
                      "expected no dirty tiles on identical frame tiles=%u rects=%u full=%u\n",
                      (unsigned int)plan.dirty_tile_count,
                      (unsigned int)plan.dirty_rect_count,
                      (unsigned int)plan.full_redraw);
        return 1;
    }

    (void)memcpy(moved_ops, ops, sizeof(ops));
    moved_ops[1].x = 16;
    rc = vn_dirty_planner_build(&state, moved_ops, 3u, &plan);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "moved build failed rc=%d\n", rc);
        return 1;
    }
    if (plan.full_redraw != 0u || plan.dirty_tile_count == 0u || plan.dirty_rect_count == 0u) {
        (void)fprintf(stderr,
                      "expected partial dirty plan tiles=%u rects=%u full=%u\n",
                      (unsigned int)plan.dirty_tile_count,
                      (unsigned int)plan.dirty_rect_count,
                      (unsigned int)plan.full_redraw);
        return 1;
    }

    (void)memcpy(moved_ops, ops, sizeof(ops));
    moved_ops[0].alpha = 180u;
    rc = vn_dirty_planner_build(&state, moved_ops, 3u, &plan);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "clear-change build failed rc=%d\n", rc);
        return 1;
    }
    if (plan.full_redraw == 0u) {
        (void)fprintf(stderr, "expected full redraw on clear change\n");
        return 1;
    }

    fade_ops[0] = ops[0];
    fade_ops[1] = ops[1];
    fade_ops[2] = ops[2];
    init_fade(&fade_ops[3], 128u);
    rc = vn_dirty_planner_build(&state, fade_ops, 4u, &plan);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "fade build failed rc=%d\n", rc);
        return 1;
    }
    if (plan.full_redraw == 0u) {
        (void)fprintf(stderr, "expected full redraw on fade op\n");
        return 1;
    }

    vn_dirty_planner_commit_full_redraw(&state, fade_ops, 4u);
    (void)memcpy(post_fade_ops, fade_ops, sizeof(fade_ops));
    init_sprite(&post_fade_ops[3], 40, 40, 8u, 8u, 21u);

    rc = vn_dirty_planner_build(&state, post_fade_ops, 4u, &plan);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "post-fade transition build failed rc=%d\n", rc);
        return 1;
    }
    if (plan.full_redraw == 0u) {
        (void)fprintf(stderr, "expected full redraw on fade-to-sprite transition\n");
        return 1;
    }

    vn_dirty_planner_commit_full_redraw(&state, post_fade_ops, 4u);
    rc = vn_dirty_planner_build(&state, post_fade_ops, 4u, &plan);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "post-fade steady build failed rc=%d\n", rc);
        return 1;
    }
    if (plan.full_redraw != 0u || plan.dirty_tile_count != 0u || plan.dirty_rect_count != 0u) {
        (void)fprintf(stderr,
                      "expected lazy prev-bounds refresh to restore empty dirty plan tiles=%u rects=%u full=%u\n",
                      (unsigned int)plan.dirty_tile_count,
                      (unsigned int)plan.dirty_rect_count,
                      (unsigned int)plan.full_redraw);
        return 1;
    }

    post_fade_ops[3].x = 48;
    rc = vn_dirty_planner_build(&state, post_fade_ops, 4u, &plan);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "post-fade moved build failed rc=%d\n", rc);
        return 1;
    }
    if (plan.full_redraw != 0u || plan.dirty_tile_count == 0u || plan.dirty_rect_count == 0u) {
        (void)fprintf(stderr,
                      "expected partial dirty plan after lazy refresh tiles=%u rects=%u full=%u\n",
                      (unsigned int)plan.dirty_tile_count,
                      (unsigned int)plan.dirty_rect_count,
                      (unsigned int)plan.full_redraw);
        return 1;
    }

    (void)printf("test_dirty_tiles ok\n");
    return 0;
}
