#ifndef VN_DIRTY_TILES_H
#define VN_DIRTY_TILES_H

#include "vn_types.h"
#include "vn_backend.h"

#define VN_DIRTY_TILE_SIZE 8u
#define VN_DIRTY_RECT_MAX 128u
#define VN_DIRTY_OP_CAP 16u

typedef struct {
    vn_u16 x;
    vn_u16 y;
    vn_u16 w;
    vn_u16 h;
} VNDirtyRect;

typedef struct {
    vn_u32 valid;
    vn_u16 width;
    vn_u16 height;
    vn_u16 tiles_x;
    vn_u16 tiles_y;
    vn_u32 bit_word_count;
    vn_u32 dirty_tile_count;
    vn_u32 dirty_rect_count;
    vn_u32 full_redraw;
    VNDirtyRect rects[VN_DIRTY_RECT_MAX];
} VNDirtyPlan;

typedef struct {
    vn_u32 valid;
    vn_u16 width;
    vn_u16 height;
    vn_u16 tiles_x;
    vn_u16 tiles_y;
    vn_u32 bit_word_count;
    vn_u32* dirty_bits;
    vn_u32 prev_op_count;
    VNRenderOp prev_ops[VN_DIRTY_OP_CAP];
    VNDirtyRect prev_bounds[VN_DIRTY_OP_CAP];
} VNDirtyPlannerState;

vn_u32 vn_dirty_word_count(vn_u16 width, vn_u16 height);
void vn_dirty_plan_reset(VNDirtyPlan* plan);
void vn_dirty_planner_init(VNDirtyPlannerState* state,
                           vn_u16 width,
                           vn_u16 height,
                           vn_u32* dirty_bits,
                           vn_u32 bit_word_count);
void vn_dirty_planner_invalidate(VNDirtyPlannerState* state);
int vn_dirty_planner_build(VNDirtyPlannerState* state,
                           const VNRenderOp* ops,
                           vn_u32 op_count,
                           VNDirtyPlan* out_plan);
void vn_dirty_planner_commit(VNDirtyPlannerState* state,
                             const VNRenderOp* ops,
                             vn_u32 op_count);

#endif
