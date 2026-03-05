#ifndef VN_FRONTEND_H
#define VN_FRONTEND_H

#include "vn_types.h"
#include "vn_backend.h"

typedef struct {
    vn_u32 frame_index;
    vn_u32 clear_color;
    vn_u32 scene_id;
    vn_u32 resource_count;
    vn_u16 text_id;
    vn_u16 text_speed_ms;
    vn_u32 vm_waiting;
    vn_u32 vm_ended;
    vn_u32 vm_error;
    vn_u32 vm_fade_active;
    vn_u32 fade_layer_mask;
    vn_u32 fade_alpha;
    vn_u32 fade_duration_ms;
    vn_u32 bgm_id;
    vn_u32 bgm_loop;
    vn_u32 se_id;
    vn_u32 choice_count;
    vn_u32 choice_text_id;
    vn_u32 choice_selected_index;
} VNRuntimeState;

#define VN_SCENE_S0 0u
#define VN_SCENE_S1 1u
#define VN_SCENE_S2 2u
#define VN_SCENE_S3 3u

int build_render_ops(const VNRuntimeState* state, VNRenderOp* out_ops, vn_u32* io_count);

#endif
