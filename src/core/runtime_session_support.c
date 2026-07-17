#include <stdlib.h>
#include <string.h>

#include "runtime_internal.h"

vn_u16 legacy_scene_script_res_id(vn_u32 scene_id) {
    if (scene_id == VN_SCENE_S1) {
        return 1u;
    }
    if (scene_id == VN_SCENE_S2) {
        return 2u;
    }
    if (scene_id == VN_SCENE_S3) {
        return 3u;
    }
    if (scene_id == VN_SCENE_S10) {
        return 4u;
    }
    return 0u;
}

int load_script_resource(const VNPak* pak,
                         vn_u16 resource_id,
                         vn_u8** out_buf,
                         vn_u32* out_size) {
    const ResourceEntry* entry;
    vn_u8* script_buf;
    vn_u32 read_size;
    int rc;

    if (pak == (const VNPak*)0 || out_buf == (vn_u8**)0 || out_size == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }

    *out_buf = (vn_u8*)0;
    *out_size = 0u;

    entry = vnpak_get(pak, (vn_u32)resource_id);
    if (entry == (const ResourceEntry*)0 ||
        entry->type != VN_RESOURCE_TYPE_SCRIPT ||
        entry->data_size == 0u) {
        return VN_E_FORMAT;
    }

    script_buf = (vn_u8*)malloc((size_t)entry->data_size);
    if (script_buf == (vn_u8*)0) {
        return VN_E_NOMEM;
    }

    rc = vnpak_read_resource(pak,
                             (vn_u32)resource_id,
                             script_buf,
                             entry->data_size,
                             &read_size);
    if (rc != VN_OK) {
        free(script_buf);
        return rc;
    }
    if (read_size != entry->data_size) {
        free(script_buf);
        return VN_E_IO;
    }

    *out_buf = script_buf;
    *out_size = read_size;
    return VN_OK;
}

void state_reset_frame_events(VNRuntimeState* state) {
    state->se_id = 0u;
    state->choice_count = 0u;
    state->choice_text_id = 0u;
}

void state_from_vm(VNRuntimeState* state, VNState* vm) {
    state->text_id = vm_current_text_id(vm);
    state->text_speed_ms = vm_current_text_speed_ms(vm);
    state->vm_waiting = (vn_u32)vm_is_waiting(vm);
    state->vm_ended = (vn_u32)vm_is_ended(vm);
    state->vm_error = (vn_u32)vm_has_error(vm);
    state->bgm_id = (vn_u32)vm_current_bgm_id(vm);
    state->bgm_loop = (vn_u32)vm_current_bgm_loop(vm);
    state->se_id = (vn_u32)vm_take_se_id(vm);
    state->choice_count = (vn_u32)vm_last_choice_count(vm);
    state->choice_text_id = (vn_u32)vm_last_choice_text_id(vm);
    state->choice_selected_index = (vn_u32)vm_last_choice_selected_index(vm);
}

void state_init_defaults(VNRuntimeState* state) {
    vn_u32 i;
    state->frame_index = 0u;
    state->clear_color = 200u;
    state->scene_id = VN_SCENE_S0;
    state->resource_count = 0u;
    state->text_id = 0u;
    state->text_speed_ms = 0u;
    state->vm_waiting = 0u;
    state->vm_ended = 0u;
    state->vm_error = 0u;
    state->vm_fade_active = 0u;
    state->fade_layer_mask = 0u;
    state->fade_alpha = 0u;
    state->fade_duration_ms = 0u;
    state->bgm_id = 0u;
    state->bgm_loop = 0u;
    state->se_id = 0u;
    state->choice_count = 0u;
    state->choice_text_id = 0u;
    state->choice_selected_index = 0u;
    state->content_mode = 0u;
    state->base_width = 0u;
    state->base_height = 0u;
    state->render_width = 0u;
    state->render_height = 0u;
    state->background_texture_id = VN_VM_TEXTURE_NONE;
    state->previous_background_texture_id = VN_VM_TEXTURE_NONE;
    state->background_active = 0u;
    state->previous_background_active = 0u;
    state->background_transition_alpha = 255u;
    state->background_transition_active = 0u;
    for (i = 0u; i < VN_RUNTIME_VISUAL_LAYER_MAX; ++i) {
        state->visual_layers[i].texture_id = VN_VM_TEXTURE_NONE;
        state->visual_layers[i].x = 0;
        state->visual_layers[i].y = 0;
        state->visual_layers[i].width = 0u;
        state->visual_layers[i].height = 0u;
        state->visual_layers[i].layer = (vn_u8)(i + 1u);
        state->visual_layers[i].active = 0u;
    }
}

void background_player_init(BackgroundPlayer* background) {
    if (background == (BackgroundPlayer*)0) {
        return;
    }
    background->seen_serial = 0u;
    background->previous_texture_id = VN_VM_TEXTURE_NONE;
    background->texture_id = VN_VM_TEXTURE_NONE;
    background->duration_ms = 0u;
    background->elapsed_ms = 0u;
    background->active = 0u;
}

void background_player_step(BackgroundPlayer* background, const VNState* vm, vn_u32 dt_ms) {
    vn_u32 serial;

    if (background == (BackgroundPlayer*)0 || vm == (const VNState*)0) {
        return;
    }
    serial = vm_background_serial(vm);
    if (serial != background->seen_serial) {
        background->seen_serial = serial;
        background->previous_texture_id = vm_previous_background_texture_id(vm);
        background->texture_id = vm_background_texture_id(vm);
        background->duration_ms = vm_background_duration_ms(vm);
        background->elapsed_ms = 0u;
        background->active = (background->duration_ms > 0u &&
                              background->previous_texture_id != background->texture_id) ? 1u : 0u;
    }
    if (background->active != 0u) {
        if (dt_ms >= (vn_u32)background->duration_ms - background->elapsed_ms) {
            background->elapsed_ms = (vn_u32)background->duration_ms;
            background->active = 0u;
        } else {
            background->elapsed_ms += dt_ms;
        }
    }
}

static int state_validate_image(const VNPak* pak, vn_u16 texture_id, const ResourceEntry** out_entry) {
    const ResourceEntry* entry;

    if (out_entry != (const ResourceEntry**)0) {
        *out_entry = (const ResourceEntry*)0;
    }
    if (texture_id == VN_VM_TEXTURE_NONE) {
        return VN_OK;
    }
    entry = vnpak_get(pak, (vn_u32)texture_id);
    if (entry == (const ResourceEntry*)0 ||
        entry->type != VN_RESOURCE_TYPE_IMAGE ||
        entry->width == 0u || entry->height == 0u) {
        return VN_E_FORMAT;
    }
    if (out_entry != (const ResourceEntry**)0) {
        *out_entry = entry;
    }
    return VN_OK;
}

int state_apply_content_visuals(VNRuntimeState* state,
                                const VNState* vm,
                                const BackgroundPlayer* background,
                                const VNPak* pak) {
    vn_u32 i;
    int rc;

    if (state == (VNRuntimeState*)0 || vm == (const VNState*)0 ||
        background == (const BackgroundPlayer*)0 || pak == (const VNPak*)0) {
        return VN_E_INVALID_ARG;
    }
    state->content_mode = 1u;
    state->background_texture_id = background->texture_id;
    state->previous_background_texture_id = background->previous_texture_id;
    state->background_active = (background->texture_id != VN_VM_TEXTURE_NONE) ? 1u : 0u;
    state->previous_background_active = (background->active != 0u &&
                                         background->previous_texture_id != VN_VM_TEXTURE_NONE) ? 1u : 0u;
    state->background_transition_active = background->active;
    if (background->duration_ms == 0u || background->active == 0u) {
        state->background_transition_alpha = 255u;
    } else {
        state->background_transition_alpha = (vn_u8)((background->elapsed_ms * 255u) /
                                                     (vn_u32)background->duration_ms);
    }
    rc = state_validate_image(pak, state->background_texture_id, (const ResourceEntry**)0);
    if (rc != VN_OK) {
        return rc;
    }
    rc = state_validate_image(pak, state->previous_background_texture_id, (const ResourceEntry**)0);
    if (rc != VN_OK) {
        return rc;
    }

    for (i = 0u; i < VN_RUNTIME_VISUAL_LAYER_MAX; ++i) {
        const VNVMSpriteLayer* vm_layer;
        const ResourceEntry* entry;
        VNRuntimeVisualLayer* layer;

        vm_layer = vm_sprite_layer(vm, i);
        layer = &state->visual_layers[i];
        layer->layer = (vn_u8)(i + 1u);
        if (vm_layer == (const VNVMSpriteLayer*)0 || vm_layer->active == 0u) {
            layer->active = 0u;
            layer->texture_id = VN_VM_TEXTURE_NONE;
            layer->width = 0u;
            layer->height = 0u;
            continue;
        }
        rc = state_validate_image(pak, vm_layer->texture_id, &entry);
        if (rc != VN_OK || entry == (const ResourceEntry*)0) {
            return (rc != VN_OK) ? rc : VN_E_FORMAT;
        }
        layer->texture_id = vm_layer->texture_id;
        layer->x = vm_layer->x;
        layer->y = vm_layer->y;
        layer->width = entry->width;
        layer->height = entry->height;
        layer->active = 1u;
    }
    return VN_OK;
}

void state_apply_fade(VNRuntimeState* state, const FadePlayer* fade) {
    if (state == (VNRuntimeState*)0 || fade == (const FadePlayer*)0) {
        return;
    }
    state->fade_layer_mask = (vn_u32)fade->layer_mask;
    state->fade_alpha = (vn_u32)fade->alpha_current;
    if ((vn_u32)fade->duration_ms > fade->elapsed_ms) {
        state->fade_duration_ms = (vn_u32)fade->duration_ms - fade->elapsed_ms;
    } else {
        state->fade_duration_ms = 0u;
    }
    state->vm_fade_active = (fade->active != 0 || fade->alpha_current != 0u) ? 1u : 0u;
}

void runtime_result_write(const VNRuntimeSession* session, VNRunResult* out_result) {
    if (session == (const VNRuntimeSession*)0 || out_result == (VNRunResult*)0) {
        return;
    }
    out_result->frames_executed = session->frames_executed;
    out_result->text_id = session->state.text_id;
    out_result->vm_waiting = session->state.vm_waiting;
    out_result->vm_ended = session->state.vm_ended;
    out_result->vm_error = session->state.vm_error;
    out_result->fade_alpha = session->state.fade_alpha;
    out_result->fade_remain_ms = session->state.fade_duration_ms;
    out_result->bgm_id = session->state.bgm_id;
    out_result->se_id = session->state.se_id;
    out_result->choice_count = session->state.choice_count;
    out_result->choice_selected_index = session->state.choice_selected_index;
    out_result->choice_text_id = session->state.choice_text_id;
    out_result->op_count = session->last_op_count;
    out_result->backend_name = renderer_backend_name();
    out_result->perf_flags_effective = session->perf_flags;
    out_result->frame_reuse_hits = session->frame_reuse_hits;
    out_result->frame_reuse_misses = session->frame_reuse_misses;
    out_result->op_cache_hits = session->op_cache_hits;
    out_result->op_cache_misses = session->op_cache_misses;
    out_result->dirty_tile_count = session->dirty_tile_count;
    out_result->dirty_rect_count = session->dirty_rect_count;
    out_result->dirty_full_redraw = session->dirty_full_redraw;
    out_result->dirty_tile_frames = session->dirty_tile_frames;
    out_result->dirty_tile_total = session->dirty_tile_total;
    out_result->dirty_rect_total = session->dirty_rect_total;
    out_result->dirty_full_redraws = session->dirty_full_redraws;
    out_result->render_width = session->renderer_cfg.width;
    out_result->render_height = session->renderer_cfg.height;
    out_result->dynamic_resolution_tier = vn_dynres_get_current_tier(&session->dynamic_resolution);
    out_result->dynamic_resolution_switches = vn_dynres_get_switch_count(&session->dynamic_resolution);
}

void vn_runtime_frame_view_init(VNRuntimeFrameView* out_view) {
    if (out_view == (VNRuntimeFrameView*)0) {
        return;
    }
    (void)memset(out_view, 0, sizeof(*out_view));
    out_view->struct_size = (vn_u32)sizeof(*out_view);
    out_view->version = VN_RUNTIME_FRAME_VIEW_VERSION;
}

int vn_runtime_session_get_frame_view(const VNRuntimeSession* session,
                                      VNRuntimeFrameView* out_view) {
    const vn_u32* pixels;
    vn_u32 width;
    vn_u32 height;
    int rc;

    if (out_view == (VNRuntimeFrameView*)0) {
        return VN_E_INVALID_ARG;
    }
    if (out_view->struct_size < (vn_u32)sizeof(*out_view) ||
        out_view->version != VN_RUNTIME_FRAME_VIEW_VERSION) {
        return VN_E_INVALID_ARG;
    }
    vn_runtime_frame_view_init(out_view);
    if (session == (const VNRuntimeSession*)0) {
        return VN_E_INVALID_ARG;
    }
    if (session->renderer_ready == VN_FALSE || session->frame_ready == VN_FALSE) {
        return VN_E_RENDER_STATE;
    }
    pixels = (const vn_u32*)0;
    width = 0u;
    height = 0u;
    rc = renderer_get_framebuffer(&pixels, &width, &height);
    if (rc != VN_OK || pixels == (const vn_u32*)0) {
        return (rc != VN_OK) ? rc : VN_E_RENDER_STATE;
    }
    if (width != (vn_u32)session->renderer_cfg.width ||
        height != (vn_u32)session->renderer_cfg.height ||
        (height != 0u && width > 0xFFFFFFFFu / height)) {
        return VN_E_RENDER_STATE;
    }
    out_view->pixels = pixels;
    out_view->pixel_count = width * height;
    out_view->stride_pixels = width;
    out_view->width = (vn_u16)width;
    out_view->height = (vn_u16)height;
    out_view->pixel_format = VN_RUNTIME_PIXEL_FORMAT_ARGB8888_U32;
    return VN_OK;
}

void runtime_session_cleanup(VNRuntimeSession* session) {
    if (session == (VNRuntimeSession*)0) {
        return;
    }
    if (session->renderer_ready != VN_FALSE) {
        renderer_shutdown();
        session->renderer_ready = VN_FALSE;
        session->frame_ready = VN_FALSE;
    }
    if (session->texture_store_ready != VN_FALSE) {
        vn_texture_source_unbind();
        runtime_texture_store_destroy(&session->texture_store);
        session->texture_store_ready = VN_FALSE;
    }
    keyboard_disable(&session->keyboard);
    if (session->dirty_bits != (vn_u32*)0) {
        free(session->dirty_bits);
        session->dirty_bits = (vn_u32*)0;
    }
    if (session->vm_ready != VN_FALSE) {
        free(session->script_buf);
        session->script_buf = (vn_u8*)0;
        session->script_size = 0u;
        session->vm_ready = VN_FALSE;
    }
    if (session->pak_opened == VN_TRUE) {
        vnpak_close(&session->pak);
        session->pak_opened = VN_FALSE;
    }
}
