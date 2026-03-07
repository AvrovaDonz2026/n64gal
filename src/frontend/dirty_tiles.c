#include <string.h>

#include "dirty_tiles.h"
#include "vn_error.h"

#define VN_DIRTY_FULL_REDRAW_THRESHOLD_PERCENT 60u

static vn_u16 dirty_tiles_x(vn_u16 width) {
    return (vn_u16)((width + (vn_u16)(VN_DIRTY_TILE_SIZE - 1u)) / (vn_u16)VN_DIRTY_TILE_SIZE);
}

static vn_u16 dirty_tiles_y(vn_u16 height) {
    return (vn_u16)((height + (vn_u16)(VN_DIRTY_TILE_SIZE - 1u)) / (vn_u16)VN_DIRTY_TILE_SIZE);
}

static void dirty_rect_zero(VNDirtyRect* rect) {
    if (rect == (VNDirtyRect*)0) {
        return;
    }
    rect->x = 0u;
    rect->y = 0u;
    rect->w = 0u;
    rect->h = 0u;
}

static int dirty_rect_is_empty(const VNDirtyRect* rect) {
    if (rect == (const VNDirtyRect*)0) {
        return VN_TRUE;
    }
    return (rect->w == 0u || rect->h == 0u) ? VN_TRUE : VN_FALSE;
}

static void dirty_bits_clear(vn_u32* bits, vn_u32 word_count) {
    if (bits == (vn_u32*)0 || word_count == 0u) {
        return;
    }
    (void)memset(bits, 0, (size_t)word_count * sizeof(vn_u32));
}

static int dirty_mark_tile(const VNDirtyPlannerState* state,
                           vn_u32 tile_index,
                           vn_u32* marked_tile_count) {
    vn_u32 word_index;
    vn_u32 bit_index;
    vn_u32 mask;
    vn_u32 value;

    if (state == (const VNDirtyPlannerState*)0 ||
        state->dirty_bits == (vn_u32*)0 ||
        marked_tile_count == (vn_u32*)0) {
        return VN_FALSE;
    }
    word_index = tile_index >> 5;
    bit_index = tile_index & 31u;
    if (word_index >= state->bit_word_count) {
        return VN_FALSE;
    }
    mask = (vn_u32)(1u << bit_index);
    value = state->dirty_bits[word_index];
    if ((value & mask) != 0u) {
        return VN_FALSE;
    }
    state->dirty_bits[word_index] = value | mask;
    *marked_tile_count += 1u;
    return VN_TRUE;
}

static int dirty_test_tile(const VNDirtyPlannerState* state, vn_u32 tile_index) {
    vn_u32 word_index;
    vn_u32 bit_index;

    if (state == (const VNDirtyPlannerState*)0 || state->dirty_bits == (vn_u32*)0) {
        return VN_FALSE;
    }
    word_index = tile_index >> 5;
    bit_index = tile_index & 31u;
    if (word_index >= state->bit_word_count) {
        return VN_FALSE;
    }
    return ((state->dirty_bits[word_index] & (vn_u32)(1u << bit_index)) != 0u) ? VN_TRUE : VN_FALSE;
}

static int dirty_op_equal(const VNRenderOp* a, const VNRenderOp* b) {
    if (a == (const VNRenderOp*)0 || b == (const VNRenderOp*)0) {
        return VN_FALSE;
    }
    return (a->op == b->op &&
            a->layer == b->layer &&
            a->tex_id == b->tex_id &&
            a->x == b->x &&
            a->y == b->y &&
            a->w == b->w &&
            a->h == b->h &&
            a->alpha == b->alpha &&
            a->flags == b->flags) ? VN_TRUE : VN_FALSE;
}

static int dirty_compute_rect(const VNRenderOp* op,
                              vn_u16 width,
                              vn_u16 height,
                              VNDirtyRect* out_rect) {
    long x0;
    long y0;
    long x1;
    long y1;

    if (out_rect == (VNDirtyRect*)0) {
        return VN_E_INVALID_ARG;
    }
    out_rect->x = 0u;
    out_rect->y = 0u;
    out_rect->w = 0u;
    out_rect->h = 0u;
    if (op == (const VNRenderOp*)0) {
        return VN_E_INVALID_ARG;
    }
    if (op->op == VN_OP_CLEAR) {
        return VN_FALSE;
    }
    if (op->op == VN_OP_FADE) {
        out_rect->x = 0u;
        out_rect->y = 0u;
        out_rect->w = width;
        out_rect->h = height;
        return (width == 0u || height == 0u) ? VN_FALSE : VN_TRUE;
    }
    if (op->op != VN_OP_SPRITE && op->op != VN_OP_TEXT) {
        return VN_E_FORMAT;
    }

    x0 = (long)op->x;
    y0 = (long)op->y;
    x1 = x0 + (long)op->w;
    y1 = y0 + (long)op->h;

    if (x1 <= 0L || y1 <= 0L || x0 >= (long)width || y0 >= (long)height) {
        return VN_FALSE;
    }
    if (x0 < 0L) {
        x0 = 0L;
    }
    if (y0 < 0L) {
        y0 = 0L;
    }
    if (x1 > (long)width) {
        x1 = (long)width;
    }
    if (y1 > (long)height) {
        y1 = (long)height;
    }
    if (x1 <= x0 || y1 <= y0) {
        return VN_FALSE;
    }

    out_rect->x = (vn_u16)x0;
    out_rect->y = (vn_u16)y0;
    out_rect->w = (vn_u16)(x1 - x0);
    out_rect->h = (vn_u16)(y1 - y0);
    return VN_TRUE;
}

static void dirty_mark_rect(const VNDirtyPlannerState* state,
                            const VNDirtyRect* rect,
                            vn_u32* marked_tile_count) {
    vn_u32 tile_x0;
    vn_u32 tile_y0;
    vn_u32 tile_x1;
    vn_u32 tile_y1;
    vn_u32 x;
    vn_u32 y;

    if (state == (const VNDirtyPlannerState*)0 ||
        rect == (const VNDirtyRect*)0 ||
        marked_tile_count == (vn_u32*)0 ||
        dirty_rect_is_empty(rect) != VN_FALSE) {
        return;
    }

    tile_x0 = (vn_u32)(rect->x / (vn_u16)VN_DIRTY_TILE_SIZE);
    tile_y0 = (vn_u32)(rect->y / (vn_u16)VN_DIRTY_TILE_SIZE);
    tile_x1 = (vn_u32)((rect->x + rect->w - 1u) / (vn_u16)VN_DIRTY_TILE_SIZE);
    tile_y1 = (vn_u32)((rect->y + rect->h - 1u) / (vn_u16)VN_DIRTY_TILE_SIZE);

    if (tile_x1 >= state->tiles_x) {
        tile_x1 = (vn_u32)state->tiles_x - 1u;
    }
    if (tile_y1 >= state->tiles_y) {
        tile_y1 = (vn_u32)state->tiles_y - 1u;
    }

    for (y = tile_y0; y <= tile_y1; ++y) {
        for (x = tile_x0; x <= tile_x1; ++x) {
            (void)dirty_mark_tile(state,
                                  y * (vn_u32)state->tiles_x + x,
                                  marked_tile_count);
        }
    }
}

static void dirty_plan_set_full_redraw(const VNDirtyPlannerState* state, VNDirtyPlan* plan) {
    if (state == (const VNDirtyPlannerState*)0 || plan == (VNDirtyPlan*)0) {
        return;
    }
    plan->valid = 1u;
    plan->full_redraw = 1u;
    plan->dirty_tile_count = (vn_u32)state->tiles_x * (vn_u32)state->tiles_y;
    plan->dirty_rect_count = 0u;
    if (state->width != 0u && state->height != 0u) {
        plan->rects[0].x = 0u;
        plan->rects[0].y = 0u;
        plan->rects[0].w = state->width;
        plan->rects[0].h = state->height;
        plan->dirty_rect_count = 1u;
    }
}

static int dirty_collect_rects(const VNDirtyPlannerState* state, VNDirtyPlan* plan) {
    vn_u32 y;
    vn_u32 x;
    vn_u32 rect_count;

    if (state == (const VNDirtyPlannerState*)0 || plan == (VNDirtyPlan*)0) {
        return VN_E_INVALID_ARG;
    }

    rect_count = 0u;
    for (y = 0u; y < (vn_u32)state->tiles_y; ++y) {
        x = 0u;
        while (x < (vn_u32)state->tiles_x) {
            vn_u32 run_start;
            vn_u32 run_end;
            VNDirtyRect rect;
            vn_u32 pixel_end_x;
            vn_u32 pixel_end_y;

            if (dirty_test_tile(state, y * (vn_u32)state->tiles_x + x) == VN_FALSE) {
                x += 1u;
                continue;
            }

            run_start = x;
            run_end = x;
            while ((run_end + 1u) < (vn_u32)state->tiles_x &&
                   dirty_test_tile(state, y * (vn_u32)state->tiles_x + run_end + 1u) != VN_FALSE) {
                run_end += 1u;
            }

            rect.x = (vn_u16)(run_start * VN_DIRTY_TILE_SIZE);
            rect.y = (vn_u16)(y * VN_DIRTY_TILE_SIZE);
            pixel_end_x = (run_end + 1u) * VN_DIRTY_TILE_SIZE;
            if (pixel_end_x > (vn_u32)state->width) {
                pixel_end_x = (vn_u32)state->width;
            }
            pixel_end_y = (y + 1u) * VN_DIRTY_TILE_SIZE;
            if (pixel_end_y > (vn_u32)state->height) {
                pixel_end_y = (vn_u32)state->height;
            }
            rect.w = (vn_u16)(pixel_end_x - (vn_u32)rect.x);
            rect.h = (vn_u16)(pixel_end_y - (vn_u32)rect.y);

            if (rect_count > 0u) {
                VNDirtyRect* prev_rect;
                prev_rect = &plan->rects[rect_count - 1u];
                if (prev_rect->x == rect.x &&
                    prev_rect->w == rect.w &&
                    (vn_u32)prev_rect->y + (vn_u32)prev_rect->h == (vn_u32)rect.y) {
                    prev_rect->h = (vn_u16)((vn_u32)prev_rect->h + (vn_u32)rect.h);
                    x = run_end + 1u;
                    continue;
                }
            }

            if (rect_count >= VN_DIRTY_RECT_MAX) {
                return VN_E_NOMEM;
            }
            plan->rects[rect_count] = rect;
            rect_count += 1u;
            x = run_end + 1u;
        }
    }

    plan->dirty_rect_count = rect_count;
    return VN_OK;
}

vn_u32 vn_dirty_word_count(vn_u16 width, vn_u16 height) {
    vn_u32 tile_count;

    if (width == 0u || height == 0u) {
        return 0u;
    }
    tile_count = (vn_u32)dirty_tiles_x(width) * (vn_u32)dirty_tiles_y(height);
    return (tile_count + 31u) >> 5;
}

void vn_dirty_plan_reset(VNDirtyPlan* plan) {
    if (plan == (VNDirtyPlan*)0) {
        return;
    }
    (void)memset(plan, 0, sizeof(*plan));
}

void vn_dirty_planner_init(VNDirtyPlannerState* state,
                           vn_u16 width,
                           vn_u16 height,
                           vn_u32* dirty_bits,
                           vn_u32 bit_word_count) {
    if (state == (VNDirtyPlannerState*)0) {
        return;
    }
    (void)memset(state, 0, sizeof(*state));
    state->width = width;
    state->height = height;
    state->tiles_x = dirty_tiles_x(width);
    state->tiles_y = dirty_tiles_y(height);
    state->dirty_bits = dirty_bits;
    state->bit_word_count = bit_word_count;
}

static void dirty_clear_prev_bounds(VNDirtyPlannerState* state, vn_u32 start_index) {
    vn_u32 i;

    if (state == (VNDirtyPlannerState*)0) {
        return;
    }
    for (i = start_index; i < VN_DIRTY_OP_CAP; ++i) {
        dirty_rect_zero(&state->prev_bounds[i]);
    }
}

static int dirty_copy_prev_ops(VNDirtyPlannerState* state,
                               const VNRenderOp* ops,
                               vn_u32 op_count) {
    vn_u32 i;

    if (state == (VNDirtyPlannerState*)0 ||
        ops == (const VNRenderOp*)0 ||
        op_count == 0u ||
        op_count > VN_DIRTY_OP_CAP) {
        return VN_E_INVALID_ARG;
    }

    state->prev_op_count = op_count;
    for (i = 0u; i < op_count; ++i) {
        state->prev_ops[i] = ops[i];
    }
    return VN_OK;
}

static int dirty_refresh_prev_bounds(VNDirtyPlannerState* state) {
    vn_u32 i;
    int rc;

    if (state == (VNDirtyPlannerState*)0 || state->prev_op_count == 0u) {
        return VN_E_INVALID_ARG;
    }

    for (i = 0u; i < state->prev_op_count; ++i) {
        rc = dirty_compute_rect(&state->prev_ops[i], state->width, state->height, &state->prev_bounds[i]);
        if (rc < 0) {
            return rc;
        }
        if (rc == VN_FALSE) {
            dirty_rect_zero(&state->prev_bounds[i]);
        }
    }
    dirty_clear_prev_bounds(state, i);
    state->prev_bounds_valid = 1u;
    return VN_OK;
}

void vn_dirty_planner_invalidate(VNDirtyPlannerState* state) {
    if (state == (VNDirtyPlannerState*)0) {
        return;
    }
    state->valid = 0u;
    state->prev_op_count = 0u;
    state->prev_bounds_valid = 0u;
}

int vn_dirty_planner_build(VNDirtyPlannerState* state,
                           const VNRenderOp* ops,
                           vn_u32 op_count,
                           VNDirtyPlan* out_plan) {
    vn_u32 i;
    vn_u32 total_tiles;
    vn_u32 marked_tile_count;
    VNDirtyRect current_bounds;
    int rc;

    if (state == (VNDirtyPlannerState*)0 ||
        out_plan == (VNDirtyPlan*)0 ||
        (ops == (const VNRenderOp*)0 && op_count != 0u) ||
        op_count > VN_DIRTY_OP_CAP) {
        return VN_E_INVALID_ARG;
    }
    if (state->width == 0u || state->height == 0u || state->dirty_bits == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }
    if (state->bit_word_count < vn_dirty_word_count(state->width, state->height)) {
        return VN_E_INVALID_ARG;
    }

    vn_dirty_plan_reset(out_plan);
    out_plan->width = state->width;
    out_plan->height = state->height;
    out_plan->tiles_x = state->tiles_x;
    out_plan->tiles_y = state->tiles_y;
    out_plan->bit_word_count = state->bit_word_count;
    dirty_bits_clear(state->dirty_bits, state->bit_word_count);
    total_tiles = (vn_u32)state->tiles_x * (vn_u32)state->tiles_y;
    marked_tile_count = 0u;

    if (state->valid == 0u || state->prev_op_count != op_count || op_count == 0u) {
        dirty_plan_set_full_redraw(state, out_plan);
        return VN_OK;
    }
    if (ops[0].op != VN_OP_CLEAR || state->prev_ops[0].op != VN_OP_CLEAR) {
        dirty_plan_set_full_redraw(state, out_plan);
        return VN_OK;
    }
    if (dirty_op_equal(&ops[0], &state->prev_ops[0]) == VN_FALSE) {
        dirty_plan_set_full_redraw(state, out_plan);
        return VN_OK;
    }

    for (i = 1u; i < op_count; ++i) {
        if (ops[i].op == VN_OP_FADE || state->prev_ops[i].op == VN_OP_FADE) {
            dirty_plan_set_full_redraw(state, out_plan);
            return VN_OK;
        }
        if (ops[i].op != state->prev_ops[i].op) {
            dirty_plan_set_full_redraw(state, out_plan);
            return VN_OK;
        }
    }

    if (state->prev_bounds_valid == 0u) {
        rc = dirty_refresh_prev_bounds(state);
        if (rc != VN_OK) {
            dirty_plan_set_full_redraw(state, out_plan);
            return VN_OK;
        }
    }

    for (i = 1u; i < op_count; ++i) {
        if (dirty_op_equal(&ops[i], &state->prev_ops[i]) != VN_FALSE) {
            continue;
        }
        rc = dirty_compute_rect(&ops[i], state->width, state->height, &current_bounds);
        if (rc < 0) {
            dirty_plan_set_full_redraw(state, out_plan);
            return VN_OK;
        }
        if (dirty_rect_is_empty(&state->prev_bounds[i]) == VN_FALSE) {
            dirty_mark_rect(state, &state->prev_bounds[i], &marked_tile_count);
        }
        if (dirty_rect_is_empty(&current_bounds) == VN_FALSE) {
            dirty_mark_rect(state, &current_bounds, &marked_tile_count);
        }
    }

    out_plan->dirty_tile_count = marked_tile_count;
    out_plan->valid = 1u;
    if (out_plan->dirty_tile_count == 0u || total_tiles == 0u) {
        return VN_OK;
    }
    if ((out_plan->dirty_tile_count * 100u) > (total_tiles * VN_DIRTY_FULL_REDRAW_THRESHOLD_PERCENT)) {
        dirty_plan_set_full_redraw(state, out_plan);
        return VN_OK;
    }
    rc = dirty_collect_rects(state, out_plan);
    if (rc != VN_OK) {
        dirty_plan_set_full_redraw(state, out_plan);
        return VN_OK;
    }
    return VN_OK;
}

void vn_dirty_planner_commit(VNDirtyPlannerState* state,
                             const VNRenderOp* ops,
                             vn_u32 op_count) {
    int rc;

    rc = dirty_copy_prev_ops(state, ops, op_count);
    if (rc != VN_OK) {
        vn_dirty_planner_invalidate(state);
        return;
    }
    rc = dirty_refresh_prev_bounds(state);
    if (rc != VN_OK) {
        vn_dirty_planner_invalidate(state);
        return;
    }
    state->valid = 1u;
}

void vn_dirty_planner_commit_full_redraw(VNDirtyPlannerState* state,
                                         const VNRenderOp* ops,
                                         vn_u32 op_count) {
    int rc;

    rc = dirty_copy_prev_ops(state, ops, op_count);
    if (rc != VN_OK) {
        vn_dirty_planner_invalidate(state);
        return;
    }
    dirty_clear_prev_bounds(state, 0u);
    state->prev_bounds_valid = 0u;
    state->valid = 1u;
}
