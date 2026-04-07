#include <stdlib.h>
#include <string.h>

#include "runtime_internal.h"

vn_u32 scene_script_res_id(vn_u32 scene_id) {
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

int load_scene_script(const VNPak* pak, vn_u32 scene_id, vn_u8** out_buf, vn_u32* out_size) {
    vn_u32 res_id;
    const ResourceEntry* entry;
    vn_u8* script_buf;
    vn_u32 read_size;
    int rc;

    if (pak == (const VNPak*)0 || out_buf == (vn_u8**)0 || out_size == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }

    *out_buf = (vn_u8*)0;
    *out_size = 0u;

    res_id = scene_script_res_id(scene_id);
    entry = vnpak_get(pak, res_id);
    if (entry == (const ResourceEntry*)0 || entry->type != 2u || entry->data_size == 0u) {
        return VN_E_FORMAT;
    }

    script_buf = (vn_u8*)malloc((size_t)entry->data_size);
    if (script_buf == (vn_u8*)0) {
        return VN_E_NOMEM;
    }

    rc = vnpak_read_resource(pak, res_id, script_buf, entry->data_size, &read_size);
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

void runtime_session_cleanup(VNRuntimeSession* session) {
    if (session == (VNRuntimeSession*)0) {
        return;
    }
    if (session->renderer_ready != VN_FALSE) {
        renderer_shutdown();
        session->renderer_ready = VN_FALSE;
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
