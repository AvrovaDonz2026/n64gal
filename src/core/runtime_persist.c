#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_internal.h"
#include "platform.h"
#include "vn_preview.h"

#define VN_RUNTIME_SNAPSHOT_MAGIC_0 ((vn_u8)'V')
#define VN_RUNTIME_SNAPSHOT_MAGIC_1 ((vn_u8)'N')
#define VN_RUNTIME_SNAPSHOT_MAGIC_2 ((vn_u8)'R')
#define VN_RUNTIME_SNAPSHOT_MAGIC_3 ((vn_u8)'S')
#define VN_RUNTIME_SNAPSHOT_PAYLOAD_VERSION 1u

static vn_u32 g_runtime_snapshot_crc32_table[256];
static int g_runtime_snapshot_crc32_ready = VN_FALSE;

void vn_runtime_query_build_info(VNRuntimeBuildInfo* out_info) {
    if (out_info == (VNRuntimeBuildInfo*)0) {
        return;
    }

    out_info->runtime_api_version = VN_RUNTIME_API_VERSION;
    out_info->runtime_api_stability = VN_RUNTIME_API_STABILITY;
    out_info->preview_protocol_version = VN_PREVIEW_PROTOCOL_VERSION;
    out_info->vnpak_read_min_version = VNPAK_READ_MIN_VERSION;
    out_info->vnpak_read_max_version = VNPAK_READ_MAX_VERSION;
    out_info->vnpak_write_default_version = VNPAK_WRITE_DEFAULT_VERSION;
    out_info->vnsave_latest_version = VNSAVE_VERSION_1;
    out_info->vnsave_api_stability = VNSAVE_API_STABILITY;
    out_info->vnsave_public_saveload_scope = VNSAVE_PUBLIC_SAVELOAD_SCOPE;
    out_info->host_os = vn_platform_host_os_name();
    out_info->host_arch = vn_platform_host_arch_name();
    out_info->host_compiler = vn_platform_host_compiler_name();
}

static void runtime_copy_string(char* dst, size_t dst_cap, const char* src) {
    size_t len;

    if (dst == (char*)0 || dst_cap == 0u) {
        return;
    }
    dst[0] = '\0';
    if (src == (const char*)0) {
        return;
    }
    len = strlen(src);
    if (len + 1u > dst_cap) {
        len = dst_cap - 1u;
    }
    if (len > 0u) {
        (void)memcpy(dst, src, len);
    }
    dst[len] = '\0';
}

static void runtime_u16_le_write(vn_u8* p, vn_u16 value) {
    p[0] = (vn_u8)(value & 0xFFu);
    p[1] = (vn_u8)((value >> 8) & 0xFFu);
}

static void runtime_u32_le_write(vn_u8* p, vn_u32 value) {
    p[0] = (vn_u8)(value & 0xFFu);
    p[1] = (vn_u8)((value >> 8) & 0xFFu);
    p[2] = (vn_u8)((value >> 16) & 0xFFu);
    p[3] = (vn_u8)((value >> 24) & 0xFFu);
}

static vn_u16 runtime_u16_le_read(const vn_u8* p) {
    return (vn_u16)((vn_u16)p[0] | ((vn_u16)p[1] << 8));
}

static vn_u32 runtime_u32_le_read(const vn_u8* p) {
    return (vn_u32)(((vn_u32)p[0]) |
                    ((vn_u32)p[1] << 8) |
                    ((vn_u32)p[2] << 16) |
                    ((vn_u32)p[3] << 24));
}

static void runtime_snapshot_crc32_table_init(void) {
    vn_u32 i;

    if (g_runtime_snapshot_crc32_ready != VN_FALSE) {
        return;
    }
    for (i = 0u; i < 256u; ++i) {
        vn_u32 c;
        vn_u32 j;

        c = i;
        for (j = 0u; j < 8u; ++j) {
            if ((c & 1u) != 0u) {
                c = (c >> 1) ^ 0xEDB88320u;
            } else {
                c >>= 1;
            }
        }
        g_runtime_snapshot_crc32_table[i] = c;
    }
    g_runtime_snapshot_crc32_ready = VN_TRUE;
}

static vn_u32 runtime_snapshot_crc32(const vn_u8* data, vn_u32 size) {
    vn_u32 crc;
    vn_u32 i;

    if (data == (const vn_u8*)0 || size == 0u) {
        return 0u;
    }

    runtime_snapshot_crc32_table_init();
    crc = 0xFFFFFFFFu;
    for (i = 0u; i < size; ++i) {
        vn_u32 idx;
        idx = (vn_u32)((crc ^ (vn_u32)data[i]) & 0xFFu);
        crc = g_runtime_snapshot_crc32_table[idx] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

static int runtime_snapshot_write_u8(vn_u8** io_p, const vn_u8* end, vn_u8 value) {
    if (io_p == (vn_u8**)0 || *io_p == (vn_u8*)0 || end == (const vn_u8*)0 || (size_t)(end - *io_p) < 1u) {
        return VN_E_FORMAT;
    }
    (*io_p)[0] = value;
    *io_p += 1;
    return VN_OK;
}

static int runtime_snapshot_write_u16(vn_u8** io_p, const vn_u8* end, vn_u16 value) {
    if (io_p == (vn_u8**)0 || *io_p == (vn_u8*)0 || end == (const vn_u8*)0 || (size_t)(end - *io_p) < 2u) {
        return VN_E_FORMAT;
    }
    runtime_u16_le_write(*io_p, value);
    *io_p += 2;
    return VN_OK;
}

static int runtime_snapshot_write_u32(vn_u8** io_p, const vn_u8* end, vn_u32 value) {
    if (io_p == (vn_u8**)0 || *io_p == (vn_u8*)0 || end == (const vn_u8*)0 || (size_t)(end - *io_p) < 4u) {
        return VN_E_FORMAT;
    }
    runtime_u32_le_write(*io_p, value);
    *io_p += 4;
    return VN_OK;
}

static int runtime_snapshot_write_bytes(vn_u8** io_p,
                                        const vn_u8* end,
                                        const void* src,
                                        vn_u32 size) {
    if (io_p == (vn_u8**)0 || *io_p == (vn_u8*)0 || end == (const vn_u8*)0 || src == (const void*)0) {
        return VN_E_FORMAT;
    }
    if ((size_t)(end - *io_p) < (size_t)size) {
        return VN_E_FORMAT;
    }
    if (size > 0u) {
        (void)memcpy(*io_p, src, (size_t)size);
        *io_p += size;
    }
    return VN_OK;
}

static int runtime_snapshot_read_u8(const vn_u8** io_p, const vn_u8* end, vn_u8* out_value) {
    if (io_p == (const vn_u8**)0 || *io_p == (const vn_u8*)0 || end == (const vn_u8*)0 || out_value == (vn_u8*)0) {
        return VN_E_FORMAT;
    }
    if ((size_t)(end - *io_p) < 1u) {
        return VN_E_FORMAT;
    }
    *out_value = (*io_p)[0];
    *io_p += 1;
    return VN_OK;
}

static int runtime_snapshot_read_u16(const vn_u8** io_p, const vn_u8* end, vn_u16* out_value) {
    if (io_p == (const vn_u8**)0 || *io_p == (const vn_u8*)0 || end == (const vn_u8*)0 || out_value == (vn_u16*)0) {
        return VN_E_FORMAT;
    }
    if ((size_t)(end - *io_p) < 2u) {
        return VN_E_FORMAT;
    }
    *out_value = runtime_u16_le_read(*io_p);
    *io_p += 2;
    return VN_OK;
}

static int runtime_snapshot_read_u32(const vn_u8** io_p, const vn_u8* end, vn_u32* out_value) {
    if (io_p == (const vn_u8**)0 || *io_p == (const vn_u8*)0 || end == (const vn_u8*)0 || out_value == (vn_u32*)0) {
        return VN_E_FORMAT;
    }
    if ((size_t)(end - *io_p) < 4u) {
        return VN_E_FORMAT;
    }
    *out_value = runtime_u32_le_read(*io_p);
    *io_p += 4;
    return VN_OK;
}

static int runtime_snapshot_read_bytes(const vn_u8** io_p,
                                       const vn_u8* end,
                                       void* dst,
                                       vn_u32 size) {
    if (io_p == (const vn_u8**)0 || *io_p == (const vn_u8*)0 || end == (const vn_u8*)0 || dst == (void*)0) {
        return VN_E_FORMAT;
    }
    if ((size_t)(end - *io_p) < (size_t)size) {
        return VN_E_FORMAT;
    }
    if (size > 0u) {
        (void)memcpy(dst, *io_p, (size_t)size);
        *io_p += size;
    }
    return VN_OK;
}

static vn_u32 runtime_snapshot_payload_size(void) {
    return 12u +
           VN_RUNTIME_SNAPSHOT_PATH_MAX +
           VN_RUNTIME_SNAPSHOT_BACKEND_MAX +
           4u +
           2u + 2u +
           4u + 4u + 4u + 4u + 4u + 4u + 4u + 4u +
           1u +
           VN_RUNTIME_MAX_CHOICE_SEQ +
           4u + 4u +
           4u + 4u +
           4u + 4u +
           (VN_RUNTIME_SNAPSHOT_CALL_STACK_MAX * 2u) +
           1u +
           2u + 2u +
           1u + 1u +
           2u +
           2u +
           1u +
           2u +
           1u + 1u + 1u + 1u + 1u +
           2u +
           4u + 4u + 4u +
           4u +
           1u + 1u + 1u + 1u + 1u +
           2u +
           4u;
}

static int runtime_text_is_terminated(const char* text, size_t cap) {
    if (text == (const char*)0) {
        return VN_FALSE;
    }
    return (memchr((const void*)text, '\0', cap) != (const void*)0) ? VN_TRUE : VN_FALSE;
}

static const char* runtime_backend_name_from_flags(vn_u32 flags) {
    if ((flags & VN_RENDERER_FLAG_FORCE_AVX2_ASM) != 0u) {
        return "avx2_asm";
    }
    if ((flags & VN_RENDERER_FLAG_FORCE_AVX2) != 0u) {
        return "avx2";
    }
    if ((flags & VN_RENDERER_FLAG_FORCE_NEON) != 0u) {
        return "neon";
    }
    if ((flags & VN_RENDERER_FLAG_FORCE_RVV) != 0u) {
        return "rvv";
    }
    if ((flags & VN_RENDERER_FLAG_FORCE_SCALAR) != 0u) {
        return "scalar";
    }
    return "auto";
}

static int runtime_backend_name_is_valid(const char* value) {
    if (value == (const char*)0 || value[0] == '\0' || strcmp(value, "auto") == 0) {
        return VN_TRUE;
    }
    return (parse_backend_flag(value) != 0u) ? VN_TRUE : VN_FALSE;
}

static int runtime_snapshot_base_dims(const VNRuntimeSession* session,
                                      vn_u16* out_width,
                                      vn_u16* out_height) {
    const VNDynResTier* tier0;

    if (session == (const VNRuntimeSession*)0 ||
        out_width == (vn_u16*)0 ||
        out_height == (vn_u16*)0) {
        return VN_E_INVALID_ARG;
    }

    tier0 = vn_dynres_get_tier(&session->dynamic_resolution, 0u);
    if (tier0 != (const VNDynResTier*)0 &&
        tier0->width != 0u &&
        tier0->height != 0u) {
        *out_width = tier0->width;
        *out_height = tier0->height;
        return VN_OK;
    }

    if (session->renderer_cfg.width == 0u || session->renderer_cfg.height == 0u) {
        return VN_E_FORMAT;
    }
    *out_width = session->renderer_cfg.width;
    *out_height = session->renderer_cfg.height;
    return VN_OK;
}

static void runtime_state_from_snapshot_fields(VNRuntimeSession* session,
                                               const VNRuntimeSessionSnapshot* snapshot) {
    if (session == (VNRuntimeSession*)0 || snapshot == (const VNRuntimeSessionSnapshot*)0) {
        return;
    }

    session->state.frame_index = session->frames_executed;
    session->state.text_id = snapshot->vm_current_text_id;
    session->state.text_speed_ms = snapshot->vm_current_text_speed_ms;
    session->state.vm_waiting = ((snapshot->vm_flags & VN_VM_FLAG_WAITING) != 0u) ? 1u : 0u;
    session->state.vm_ended = ((snapshot->vm_flags & VN_VM_FLAG_ENDED) != 0u) ? 1u : 0u;
    session->state.vm_error = ((snapshot->vm_flags & VN_VM_FLAG_ERROR) != 0u) ? 1u : 0u;
    session->state.bgm_id = snapshot->vm_current_bgm_id;
    session->state.bgm_loop = snapshot->vm_current_bgm_loop;
    session->state.se_id = 0u;
    session->state.choice_count = snapshot->vm_last_choice_count;
    session->state.choice_text_id = snapshot->vm_last_choice_text_id;
    session->state.choice_selected_index = snapshot->vm_last_choice_selected_index;
    state_apply_fade(&session->state, &session->fade_player);
}

int vn_runtime_session_capture_snapshot(const VNRuntimeSession* session,
                                        VNRuntimeSessionSnapshot* out_snapshot) {
    vn_u16 base_width;
    vn_u16 base_height;
    vn_u32 i;
    vn_u32 pc_offset;

    if (session == (const VNRuntimeSession*)0 || out_snapshot == (VNRuntimeSessionSnapshot*)0) {
        return VN_E_INVALID_ARG;
    }
    if (session->exit_code != 0 ||
        session->injected_has_choice != VN_FALSE ||
        session->injected_trace_toggle_count != 0u ||
        session->injected_quit != VN_FALSE) {
        return VN_E_UNSUPPORTED;
    }
    if (session->pak.path == (const char*)0 ||
        session->script_buf == (vn_u8*)0 ||
        session->vm.script_base == (const vn_u8*)0 ||
        session->vm.script_pc == (const vn_u8*)0) {
        return VN_E_RENDER_STATE;
    }
    if (session->choice_feed.count > VN_RUNTIME_MAX_CHOICE_SEQ ||
        session->choice_feed.cursor > session->choice_feed.count ||
        session->vm.call_sp > VN_RUNTIME_SNAPSHOT_CALL_STACK_MAX) {
        return VN_E_FORMAT;
    }

    if (runtime_snapshot_base_dims(session, &base_width, &base_height) != VN_OK) {
        return VN_E_FORMAT;
    }
    if (strlen(session->pak.path) + 1u > sizeof(out_snapshot->pack_path) ||
        strlen(runtime_backend_name_from_flags(session->renderer_cfg.flags)) + 1u > sizeof(out_snapshot->backend_name)) {
        return VN_E_NOMEM;
    }

    (void)memset(out_snapshot, 0, sizeof(*out_snapshot));
    runtime_copy_string(out_snapshot->pack_path,
                        sizeof(out_snapshot->pack_path),
                        session->pak.path);
    runtime_copy_string(out_snapshot->backend_name,
                        sizeof(out_snapshot->backend_name),
                        runtime_backend_name_from_flags(session->renderer_cfg.flags));
    out_snapshot->scene_id = session->state.scene_id;
    out_snapshot->base_width = base_width;
    out_snapshot->base_height = base_height;
    out_snapshot->frames_limit = session->frames_limit;
    out_snapshot->frames_executed = session->frames_executed;
    out_snapshot->dt_ms = session->dt_ms;
    out_snapshot->trace = session->trace;
    out_snapshot->emit_logs = session->emit_logs;
    out_snapshot->hold_on_end = session->hold_on_end;
    out_snapshot->perf_flags = session->perf_flags;
    out_snapshot->keyboard_enabled = (session->keyboard.enabled != VN_FALSE) ? 1u : 0u;
    out_snapshot->default_choice_index = session->default_choice_index;
    out_snapshot->choice_feed_count = session->choice_feed.count;
    out_snapshot->choice_feed_cursor = session->choice_feed.cursor;
    for (i = 0u; i < session->choice_feed.count; ++i) {
        out_snapshot->choice_feed_items[i] = session->choice_feed.items[i];
    }
    out_snapshot->dynamic_resolution_tier = vn_dynres_get_current_tier(&session->dynamic_resolution);
    out_snapshot->dynamic_resolution_switches = vn_dynres_get_switch_count(&session->dynamic_resolution);

    pc_offset = (vn_u32)(session->vm.script_pc - session->vm.script_base);
    out_snapshot->vm_pc_offset = pc_offset;
    out_snapshot->vm_wait_ms = session->vm.wait_ms;
    out_snapshot->vm_call_sp = session->vm.call_sp;
    for (i = 0u; i < session->vm.call_sp; ++i) {
        out_snapshot->vm_call_stack[i] = session->vm.call_stack[i];
    }
    out_snapshot->vm_current_text_id = session->vm.current_text_id;
    out_snapshot->vm_current_text_speed_ms = session->vm.current_text_speed_ms;
    out_snapshot->vm_fade_layer_mask = session->vm.fade_layer_mask;
    out_snapshot->vm_fade_target_alpha = session->vm.fade_target_alpha;
    out_snapshot->vm_fade_duration_ms = session->vm.fade_duration_ms;
    out_snapshot->vm_current_bgm_id = session->vm.current_bgm_id;
    out_snapshot->vm_current_bgm_loop = session->vm.current_bgm_loop;
    out_snapshot->vm_pending_se_id = session->vm.pending_se_id;
    out_snapshot->vm_pending_se_flag = session->vm.pending_se_flag;
    out_snapshot->vm_last_choice_count = session->vm.last_choice_count;
    out_snapshot->vm_last_choice_selected_index = session->vm.last_choice_selected_index;
    out_snapshot->vm_external_choice_valid = session->vm.external_choice_valid;
    out_snapshot->vm_external_choice_index = session->vm.external_choice_index;
    out_snapshot->vm_last_choice_text_id = session->vm.last_choice_text_id;
    out_snapshot->vm_choice_serial = session->vm.choice_serial;
    out_snapshot->vm_fade_serial = session->vm.fade_serial;
    out_snapshot->vm_flags = session->vm.flags;

    out_snapshot->fade_seen_serial = session->fade_player.seen_serial;
    out_snapshot->fade_active = (vn_u8)(session->fade_player.active != VN_FALSE ? 1u : 0u);
    out_snapshot->fade_layer_mask = session->fade_player.layer_mask;
    out_snapshot->fade_alpha_current = session->fade_player.alpha_current;
    out_snapshot->fade_alpha_start = session->fade_player.alpha_start;
    out_snapshot->fade_alpha_target = session->fade_player.alpha_target;
    out_snapshot->fade_duration_ms = session->fade_player.duration_ms;
    out_snapshot->fade_elapsed_ms = session->fade_player.elapsed_ms;
    return VN_OK;
}

int vn_runtime_session_create_from_snapshot(const VNRuntimeSessionSnapshot* snapshot,
                                            VNRuntimeSession** out_session) {
    VNRunConfig cfg;
    VNRuntimeSession* session;
    const VNDynResTier* current_dims;
    const char* scene_name;
    vn_u32 i;
    int rc;

    if (snapshot == (const VNRuntimeSessionSnapshot*)0 || out_session == (VNRuntimeSession**)0) {
        return VN_E_INVALID_ARG;
    }
    if (runtime_text_is_terminated(snapshot->pack_path, sizeof(snapshot->pack_path)) == VN_FALSE ||
        runtime_text_is_terminated(snapshot->backend_name, sizeof(snapshot->backend_name)) == VN_FALSE ||
        runtime_backend_name_is_valid(snapshot->backend_name) == VN_FALSE ||
        snapshot->base_width == 0u ||
        snapshot->base_height == 0u ||
        snapshot->frames_limit == 0u ||
        snapshot->dt_ms > 1000u ||
        snapshot->choice_feed_count > VN_RUNTIME_MAX_CHOICE_SEQ ||
        snapshot->choice_feed_cursor > snapshot->choice_feed_count ||
        snapshot->vm_call_sp > VN_RUNTIME_SNAPSHOT_CALL_STACK_MAX) {
        return VN_E_INVALID_ARG;
    }

    scene_name = scene_name_from_id(snapshot->scene_id);
    if ((snapshot->scene_id != VN_SCENE_S0 && strcmp(scene_name, "S0") == 0) ||
        (snapshot->perf_flags & ~runtime_supported_perf_flags()) != 0u) {
        return VN_E_INVALID_ARG;
    }

    vn_run_config_init(&cfg);
    cfg.pack_path = (snapshot->pack_path[0] != '\0') ? snapshot->pack_path : cfg.pack_path;
    cfg.scene_name = scene_name;
    cfg.backend_name = (snapshot->backend_name[0] != '\0') ? snapshot->backend_name : cfg.backend_name;
    cfg.width = snapshot->base_width;
    cfg.height = snapshot->base_height;
    cfg.frames = snapshot->frames_limit;
    cfg.dt_ms = snapshot->dt_ms;
    cfg.trace = snapshot->trace;
    cfg.keyboard = snapshot->keyboard_enabled;
    cfg.emit_logs = snapshot->emit_logs;
    cfg.hold_on_end = snapshot->hold_on_end;
    cfg.perf_flags = snapshot->perf_flags;
    cfg.choice_index = snapshot->default_choice_index;
    cfg.choice_seq_count = snapshot->choice_feed_count;
    for (i = 0u; i < snapshot->choice_feed_count; ++i) {
        cfg.choice_seq[i] = snapshot->choice_feed_items[i];
    }

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK) {
        return rc;
    }

    if (snapshot->vm_pc_offset >= session->script_size) {
        (void)vn_runtime_session_destroy(session);
        return VN_E_FORMAT;
    }

    vn_dynres_init(&session->dynamic_resolution, snapshot->base_width, snapshot->base_height);
    if (snapshot->dynamic_resolution_tier >= vn_dynres_get_tier_count(&session->dynamic_resolution)) {
        (void)vn_runtime_session_destroy(session);
        return VN_E_FORMAT;
    }
    session->dynamic_resolution.current_tier = snapshot->dynamic_resolution_tier;
    session->dynamic_resolution.switch_count = snapshot->dynamic_resolution_switches;
    vn_dynres_reset_history(&session->dynamic_resolution);
    current_dims = vn_dynres_get_current_dims(&session->dynamic_resolution);
    if (current_dims == (const VNDynResTier*)0 ||
        runtime_renderer_reconfigure(session, current_dims->width, current_dims->height) != VN_OK) {
        (void)vn_runtime_session_destroy(session);
        return VN_E_RENDER_STATE;
    }

    session->frames_executed = snapshot->frames_executed;
    session->default_choice_index = snapshot->default_choice_index;
    session->choice_feed.count = snapshot->choice_feed_count;
    session->choice_feed.cursor = snapshot->choice_feed_cursor;
    for (i = 0u; i < snapshot->choice_feed_count; ++i) {
        session->choice_feed.items[i] = snapshot->choice_feed_items[i];
    }

    session->vm.script_pc = session->vm.script_base + snapshot->vm_pc_offset;
    session->vm.wait_ms = snapshot->vm_wait_ms;
    session->vm.call_sp = snapshot->vm_call_sp;
    for (i = 0u; i < snapshot->vm_call_sp; ++i) {
        session->vm.call_stack[i] = snapshot->vm_call_stack[i];
    }
    session->vm.current_text_id = snapshot->vm_current_text_id;
    session->vm.current_text_speed_ms = snapshot->vm_current_text_speed_ms;
    session->vm.fade_layer_mask = snapshot->vm_fade_layer_mask;
    session->vm.fade_target_alpha = snapshot->vm_fade_target_alpha;
    session->vm.fade_duration_ms = snapshot->vm_fade_duration_ms;
    session->vm.current_bgm_id = snapshot->vm_current_bgm_id;
    session->vm.current_bgm_loop = snapshot->vm_current_bgm_loop;
    session->vm.pending_se_id = snapshot->vm_pending_se_id;
    session->vm.pending_se_flag = snapshot->vm_pending_se_flag;
    session->vm.last_choice_count = snapshot->vm_last_choice_count;
    session->vm.last_choice_selected_index = snapshot->vm_last_choice_selected_index;
    session->vm.external_choice_valid = snapshot->vm_external_choice_valid;
    session->vm.external_choice_index = snapshot->vm_external_choice_index;
    session->vm.last_choice_text_id = snapshot->vm_last_choice_text_id;
    session->vm.choice_serial = snapshot->vm_choice_serial;
    session->vm.fade_serial = snapshot->vm_fade_serial;
    session->vm.flags = snapshot->vm_flags;

    session->fade_player.seen_serial = snapshot->fade_seen_serial;
    session->fade_player.active = (snapshot->fade_active != 0u) ? VN_TRUE : VN_FALSE;
    session->fade_player.layer_mask = snapshot->fade_layer_mask;
    session->fade_player.alpha_current = snapshot->fade_alpha_current;
    session->fade_player.alpha_start = snapshot->fade_alpha_start;
    session->fade_player.alpha_target = snapshot->fade_alpha_target;
    session->fade_player.duration_ms = snapshot->fade_duration_ms;
    session->fade_player.elapsed_ms = snapshot->fade_elapsed_ms;

    session->last_choice_serial = session->vm.choice_serial;
    session->last_op_count = 0u;
    session->done = VN_FALSE;
    session->exit_code = 0;
    session->summary_emitted = VN_FALSE;
    session->injected_trace_toggle_count = 0u;
    session->injected_has_choice = VN_FALSE;
    session->injected_quit = VN_FALSE;
    runtime_render_cache_invalidate(session);
    runtime_dirty_planner_reconfigure(session,
                                      session->renderer_cfg.width,
                                      session->renderer_cfg.height);
    runtime_state_from_snapshot_fields(session, snapshot);
    runtime_result_publish(session);
    *out_session = session;
    return VN_OK;
}

static int runtime_snapshot_encode(const VNRuntimeSessionSnapshot* snapshot,
                                   vn_u8* out_payload,
                                   vn_u32 payload_size) {
    vn_u8* p;
    const vn_u8* end;
    vn_u32 i;
    int rc;

    if (snapshot == (const VNRuntimeSessionSnapshot*)0 || out_payload == (vn_u8*)0) {
        return VN_E_INVALID_ARG;
    }
    if (payload_size != runtime_snapshot_payload_size()) {
        return VN_E_INVALID_ARG;
    }
    if (runtime_text_is_terminated(snapshot->pack_path, sizeof(snapshot->pack_path)) == VN_FALSE ||
        runtime_text_is_terminated(snapshot->backend_name, sizeof(snapshot->backend_name)) == VN_FALSE) {
        return VN_E_FORMAT;
    }

    p = out_payload;
    end = out_payload + payload_size;

    rc = runtime_snapshot_write_u8(&p, end, VN_RUNTIME_SNAPSHOT_MAGIC_0);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, VN_RUNTIME_SNAPSHOT_MAGIC_1);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, VN_RUNTIME_SNAPSHOT_MAGIC_2);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, VN_RUNTIME_SNAPSHOT_MAGIC_3);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, VN_RUNTIME_SNAPSHOT_PAYLOAD_VERSION);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, payload_size);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_bytes(&p, end, snapshot->pack_path, sizeof(snapshot->pack_path));
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_bytes(&p, end, snapshot->backend_name, sizeof(snapshot->backend_name));
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->scene_id);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u16(&p, end, snapshot->base_width);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u16(&p, end, snapshot->base_height);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->frames_limit);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->frames_executed);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->dt_ms);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->trace);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->emit_logs);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->hold_on_end);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->perf_flags);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->keyboard_enabled);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->default_choice_index);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_bytes(&p, end, snapshot->choice_feed_items, VN_RUNTIME_MAX_CHOICE_SEQ);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->choice_feed_count);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->choice_feed_cursor);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->dynamic_resolution_tier);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->dynamic_resolution_switches);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->vm_pc_offset);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->vm_wait_ms);
    if (rc != VN_OK) return rc;
    for (i = 0u; i < VN_RUNTIME_SNAPSHOT_CALL_STACK_MAX; ++i) {
        rc = runtime_snapshot_write_u16(&p, end, snapshot->vm_call_stack[i]);
        if (rc != VN_OK) return rc;
    }
    rc = runtime_snapshot_write_u8(&p, end, snapshot->vm_call_sp);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u16(&p, end, snapshot->vm_current_text_id);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u16(&p, end, snapshot->vm_current_text_speed_ms);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->vm_fade_layer_mask);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->vm_fade_target_alpha);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u16(&p, end, snapshot->vm_fade_duration_ms);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u16(&p, end, snapshot->vm_current_bgm_id);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->vm_current_bgm_loop);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u16(&p, end, snapshot->vm_pending_se_id);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->vm_pending_se_flag);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->vm_last_choice_count);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->vm_last_choice_selected_index);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->vm_external_choice_valid);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->vm_external_choice_index);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u16(&p, end, snapshot->vm_last_choice_text_id);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->vm_choice_serial);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->vm_fade_serial);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->vm_flags);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->fade_seen_serial);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->fade_active);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->fade_layer_mask);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->fade_alpha_current);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->fade_alpha_start);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u8(&p, end, snapshot->fade_alpha_target);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u16(&p, end, snapshot->fade_duration_ms);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_write_u32(&p, end, snapshot->fade_elapsed_ms);
    if (rc != VN_OK) return rc;
    return (p == end) ? VN_OK : VN_E_FORMAT;
}

static int runtime_snapshot_decode(const vn_u8* payload,
                                   vn_u32 payload_size,
                                   VNRuntimeSessionSnapshot* out_snapshot) {
    const vn_u8* p;
    const vn_u8* end;
    vn_u8 magic[4];
    vn_u32 payload_version;
    vn_u32 declared_size;
    vn_u32 i;
    int rc;

    if (payload == (const vn_u8*)0 || out_snapshot == (VNRuntimeSessionSnapshot*)0) {
        return VN_E_INVALID_ARG;
    }
    if (payload_size != runtime_snapshot_payload_size()) {
        return VN_E_FORMAT;
    }

    (void)memset(out_snapshot, 0, sizeof(*out_snapshot));
    p = payload;
    end = payload + payload_size;

    for (i = 0u; i < 4u; ++i) {
        rc = runtime_snapshot_read_u8(&p, end, &magic[i]);
        if (rc != VN_OK) return rc;
    }
    if (magic[0] != VN_RUNTIME_SNAPSHOT_MAGIC_0 ||
        magic[1] != VN_RUNTIME_SNAPSHOT_MAGIC_1 ||
        magic[2] != VN_RUNTIME_SNAPSHOT_MAGIC_2 ||
        magic[3] != VN_RUNTIME_SNAPSHOT_MAGIC_3) {
        return VN_E_UNSUPPORTED;
    }
    rc = runtime_snapshot_read_u32(&p, end, &payload_version);
    if (rc != VN_OK) return rc;
    if (payload_version != VN_RUNTIME_SNAPSHOT_PAYLOAD_VERSION) {
        return VN_E_UNSUPPORTED;
    }
    rc = runtime_snapshot_read_u32(&p, end, &declared_size);
    if (rc != VN_OK) return rc;
    if (declared_size != payload_size) {
        return VN_E_FORMAT;
    }
    rc = runtime_snapshot_read_bytes(&p, end, out_snapshot->pack_path, sizeof(out_snapshot->pack_path));
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_bytes(&p, end, out_snapshot->backend_name, sizeof(out_snapshot->backend_name));
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->scene_id);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u16(&p, end, &out_snapshot->base_width);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u16(&p, end, &out_snapshot->base_height);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->frames_limit);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->frames_executed);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->dt_ms);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->trace);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->emit_logs);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->hold_on_end);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->perf_flags);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->keyboard_enabled);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->default_choice_index);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_bytes(&p, end, out_snapshot->choice_feed_items, VN_RUNTIME_MAX_CHOICE_SEQ);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->choice_feed_count);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->choice_feed_cursor);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->dynamic_resolution_tier);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->dynamic_resolution_switches);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->vm_pc_offset);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->vm_wait_ms);
    if (rc != VN_OK) return rc;
    for (i = 0u; i < VN_RUNTIME_SNAPSHOT_CALL_STACK_MAX; ++i) {
        rc = runtime_snapshot_read_u16(&p, end, &out_snapshot->vm_call_stack[i]);
        if (rc != VN_OK) return rc;
    }
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->vm_call_sp);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u16(&p, end, &out_snapshot->vm_current_text_id);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u16(&p, end, &out_snapshot->vm_current_text_speed_ms);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->vm_fade_layer_mask);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->vm_fade_target_alpha);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u16(&p, end, &out_snapshot->vm_fade_duration_ms);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u16(&p, end, &out_snapshot->vm_current_bgm_id);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->vm_current_bgm_loop);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u16(&p, end, &out_snapshot->vm_pending_se_id);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->vm_pending_se_flag);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->vm_last_choice_count);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->vm_last_choice_selected_index);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->vm_external_choice_valid);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->vm_external_choice_index);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u16(&p, end, &out_snapshot->vm_last_choice_text_id);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->vm_choice_serial);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->vm_fade_serial);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->vm_flags);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->fade_seen_serial);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->fade_active);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->fade_layer_mask);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->fade_alpha_current);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->fade_alpha_start);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u8(&p, end, &out_snapshot->fade_alpha_target);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u16(&p, end, &out_snapshot->fade_duration_ms);
    if (rc != VN_OK) return rc;
    rc = runtime_snapshot_read_u32(&p, end, &out_snapshot->fade_elapsed_ms);
    if (rc != VN_OK) return rc;
    if (p != end) {
        return VN_E_FORMAT;
    }
    if (runtime_text_is_terminated(out_snapshot->pack_path, sizeof(out_snapshot->pack_path)) == VN_FALSE ||
        runtime_text_is_terminated(out_snapshot->backend_name, sizeof(out_snapshot->backend_name)) == VN_FALSE) {
        return VN_E_FORMAT;
    }
    return VN_OK;
}

int vn_runtime_session_save_to_file(const VNRuntimeSession* session,
                                    const char* path,
                                    vn_u32 slot_id,
                                    vn_u32 timestamp_s) {
    VNRuntimeSessionSnapshot snapshot;
    vn_u8 header[VNSAVE_HEADER_SIZE_V1];
    vn_u8* payload;
    vn_u32 payload_size;
    vn_u32 crc;
    FILE* fp;
    int rc;

    if (path == (const char*)0) {
        return VN_E_INVALID_ARG;
    }
    payload = (vn_u8*)0;
    payload_size = runtime_snapshot_payload_size();
    rc = vn_runtime_session_capture_snapshot(session, &snapshot);
    if (rc != VN_OK) {
        return rc;
    }
    payload = (vn_u8*)malloc((size_t)payload_size);
    if (payload == (vn_u8*)0) {
        return VN_E_NOMEM;
    }
    rc = runtime_snapshot_encode(&snapshot, payload, payload_size);
    if (rc != VN_OK) {
        free(payload);
        return rc;
    }
    crc = runtime_snapshot_crc32(payload, payload_size);
    header[0] = (vn_u8)'V';
    header[1] = (vn_u8)'N';
    header[2] = (vn_u8)'S';
    header[3] = (vn_u8)'V';
    runtime_u32_le_write(header + 4, VNSAVE_VERSION_1);
    runtime_u32_le_write(header + 8, slot_id);
    runtime_u32_le_write(header + 12, snapshot.vm_pc_offset);
    runtime_u32_le_write(header + 16, snapshot.scene_id);
    runtime_u32_le_write(header + 20, timestamp_s);
    runtime_u32_le_write(header + 24, crc);
    runtime_u32_le_write(header + 28, 0u);

    fp = fopen(path, "wb");
    if (fp == (FILE*)0) {
        free(payload);
        return VN_E_IO;
    }
    if (fwrite(header, 1u, VNSAVE_HEADER_SIZE_V1, fp) != (size_t)VNSAVE_HEADER_SIZE_V1 ||
        fwrite(payload, 1u, payload_size, fp) != (size_t)payload_size) {
        free(payload);
        (void)fclose(fp);
        return VN_E_IO;
    }
    free(payload);
    if (fclose(fp) != 0) {
        return VN_E_IO;
    }
    return VN_OK;
}

int vn_runtime_session_load_from_file(const char* path,
                                      VNRuntimeSession** out_session) {
    VNSaveProbe probe;
    FILE* fp;
    vn_u8* payload;
    VNRuntimeSessionSnapshot snapshot;
    int rc;

    if (path == (const char*)0 || out_session == (VNRuntimeSession**)0) {
        return VN_E_INVALID_ARG;
    }
    *out_session = (VNRuntimeSession*)0;

    rc = vnsave_probe_file(path, &probe);
    if (rc != VN_OK) {
        return rc;
    }
    if (probe.version != VNSAVE_VERSION_1 || probe.header_size != VNSAVE_HEADER_SIZE_V1) {
        return VN_E_UNSUPPORTED;
    }
    if (probe.payload_size != runtime_snapshot_payload_size()) {
        return VN_E_UNSUPPORTED;
    }

    fp = vn_platform_fopen_read_binary(path);
    if (fp == (FILE*)0) {
        return VN_E_IO;
    }
    if (fseek(fp, (long)VNSAVE_HEADER_SIZE_V1, SEEK_SET) != 0) {
        (void)fclose(fp);
        return VN_E_IO;
    }

    payload = (vn_u8*)malloc((size_t)probe.payload_size);
    if (payload == (vn_u8*)0) {
        (void)fclose(fp);
        return VN_E_NOMEM;
    }
    if ((int)fread(payload, 1u, probe.payload_size, fp) != (int)probe.payload_size) {
        free(payload);
        (void)fclose(fp);
        return VN_E_IO;
    }
    (void)fclose(fp);

    rc = runtime_snapshot_decode(payload, probe.payload_size, &snapshot);
    free(payload);
    if (rc != VN_OK) {
        return rc;
    }
    if (probe.script_pc != snapshot.vm_pc_offset || probe.scene_id != snapshot.scene_id) {
        return VN_E_FORMAT;
    }
    return vn_runtime_session_create_from_snapshot(&snapshot, out_session);
}
