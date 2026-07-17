#include "vn_frontend.h"
#include "vn_error.h"

static vn_u32 vn_scene_required_ops(const VNRuntimeState* state) {
    if (state->scene_id == VN_SCENE_S10) {
        return 6u;
    }
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

static void vn_fill_sprite_variant(VNRenderOp* op,
                                   vn_u8 layer,
                                   vn_u16 tex_id,
                                   vn_i16 x_pos,
                                   vn_i16 y_pos,
                                   vn_u16 width,
                                   vn_u16 height,
                                   vn_u8 alpha,
                                   vn_u8 flags) {
    op->op = VN_OP_SPRITE;
    op->layer = layer;
    op->tex_id = tex_id;
    op->x = x_pos;
    op->y = y_pos;
    op->w = width;
    op->h = height;
    op->alpha = alpha;
    op->flags = flags;
}

static void vn_fill_sprite(VNRenderOp* op, const VNRuntimeState* state) {
    vn_i16 x_pos;
    vn_i16 y_pos;

    x_pos = (vn_i16)(40 + (state->frame_index % 160u));
    y_pos = (vn_i16)(110 + (state->scene_id * 20u));
    vn_fill_sprite_variant(op,
                           1u,
                           (vn_u16)(10u + (vn_u16)state->scene_id),
                           x_pos,
                           y_pos,
                           128u,
                           128u,
                           255u,
                           (vn_u8)(state->se_id != 0u ? 1u : 0u));
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
    if (state->choice_selected_index > 0u) {
        text_flags = (vn_u8)(text_flags | 8u);
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
    if (state->scene_id == VN_SCENE_S10) {
        op->x = 28;
        op->y = 208;
        op->w = 448u;
        op->h = 48u;
    }
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

static void vn_fill_scene10_overlay(VNRenderOp* out_ops,
                                    const VNRuntimeState* state,
                                    vn_u32* io_write_count) {
    vn_i16 sprite_x;
    vn_u8 sprite_flags;

    sprite_x = (vn_i16)(40 + (state->frame_index % 160u));
    sprite_flags = (vn_u8)(state->se_id != 0u ? 1u : 0u);

    vn_fill_sprite_variant(&out_ops[3],
                           3u,
                           30u,
                           sprite_x,
                           132,
                           504u,
                           332u,
                           208u,
                           sprite_flags);
    vn_fill_sprite_variant(&out_ops[4],
                           4u,
                           31u,
                           sprite_x,
                           392,
                           432u,
                           208u,
                           176u,
                           sprite_flags);
    vn_fill_fade(&out_ops[5], state);
    *io_write_count = 6u;
}

static int vn_build_legacy_render_ops(const VNRuntimeState* state,
                                      VNRenderOp* out_ops,
                                      vn_u32* io_count) {
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
    if (state->scene_id == VN_SCENE_S10) {
        vn_fill_scene10_overlay(out_ops, state, &write_count);
    } else if (required > 3u) {
        vn_fill_fade(&out_ops[write_count], state);
        write_count += 1u;
    }

    *io_count = write_count;
    return VN_OK;
}

static vn_i16 vn_content_scale_pos(vn_i16 value, vn_u16 base, vn_u16 render) {
    long scaled;
    if (base == 0u || render == 0u || base == render) {
        return value;
    }
    scaled = ((long)value * (long)render) / (long)base;
    if (scaled < -32768L) {
        scaled = -32768L;
    }
    if (scaled > 32767L) {
        scaled = 32767L;
    }
    return (vn_i16)scaled;
}

static vn_u16 vn_content_scale_extent(vn_u16 value, vn_u16 base, vn_u16 render) {
    vn_u32 scaled;
    if (value == 0u || base == 0u || render == 0u || base == render) {
        return value;
    }
    scaled = ((vn_u32)value * (vn_u32)render) / (vn_u32)base;
    if (scaled == 0u) {
        scaled = 1u;
    }
    if (scaled > 65535u) {
        scaled = 65535u;
    }
    return (vn_u16)scaled;
}

static void vn_fill_resource_texture(VNRenderOp* op,
                                     vn_u8 layer,
                                     vn_u16 texture_id,
                                     vn_i16 x,
                                     vn_i16 y,
                                     vn_u16 width,
                                     vn_u16 height,
                                     vn_u8 alpha) {
    op->op = VN_OP_SPRITE;
    op->layer = layer;
    op->tex_id = texture_id;
    op->x = x;
    op->y = y;
    op->w = width;
    op->h = height;
    op->alpha = alpha;
    op->flags = VN_OP_FLAG_RESOURCE_TEXTURE;
}

static vn_u32 vn_content_required_ops(const VNRuntimeState* state) {
    vn_u32 required;
    vn_u32 i;

    required = 1u;
    if (state->previous_background_active != 0u) {
        required += 1u;
    }
    if (state->background_active != 0u) {
        required += 1u;
    }
    for (i = 0u; i < VN_RUNTIME_VISUAL_LAYER_MAX; ++i) {
        if (state->visual_layers[i].active != 0u) {
            required += 1u;
        }
    }
    if (state->text_id != 0u) {
        required += 1u;
    }
    if (state->vm_fade_active != 0u) {
        required += 1u;
    }
    return required;
}

static int vn_build_content_render_ops(const VNRuntimeState* state,
                                       VNRenderOp* out_ops,
                                       vn_u32* io_count) {
    vn_u32 required;
    vn_u32 write_count;
    vn_u32 i;
    vn_u16 render_width;
    vn_u16 render_height;

    required = vn_content_required_ops(state);
    if (*io_count < required) {
        *io_count = required;
        return VN_E_NOMEM;
    }
    render_width = (state->render_width != 0u) ? state->render_width : state->base_width;
    render_height = (state->render_height != 0u) ? state->render_height : state->base_height;
    if (render_width == 0u || render_height == 0u) {
        return VN_E_INVALID_ARG;
    }

    vn_fill_clear(&out_ops[0], state->clear_color, state->resource_count);
    write_count = 1u;
    if (state->previous_background_active != 0u) {
        vn_u8 previous_alpha;
        previous_alpha = 255u;
        if (state->background_transition_active != 0u) {
            previous_alpha = (vn_u8)(255u - state->background_transition_alpha);
        }
        vn_fill_resource_texture(&out_ops[write_count],
                                 0u,
                                 state->previous_background_texture_id,
                                 0,
                                 0,
                                 render_width,
                                 render_height,
                                 previous_alpha);
        if (state->background_active != 0u &&
            state->background_transition_active != 0u) {
            out_ops[write_count].flags = (vn_u8)(VN_OP_FLAG_RESOURCE_TEXTURE |
                                                  VN_OP_FLAG_RESOURCE_CROSSFADE_FROM);
        }
        write_count += 1u;
    }
    if (state->background_active != 0u) {
        vn_u8 current_alpha;
        current_alpha = (state->background_transition_active != 0u) ?
                        state->background_transition_alpha : 255u;
        vn_fill_resource_texture(&out_ops[write_count],
                                 0u,
                                 state->background_texture_id,
                                 0,
                                 0,
                                 render_width,
                                 render_height,
                                 current_alpha);
        if (state->previous_background_active != 0u &&
            state->background_transition_active != 0u) {
            out_ops[write_count].flags = (vn_u8)(VN_OP_FLAG_RESOURCE_TEXTURE |
                                                  VN_OP_FLAG_RESOURCE_CROSSFADE_TO);
        }
        write_count += 1u;
    }

    for (i = 0u; i < VN_RUNTIME_VISUAL_LAYER_MAX; ++i) {
        const VNRuntimeVisualLayer* layer;
        layer = &state->visual_layers[i];
        if (layer->active == 0u) {
            continue;
        }
        vn_fill_resource_texture(&out_ops[write_count],
                                 layer->layer,
                                 layer->texture_id,
                                 vn_content_scale_pos(layer->x, state->base_width, render_width),
                                 vn_content_scale_pos(layer->y, state->base_height, render_height),
                                 vn_content_scale_extent(layer->width, state->base_width, render_width),
                                 vn_content_scale_extent(layer->height, state->base_height, render_height),
                                 255u);
        write_count += 1u;
    }

    if (state->text_id != 0u) {
        VNRenderOp* text_op;
        text_op = &out_ops[write_count];
        vn_fill_text(text_op, state);
        text_op->layer = 9u;
        text_op->x = vn_content_scale_pos(24, state->base_width, render_width);
        text_op->y = vn_content_scale_pos((vn_i16)((int)state->base_height - 80),
                                          state->base_height,
                                          render_height);
        text_op->w = vn_content_scale_extent(320u, state->base_width, render_width);
        text_op->h = vn_content_scale_extent(36u, state->base_height, render_height);
        write_count += 1u;
    }
    if (state->vm_fade_active != 0u) {
        vn_fill_fade(&out_ops[write_count], state);
        out_ops[write_count].layer = 10u;
        write_count += 1u;
    }

    *io_count = write_count;
    return VN_OK;
}

int build_render_ops(const VNRuntimeState* state, VNRenderOp* out_ops, vn_u32* io_count) {
    if (state == (const VNRuntimeState*)0 ||
        out_ops == (VNRenderOp*)0 ||
        io_count == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }
    if (state->content_mode != 0u) {
        return vn_build_content_render_ops(state, out_ops, io_count);
    }
    return vn_build_legacy_render_ops(state, out_ops, io_count);
}
