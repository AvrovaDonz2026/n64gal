#include "vn_frontend.h"
#include "vn_error.h"

static vn_u32 vn_scene_required_ops(const VNRuntimeState* state) {
    if (state->vm_fade_active != 0u || state->vm_waiting != 0u) {
        return 4u;
    }
    if (state->scene_id == VN_SCENE_S1 || state->scene_id == VN_SCENE_S3) {
        return 4u;
    }
    return 3u;
}

static void vn_fill_clear(VNRenderOp* op, vn_u32 clear_color, vn_u32 resource_count) {
    op->op = VN_OP_CLEAR;
    op->layer = 0;
    op->tex_id = 0;
    op->x = 0;
    op->y = 0;
    op->w = 0;
    op->h = 0;
    op->alpha = (vn_u8)(clear_color & 0xFFu);
    op->flags = (vn_u8)(resource_count > 0u ? 1u : 0u);
}

static void vn_fill_sprite(VNRenderOp* op, const VNRuntimeState* state) {
    vn_i16 x_pos;
    vn_i16 y_pos;

    x_pos = (vn_i16)(40 + (state->frame_index % 160u));
    y_pos = (vn_i16)(110 + (state->scene_id * 20u));

    op->op = VN_OP_SPRITE;
    op->layer = 1;
    op->tex_id = (vn_u16)(10u + (vn_u16)state->scene_id);
    op->x = x_pos;
    op->y = y_pos;
    op->w = 128u;
    op->h = 128u;
    op->alpha = 255u;
    op->flags = (vn_u8)(state->se_id != 0u ? 1u : 0u);
}

static void vn_fill_text(VNRenderOp* op, const VNRuntimeState* state) {
    vn_u8 text_flags;
    vn_u16 text_tex_id;
    text_tex_id = (vn_u16)(100u + (vn_u16)state->scene_id);
    if (state->text_id != 0u) {
        text_tex_id = state->text_id;
    }
    text_flags = 0u;
    if (state->text_speed_ms > 0u) {
        text_flags = (vn_u8)(text_flags | 1u);
    }
    if (state->choice_count > 0u) {
        text_flags = (vn_u8)(text_flags | 2u);
    }
    if (state->vm_error != 0u) {
        text_flags = (vn_u8)(text_flags | 4u);
    }

    op->op = VN_OP_TEXT;
    op->layer = 2;
    op->tex_id = text_tex_id;
    op->x = 24;
    op->y = (vn_i16)(40 + (vn_i16)(state->scene_id * 18u));
    op->w = 320u;
    op->h = 36u;
    op->alpha = (vn_u8)(state->vm_ended != 0u ? 180u : 255u);
    op->flags = text_flags;
}

static void vn_fill_fade(VNRenderOp* op, const VNRuntimeState* state) {
    vn_u32 phase;
    phase = state->frame_index & 0x3Fu;

    op->op = VN_OP_FADE;
    op->layer = 3;
    op->tex_id = 0;
    op->x = 0;
    op->y = 0;
    op->w = 0;
    op->h = 0;
    if (state->vm_fade_active != 0u) {
        op->tex_id = (vn_u16)(state->fade_layer_mask & 0xFFFFu);
        op->alpha = (vn_u8)(state->fade_alpha & 0xFFu);
        op->flags = 2u;
    } else {
        op->alpha = (vn_u8)(state->vm_waiting != 0u ? (120u + phase) : (phase * 3u));
        op->flags = (vn_u8)(state->vm_waiting != 0u ? 1u : 0u);
    }
}

int build_render_ops(const VNRuntimeState* state, VNRenderOp* out_ops, vn_u32* io_count) {
    vn_u32 max_count;
    vn_u32 required;
    vn_u32 write_count;

    if (state == (const VNRuntimeState*)0 || out_ops == (VNRenderOp*)0 || io_count == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }

    max_count = *io_count;
    required = vn_scene_required_ops(state);
    if (max_count < required) {
        *io_count = required;
        return VN_E_NOMEM;
    }
    if (max_count < 3u) {
        return VN_E_INVALID_ARG;
    }

    vn_fill_clear(&out_ops[0], state->clear_color, state->resource_count);
    vn_fill_sprite(&out_ops[1], state);
    vn_fill_text(&out_ops[2], state);

    write_count = 3u;
    if (required > 3u) {
        vn_fill_fade(&out_ops[write_count], state);
        write_count += 1u;
    }

    *io_count = write_count;
    return VN_OK;
}
