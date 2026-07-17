#ifndef VN_FRONTEND_H
#define VN_FRONTEND_H

#include "vn_types.h"
#include "vn_backend.h"

#define VN_RUNTIME_VISUAL_LAYER_MAX 8u

typedef struct {
    vn_u16 texture_id;
    vn_i16 x;
    vn_i16 y;
    vn_u16 width;
    vn_u16 height;
    vn_u8 layer;
    vn_u8 active;
} VNRuntimeVisualLayer;

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
    vn_u32 content_mode;
    vn_u16 base_width;
    vn_u16 base_height;
    vn_u16 render_width;
    vn_u16 render_height;
    vn_u16 background_texture_id;
    vn_u16 previous_background_texture_id;
    vn_u8 background_active;
    vn_u8 previous_background_active;
    vn_u8 background_transition_alpha;
    vn_u8 background_transition_active;
    VNRuntimeVisualLayer visual_layers[VN_RUNTIME_VISUAL_LAYER_MAX];
} VNRuntimeState;

#define VN_SCENE_S0 0u
#define VN_SCENE_S1 1u
#define VN_SCENE_S2 2u
#define VN_SCENE_S3 3u
#define VN_SCENE_S10 10u

int build_render_ops(const VNRuntimeState* state, VNRenderOp* out_ops, vn_u32* io_count);

#endif
