#ifndef VN_FRONTEND_H
#define VN_FRONTEND_H

#include "vn_types.h"
#include "vn_backend.h"

typedef struct {
    vn_u32 frame_index;
    vn_u32 clear_color;
    vn_u32 scene_id;
    vn_u32 resource_count;
} VNRuntimeState;

#define VN_SCENE_S0 0u
#define VN_SCENE_S1 1u
#define VN_SCENE_S2 2u
#define VN_SCENE_S3 3u

int build_render_ops(const VNRuntimeState* state, VNRenderOp* out_ops, vn_u32* io_count);

#endif
