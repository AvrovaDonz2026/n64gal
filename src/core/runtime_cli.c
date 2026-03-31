#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <conio.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#endif

#include "vn_renderer.h"
#include "vn_frontend.h"
#include "../frontend/dirty_tiles.h"
#include "vn_pack.h"
#include "vn_preview.h"
#include "vn_vm.h"
#include "vn_runtime.h"
#include "vn_error.h"
#include "vn_save.h"
#include "platform.h"
#include "dynamic_resolution.h"

#define VN_MAX_CHOICE_SEQ 64u
#define VN_OP_CACHE_CAP 320u
#define VN_RUNTIME_SNAPSHOT_PAYLOAD_VERSION_1 1u
#define VN_RUNTIME_SNAPSHOT_MAGIC_0 ((vn_u8)'V')
#define VN_RUNTIME_SNAPSHOT_MAGIC_1 ((vn_u8)'N')
#define VN_RUNTIME_SNAPSHOT_MAGIC_2 ((vn_u8)'R')
#define VN_RUNTIME_SNAPSHOT_MAGIC_3 ((vn_u8)'S')
#define VN_RUNTIME_SNAPSHOT_PAYLOAD_VERSION 1u

typedef struct {
    vn_u8 items[VN_MAX_CHOICE_SEQ];
    vn_u32 count;
    vn_u32 cursor;
} ChoiceFeed;

typedef struct {
    vn_u32 seen_serial;
    vn_u8 active;
    vn_u8 layer_mask;
    vn_u8 alpha_current;
    vn_u8 alpha_start;
    vn_u8 alpha_target;
    vn_u16 duration_ms;
    vn_u32 elapsed_ms;
} FadePlayer;

typedef struct {
    int enabled;
    int active;
    int quit_requested;
#if !defined(_WIN32)
    struct termios old_termios;
    int old_flags;
#endif
} KeyboardInput;

typedef struct {
    vn_u32 op_count;
    vn_u32 clear_gray;
    vn_u32 clear_flags;
    vn_u32 scene_id;
    vn_u32 sprite_flags;
    vn_u32 text_tex_id;
    vn_u32 text_flags;
    vn_u32 text_alpha;
    vn_u32 fade_flags;
    vn_u32 fade_mask;
} RenderOpCacheKey;

typedef struct {
    vn_u32 valid;
    vn_u32 key;
    vn_u32 stamp;
    RenderOpCacheKey render_key;
    VNRenderOp ops[16];
    vn_u32 op_count;
} RenderOpCacheEntry;

struct VNRuntimeSession {
    RendererConfig renderer_cfg;
    VNRuntimeState state;
    VNRenderOp ops[16];
    VNPak pak;
    VNState vm;
    ChoiceFeed choice_feed;
    FadePlayer fade_player;
    KeyboardInput keyboard;
    RenderOpCacheEntry op_cache[VN_OP_CACHE_CAP];
    vn_u8* script_buf;
    vn_u32 script_size;
    vn_u32 frames_limit;
    vn_u32 dt_ms;
    vn_u32 trace;
    vn_u32 emit_logs;
    vn_u32 hold_on_end;
    vn_u32 perf_flags;
    vn_u32 frame_reuse_valid;
    RenderOpCacheKey frame_reuse_key;
    vn_u32 frame_reuse_hits;
    vn_u32 frame_reuse_misses;
    vn_u32 op_cache_stamp;
    vn_u32 op_cache_hits;
    vn_u32 op_cache_misses;
    VNDirtyPlannerState dirty_planner;
    VNDirtyPlan dirty_plan;
    vn_u32* dirty_bits;
    vn_u32 dirty_tile_count;
    vn_u32 dirty_rect_count;
    vn_u32 dirty_full_redraw;
    vn_u32 dirty_tile_frames;
    vn_u32 dirty_tile_total;
    vn_u32 dirty_rect_total;
    vn_u32 dirty_full_redraws;
    VNDynResState dynamic_resolution;
    vn_u32 frames_executed;
    vn_u32 last_op_count;
    vn_u32 last_choice_serial;
    vn_u32 injected_trace_toggle_count;
    vn_u8 default_choice_index;
    vn_u8 injected_choice_index;
    int injected_has_choice;
    int injected_quit;
    int pak_opened;
    int vm_ready;
    int renderer_ready;
    int done;
    int exit_code;
    int summary_emitted;
};

static vn_u32 parse_backend_flag(const char* value);
static const char* scene_name_from_id(vn_u32 scene_id);
static void state_apply_fade(VNRuntimeState* state, const FadePlayer* fade);
static void runtime_render_cache_invalidate(VNRuntimeSession* session);
static void runtime_dirty_planner_reconfigure(VNRuntimeSession* session,
                                              vn_u16 width,
                                              vn_u16 height);
static int runtime_renderer_reconfigure(VNRuntimeSession* session,
                                        vn_u16 width,
                                        vn_u16 height);
static vn_u32 runtime_supported_perf_flags(void);
static void runtime_result_publish(const VNRuntimeSession* session);

static VNRunResult g_last_run_result;
static vn_u32 g_runtime_snapshot_crc32_table[256];
static int g_runtime_snapshot_crc32_ready = VN_FALSE;

static double runtime_now_ms(void) {
    return vn_platform_now_ms();
}

static double runtime_rss_mb(void) {
#if !defined(_WIN32)
    FILE* fp;
    long rss_pages;
    long page_size;

    fp = fopen("/proc/self/statm", "r");
    if (fp == (FILE*)0) {
        return 0.0;
    }
    rss_pages = 0L;
    if (fscanf(fp, "%*s %ld", &rss_pages) != 1) {
        (void)fclose(fp);
        return 0.0;
    }
    (void)fclose(fp);

    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || rss_pages < 0L) {
        return 0.0;
    }
    return ((double)rss_pages * (double)page_size) / (1024.0 * 1024.0);
#else
    return 0.0;
#endif
}

void vn_run_config_init(VNRunConfig* cfg) {
    if (cfg == (VNRunConfig*)0) {
        return;
    }
    cfg->pack_path = "assets/demo/demo.vnpak";
    cfg->scene_name = "S0";
    cfg->backend_name = "auto";
    cfg->width = 600u;
    cfg->height = 800u;
    cfg->frames = 1u;
    cfg->dt_ms = 16u;
    cfg->trace = 0u;
    cfg->keyboard = 0u;
    cfg->emit_logs = 1u;
    cfg->hold_on_end = 0u;
    cfg->perf_flags = VN_RUNTIME_PERF_DEFAULT_FLAGS;
    cfg->choice_index = 0u;
    cfg->choice_seq_count = 0u;
}

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
    if (session->done != VN_FALSE ||
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

static void runtime_result_reset(void) {
    g_last_run_result.frames_executed = 0u;
    g_last_run_result.text_id = 0u;
    g_last_run_result.vm_waiting = 0u;
    g_last_run_result.vm_ended = 0u;
    g_last_run_result.vm_error = 0u;
    g_last_run_result.fade_alpha = 0u;
    g_last_run_result.fade_remain_ms = 0u;
    g_last_run_result.bgm_id = 0u;
    g_last_run_result.se_id = 0u;
    g_last_run_result.choice_count = 0u;
    g_last_run_result.choice_selected_index = 0u;
    g_last_run_result.choice_text_id = 0u;
    g_last_run_result.op_count = 0u;
    g_last_run_result.backend_name = "none";
    g_last_run_result.perf_flags_effective = 0u;
    g_last_run_result.frame_reuse_hits = 0u;
    g_last_run_result.frame_reuse_misses = 0u;
    g_last_run_result.op_cache_hits = 0u;
    g_last_run_result.op_cache_misses = 0u;
    g_last_run_result.dirty_tile_count = 0u;
    g_last_run_result.dirty_rect_count = 0u;
    g_last_run_result.dirty_full_redraw = 0u;
    g_last_run_result.dirty_tile_frames = 0u;
    g_last_run_result.dirty_tile_total = 0u;
    g_last_run_result.dirty_rect_total = 0u;
    g_last_run_result.dirty_full_redraws = 0u;
    g_last_run_result.render_width = 0u;
    g_last_run_result.render_height = 0u;
    g_last_run_result.dynamic_resolution_tier = 0u;
    g_last_run_result.dynamic_resolution_switches = 0u;
}

static int runtime_perf_flag_enabled(vn_u32 perf_flags, vn_u32 flag) {
    return ((perf_flags & flag) != 0u) ? VN_TRUE : VN_FALSE;
}

static vn_u32 runtime_supported_perf_flags(void) {
    return VN_RUNTIME_PERF_OP_CACHE | VN_RUNTIME_PERF_FRAME_REUSE | VN_RUNTIME_PERF_DIRTY_TILE | VN_RUNTIME_PERF_DYNAMIC_RESOLUTION;
}

static void runtime_perf_flag_set(vn_u32* perf_flags, vn_u32 flag, int enabled) {
    if (perf_flags == (vn_u32*)0) {
        return;
    }
    if (enabled != VN_FALSE) {
        *perf_flags |= flag;
    } else {
        *perf_flags &= ~flag;
    }
}

static void runtime_dirty_stats_reset(VNRuntimeSession* session);

static void runtime_render_cache_invalidate(VNRuntimeSession* session) {
    if (session == (VNRuntimeSession*)0) {
        return;
    }
    session->frame_reuse_valid = 0u;
    session->op_cache_stamp = 0u;
    (void)memset(session->op_cache, 0, sizeof(session->op_cache));
}

static void runtime_dirty_planner_reconfigure(VNRuntimeSession* session,
                                              vn_u16 width,
                                              vn_u16 height) {
    vn_u32 dirty_word_count;

    if (session == (VNRuntimeSession*)0) {
        return;
    }

    if (session->dirty_bits != (vn_u32*)0) {
        free(session->dirty_bits);
        session->dirty_bits = (vn_u32*)0;
    }

    dirty_word_count = 0u;
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_DIRTY_TILE) != VN_FALSE) {
        dirty_word_count = vn_dirty_word_count(width, height);
        if (dirty_word_count != 0u) {
            session->dirty_bits = (vn_u32*)malloc((size_t)dirty_word_count * sizeof(vn_u32));
            if (session->dirty_bits == (vn_u32*)0) {
                runtime_perf_flag_set(&session->perf_flags, VN_RUNTIME_PERF_DIRTY_TILE, VN_FALSE);
                dirty_word_count = 0u;
                if (session->emit_logs != 0u) {
                    (void)fprintf(stderr, "dirty tile planner disabled: scratch alloc failed\n");
                }
            }
        }
    }

    vn_dirty_planner_init(&session->dirty_planner, width, height, session->dirty_bits, dirty_word_count);
    runtime_dirty_stats_reset(session);
}

static int runtime_renderer_reconfigure(VNRuntimeSession* session,
                                        vn_u16 width,
                                        vn_u16 height) {
    RendererConfig next_cfg;
    vn_u16 old_width;
    vn_u16 old_height;
    int rc;

    if (session == (VNRuntimeSession*)0 || width == 0u || height == 0u) {
        return VN_E_INVALID_ARG;
    }
    if (session->renderer_cfg.width == width && session->renderer_cfg.height == height) {
        return VN_OK;
    }

    old_width = session->renderer_cfg.width;
    old_height = session->renderer_cfg.height;
    next_cfg = session->renderer_cfg;
    next_cfg.width = width;
    next_cfg.height = height;

    if (session->renderer_ready != VN_FALSE) {
        renderer_shutdown();
        session->renderer_ready = VN_FALSE;
    }

    rc = renderer_init(&next_cfg);
    if (rc != VN_OK) {
        session->renderer_cfg.width = old_width;
        session->renderer_cfg.height = old_height;
        rc = renderer_init(&session->renderer_cfg);
        if (rc == VN_OK) {
            session->renderer_ready = VN_TRUE;
        }
        return VN_E_RENDER_STATE;
    }

    session->renderer_cfg = next_cfg;
    session->renderer_ready = VN_TRUE;
    runtime_render_cache_invalidate(session);
    runtime_dirty_planner_reconfigure(session, width, height);
    return VN_OK;
}

static int runtime_dynamic_resolution_maybe_switch(VNRuntimeSession* session,
                                                   double frame_ms) {
    vn_u32 next_tier;
    vn_u32 current_tier;
    vn_u32 dirty_tile_count;
    vn_u32 dirty_rect_count;
    vn_u32 dirty_full_redraw;
    double window_p95_ms;
    const VNDynResTier* current_dims;
    const VNDynResTier* next_dims;
    int rc;

    if (session == (VNRuntimeSession*)0) {
        return VN_OK;
    }
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_DYNAMIC_RESOLUTION) == VN_FALSE) {
        return VN_OK;
    }

    next_tier = 0u;
    window_p95_ms = 0.0;
    if (vn_dynres_should_switch(&session->dynamic_resolution, frame_ms, &next_tier, &window_p95_ms) == 0) {
        return VN_OK;
    }

    current_tier = vn_dynres_get_current_tier(&session->dynamic_resolution);
    current_dims = vn_dynres_get_current_dims(&session->dynamic_resolution);
    next_dims = vn_dynres_get_tier(&session->dynamic_resolution, next_tier);
    if (current_dims == (const VNDynResTier*)0 || next_dims == (const VNDynResTier*)0) {
        vn_dynres_reset_history(&session->dynamic_resolution);
        return VN_OK;
    }

    dirty_tile_count = session->dirty_tile_count;
    dirty_rect_count = session->dirty_rect_count;
    dirty_full_redraw = session->dirty_full_redraw;

    rc = runtime_renderer_reconfigure(session, next_dims->width, next_dims->height);
    if (rc != VN_OK) {
        vn_dynres_reset_history(&session->dynamic_resolution);
        if (session->emit_logs != 0u) {
            (void)fprintf(stderr,
                          "WARN perf resolution_switch_failed from=%s to=%s rc=%d\n",
                          vn_dynres_tier_name(current_tier),
                          vn_dynres_tier_name(next_tier),
                          rc);
        }
        return rc;
    }

    session->dirty_tile_count = dirty_tile_count;
    session->dirty_rect_count = dirty_rect_count;
    session->dirty_full_redraw = dirty_full_redraw;
    (void)vn_dynres_apply_tier(&session->dynamic_resolution, next_tier);
    if (session->emit_logs != 0u) {
        (void)printf("INFO perf resolution_switch from=%s to=%s resolution=%ux%u p95_ms=%.3f\n",
                     vn_dynres_tier_name(current_tier),
                     vn_dynres_tier_name(next_tier),
                     (unsigned int)next_dims->width,
                     (unsigned int)next_dims->height,
                     window_p95_ms);
    }
    return VN_OK;
}

static void runtime_dirty_stats_reset(VNRuntimeSession* session) {
    if (session == (VNRuntimeSession*)0) {
        return;
    }
    session->dirty_tile_count = 0u;
    session->dirty_rect_count = 0u;
    session->dirty_full_redraw = 0u;
    vn_dirty_plan_reset(&session->dirty_plan);
}

static void runtime_dirty_plan_seed(VNRuntimeSession* session,
                                    VNDirtyPlan* plan) {
    VNDirtyPlannerState* state;

    if (session == (VNRuntimeSession*)0 || plan == (VNDirtyPlan*)0) {
        return;
    }
    state = &session->dirty_planner;
    vn_dirty_plan_reset(plan);
    plan->width = state->width;
    plan->height = state->height;
    plan->tiles_x = state->tiles_x;
    plan->tiles_y = state->tiles_y;
    plan->bit_word_count = state->bit_word_count;
}

static void runtime_dirty_plan_force_full_redraw(VNRuntimeSession* session,
                                                 VNDirtyPlan* plan) {
    vn_u32 total_tiles;

    if (session == (VNRuntimeSession*)0 || plan == (VNDirtyPlan*)0) {
        return;
    }
    runtime_dirty_plan_seed(session, plan);
    total_tiles = (vn_u32)plan->tiles_x * (vn_u32)plan->tiles_y;
    plan->valid = 1u;
    plan->full_redraw = 1u;
    plan->dirty_tile_count = total_tiles;
    plan->dirty_rect_count = 0u;
    if (plan->width != 0u && plan->height != 0u) {
        plan->rects[0].x = 0u;
        plan->rects[0].y = 0u;
        plan->rects[0].w = plan->width;
        plan->rects[0].h = plan->height;
        plan->dirty_rect_count = 1u;
    }
}

static int runtime_dirty_plan_clear_equal(const VNRenderOp* a,
                                          const VNRenderOp* b) {
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

static int runtime_prepare_dirty_plan_fast_path(VNRuntimeSession* session,
                                                const VNRenderOp* ops,
                                                vn_u32 op_count) {
    VNDirtyPlannerState* state;
    vn_u32 i;

    if (session == (VNRuntimeSession*)0 || (ops == (const VNRenderOp*)0 && op_count != 0u)) {
        return VN_FALSE;
    }
    state = &session->dirty_planner;
    if (state->width == 0u || state->height == 0u || state->dirty_bits == (vn_u32*)0) {
        return VN_FALSE;
    }
    if (state->bit_word_count < vn_dirty_word_count(state->width, state->height)) {
        return VN_FALSE;
    }
    if (state->valid == 0u || state->prev_op_count != op_count || op_count == 0u) {
        runtime_dirty_plan_force_full_redraw(session, &session->dirty_plan);
        return VN_TRUE;
    }
    if (ops[0].op != VN_OP_CLEAR || state->prev_ops[0].op != VN_OP_CLEAR) {
        runtime_dirty_plan_force_full_redraw(session, &session->dirty_plan);
        return VN_TRUE;
    }
    if (runtime_dirty_plan_clear_equal(&ops[0], &state->prev_ops[0]) == VN_FALSE) {
        runtime_dirty_plan_force_full_redraw(session, &session->dirty_plan);
        return VN_TRUE;
    }
    for (i = 1u; i < op_count; ++i) {
        if (ops[i].op == VN_OP_FADE || state->prev_ops[i].op == VN_OP_FADE) {
            runtime_dirty_plan_force_full_redraw(session, &session->dirty_plan);
            return VN_TRUE;
        }
        if (ops[i].op != state->prev_ops[i].op) {
            runtime_dirty_plan_force_full_redraw(session, &session->dirty_plan);
            return VN_TRUE;
        }
    }
    return VN_FALSE;
}

static void runtime_dirty_planner_invalidate(VNRuntimeSession* session) {
    if (session == (VNRuntimeSession*)0) {
        return;
    }
    vn_dirty_planner_invalidate(&session->dirty_planner);
    runtime_dirty_stats_reset(session);
}

static void runtime_prepare_dirty_plan(VNRuntimeSession* session,
                                       const VNRenderOp* ops,
                                       vn_u32 op_count) {
    int rc;

    if (session == (VNRuntimeSession*)0) {
        return;
    }
    runtime_dirty_stats_reset(session);
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_DIRTY_TILE) == VN_FALSE) {
        return;
    }
    if (runtime_prepare_dirty_plan_fast_path(session, ops, op_count) == VN_FALSE) {
        rc = vn_dirty_planner_build(&session->dirty_planner, ops, op_count, &session->dirty_plan);
        if (rc != VN_OK) {
            runtime_dirty_planner_invalidate(session);
            return;
        }
    }
    session->dirty_tile_count = session->dirty_plan.dirty_tile_count;
    session->dirty_rect_count = session->dirty_plan.dirty_rect_count;
    session->dirty_full_redraw = session->dirty_plan.full_redraw;
    session->dirty_tile_frames += 1u;
    session->dirty_tile_total += session->dirty_plan.dirty_tile_count;
    session->dirty_rect_total += session->dirty_plan.dirty_rect_count;
    if (session->dirty_plan.full_redraw != 0u) {
        session->dirty_full_redraws += 1u;
    }
}

static void runtime_commit_dirty_plan(VNRuntimeSession* session,
                                      const VNRenderOp* ops,
                                      vn_u32 op_count) {
    if (session == (VNRuntimeSession*)0) {
        return;
    }
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_DIRTY_TILE) == VN_FALSE) {
        runtime_dirty_planner_invalidate(session);
        return;
    }
    if (session->dirty_plan.full_redraw != 0u) {
        vn_dirty_planner_commit_full_redraw(&session->dirty_planner, ops, op_count);
        return;
    }
    vn_dirty_planner_commit(&session->dirty_planner, ops, op_count);
}

static void runtime_submit_render_ops(VNRuntimeSession* session,
                                      const VNRenderOp* ops,
                                      vn_u32 op_count) {
    VNRenderDirtySubmit dirty_submit;

    if (session == (VNRuntimeSession*)0) {
        return;
    }

    renderer_begin_frame();
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_DIRTY_TILE) != VN_FALSE &&
        session->dirty_plan.valid != 0u &&
        session->dirty_plan.full_redraw == 0u) {
        dirty_submit.width = session->dirty_plan.width;
        dirty_submit.height = session->dirty_plan.height;
        dirty_submit.rect_count = session->dirty_plan.dirty_rect_count;
        dirty_submit.full_redraw = session->dirty_plan.full_redraw;
        dirty_submit.rects = session->dirty_plan.rects;
        renderer_submit_dirty(ops, op_count, &dirty_submit);
    } else {
        renderer_submit(ops, op_count);
    }
    renderer_end_frame();
}

static vn_u32 runtime_render_sprite_phase(const VNRuntimeState* state) {
    if (state == (const VNRuntimeState*)0) {
        return 0u;
    }
    return state->frame_index % 160u;
}

static vn_u32 runtime_render_fade_phase(const VNRuntimeState* state) {
    if (state == (const VNRuntimeState*)0) {
        return 0u;
    }
    return state->frame_index & 0x3Fu;
}

static void runtime_render_key_init(RenderOpCacheKey* out_key, const VNRuntimeState* state) {
    vn_u32 scene_id;
    vn_u32 text_flags;

    if (out_key == (RenderOpCacheKey*)0) {
        return;
    }
    (void)memset(out_key, 0, sizeof(*out_key));
    if (state == (const VNRuntimeState*)0) {
        return;
    }

    scene_id = state->scene_id;
    text_flags = 0u;
    if (state->text_speed_ms > 0u) {
        text_flags |= 1u;
    }
    if (state->choice_count > 0u) {
        text_flags |= 2u;
    }
    if (state->vm_error != 0u) {
        text_flags |= 4u;
    }
    if (state->choice_selected_index > 0u) {
        text_flags |= 8u;
    }

    if (scene_id == VN_SCENE_S10) {
        out_key->op_count = 6u;
    } else if (state->vm_fade_active != 0u || state->vm_waiting != 0u || scene_id == VN_SCENE_S1 || scene_id == VN_SCENE_S3) {
        out_key->op_count = 4u;
    } else {
        out_key->op_count = 3u;
    }
    out_key->clear_gray = state->clear_color & 0xFFu;
    out_key->clear_flags = (state->resource_count > 0u) ? 1u : 0u;
    out_key->scene_id = scene_id;
    out_key->sprite_flags = (state->se_id != 0u) ? 1u : 0u;
    if (state->text_id != 0u) {
        out_key->text_tex_id = state->text_id;
    } else {
        out_key->text_tex_id = 100u + scene_id;
    }
    out_key->text_flags = text_flags;
    out_key->text_alpha = (state->vm_ended != 0u) ? 180u : 255u;

    if (out_key->op_count > 3u) {
        if (state->vm_fade_active != 0u) {
            out_key->fade_flags = 2u;
            out_key->fade_mask = state->fade_layer_mask & 0xFFFFu;
        } else {
            out_key->fade_flags = (state->vm_waiting != 0u) ? 1u : 0u;
            out_key->fade_mask = 0u;
        }
    }
}

static void runtime_render_patch_cached_ops(const VNRuntimeState* state,
                                            VNRenderOp* ops,
                                            vn_u32 op_count) {
    vn_u32 i;
    vn_u32 fade_phase;
    vn_i16 sprite_x;

    if (state == (const VNRuntimeState*)0 ||
        ops == (VNRenderOp*)0 ||
        op_count == 0u) {
        return;
    }

    sprite_x = (vn_i16)(40 + runtime_render_sprite_phase(state));
    fade_phase = runtime_render_fade_phase(state);
    for (i = 0u; i < op_count; ++i) {
        if (ops[i].op == VN_OP_SPRITE) {
            ops[i].x = sprite_x;
            ops[i].flags = (vn_u8)(state->se_id != 0u ? 1u : 0u);
        } else if (ops[i].op == VN_OP_FADE) {
            if (state->vm_fade_active != 0u) {
                ops[i].tex_id = (vn_u16)(state->fade_layer_mask & 0xFFFFu);
                ops[i].alpha = (vn_u8)(state->fade_alpha & 0xFFu);
                ops[i].flags = 2u;
            } else {
                ops[i].tex_id = 0u;
                ops[i].alpha = (vn_u8)(state->vm_waiting != 0u ? (120u + fade_phase) : (fade_phase * 3u));
                ops[i].flags = (vn_u8)(state->vm_waiting != 0u ? 1u : 0u);
            }
        }
    }
}

static vn_u32 runtime_render_key_hash(const RenderOpCacheKey* key_data) {
    vn_u32 hash;
    vn_u32 value;

    if (key_data == (const RenderOpCacheKey*)0) {
        return 0u;
    }

    hash = 2166136261u;
#define VN_HASH_VALUE(expr)     value = (vn_u32)(expr);     hash ^= value;     hash *= 16777619u
    VN_HASH_VALUE(key_data->op_count);
    VN_HASH_VALUE(key_data->clear_gray);
    VN_HASH_VALUE(key_data->clear_flags);
    VN_HASH_VALUE(key_data->scene_id);
    VN_HASH_VALUE(key_data->sprite_flags);
    VN_HASH_VALUE(key_data->text_tex_id);
    VN_HASH_VALUE(key_data->text_flags);
    VN_HASH_VALUE(key_data->text_alpha);
    VN_HASH_VALUE(key_data->fade_flags);
    VN_HASH_VALUE(key_data->fade_mask);
#undef VN_HASH_VALUE
    return hash;
}

static int runtime_render_key_equal(const RenderOpCacheKey* a, const RenderOpCacheKey* b) {
    if (a == (const RenderOpCacheKey*)0 || b == (const RenderOpCacheKey*)0) {
        return VN_FALSE;
    }
    return (a->op_count == b->op_count &&
            a->clear_gray == b->clear_gray &&
            a->clear_flags == b->clear_flags &&
            a->scene_id == b->scene_id &&
            a->sprite_flags == b->sprite_flags &&
            a->text_tex_id == b->text_tex_id &&
            a->text_flags == b->text_flags &&
            a->text_alpha == b->text_alpha &&
            a->fade_flags == b->fade_flags &&
            a->fade_mask == b->fade_mask) ? VN_TRUE : VN_FALSE;
}

static int runtime_frame_reuse_eligible(const VNRuntimeState* state) {
    if (state == (const VNRuntimeState*)0) {
        return VN_FALSE;
    }
    if (state->vm_fade_active != 0u) {
        return VN_FALSE;
    }
    if (state->se_id != 0u) {
        return VN_FALSE;
    }
    return VN_TRUE;
}

static int runtime_prepare_frame_reuse(VNRuntimeSession* session,
                                       const VNRuntimeState* state,
                                       RenderOpCacheKey* out_key,
                                       int* out_reuse_hit) {
    if (out_reuse_hit != (int*)0) {
        *out_reuse_hit = VN_FALSE;
    }
    if (session == (VNRuntimeSession*)0 ||
        state == (const VNRuntimeState*)0 ||
        out_key == (RenderOpCacheKey*)0) {
        return VN_FALSE;
    }
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_FRAME_REUSE) == VN_FALSE) {
        session->frame_reuse_valid = 0u;
        return VN_FALSE;
    }
    if (runtime_frame_reuse_eligible(state) == VN_FALSE) {
        session->frame_reuse_valid = 0u;
        return VN_FALSE;
    }

    runtime_render_key_init(out_key, state);
    if (session->frame_reuse_valid != 0u &&
        runtime_render_key_equal(&session->frame_reuse_key, out_key) != VN_FALSE) {
        session->frame_reuse_hits += 1u;
        if (out_reuse_hit != (int*)0) {
            *out_reuse_hit = VN_TRUE;
        }
        return VN_TRUE;
    }

    session->frame_reuse_valid = 0u;
    session->frame_reuse_misses += 1u;
    return VN_TRUE;
}

static void runtime_commit_frame_reuse(VNRuntimeSession* session,
                                       const RenderOpCacheKey* key_data) {
    if (session == (VNRuntimeSession*)0 || key_data == (const RenderOpCacheKey*)0) {
        return;
    }
    session->frame_reuse_key = *key_data;
    session->frame_reuse_valid = 1u;
}

static int runtime_build_render_ops_cached(VNRuntimeSession* session,
                                           const VNRuntimeState* state,
                                           VNRenderOp* out_ops,
                                           vn_u32* io_count,
                                           int* out_cache_hit) {
    RenderOpCacheKey render_key;
    vn_u32 key;
    vn_u32 i;
    vn_u32 victim_index;
    vn_u32 victim_stamp;
    int victim_found;

    if (session == (VNRuntimeSession*)0 ||
        state == (const VNRuntimeState*)0 ||
        out_ops == (VNRenderOp*)0 ||
        io_count == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }
    if (out_cache_hit != (int*)0) {
        *out_cache_hit = VN_FALSE;
    }
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_OP_CACHE) == VN_FALSE) {
        return build_render_ops(state, out_ops, io_count);
    }

    runtime_render_key_init(&render_key, state);
    key = runtime_render_key_hash(&render_key);
    victim_index = 0u;
    victim_stamp = 0u;
    victim_found = VN_FALSE;

    for (i = 0u; i < VN_OP_CACHE_CAP; ++i) {
        RenderOpCacheEntry* entry;

        entry = &session->op_cache[i];
        if (entry->valid != 0u &&
            entry->key == key &&
            runtime_render_key_equal(&entry->render_key, &render_key) != VN_FALSE) {
            if (*io_count < entry->op_count) {
                *io_count = entry->op_count;
                return VN_E_NOMEM;
            }
            (void)memcpy(out_ops, entry->ops, (size_t)entry->op_count * sizeof(VNRenderOp));
            runtime_render_patch_cached_ops(state, out_ops, entry->op_count);
            *io_count = entry->op_count;
            session->op_cache_stamp += 1u;
            entry->stamp = session->op_cache_stamp;
            session->op_cache_hits += 1u;
            if (out_cache_hit != (int*)0) {
                *out_cache_hit = VN_TRUE;
            }
            return VN_OK;
        }
        if (entry->valid == 0u) {
            if (victim_found == VN_FALSE) {
                victim_index = i;
                victim_found = VN_TRUE;
            }
            continue;
        }
        if (victim_found == VN_FALSE || entry->stamp < victim_stamp) {
            victim_index = i;
            victim_stamp = entry->stamp;
            victim_found = VN_TRUE;
        }
    }

    session->op_cache_misses += 1u;
    {
        int rc;
        vn_u32 built_count;
        RenderOpCacheEntry* entry;

        built_count = *io_count;
        rc = build_render_ops(state, out_ops, &built_count);
        *io_count = built_count;
        if (rc != VN_OK) {
            return rc;
        }
        if (victim_found == VN_FALSE) {
            return VN_OK;
        }

        entry = &session->op_cache[victim_index];
        entry->valid = 1u;
        entry->key = key;
        entry->render_key = render_key;
        entry->op_count = built_count;
        (void)memcpy(entry->ops, out_ops, (size_t)built_count * sizeof(VNRenderOp));
        session->op_cache_stamp += 1u;
        entry->stamp = session->op_cache_stamp;
    }
    return VN_OK;
}

static void keyboard_init(KeyboardInput* kb) {
    if (kb == (KeyboardInput*)0) {
        return;
    }
    kb->enabled = VN_FALSE;
    kb->active = VN_FALSE;
    kb->quit_requested = VN_FALSE;
#if !defined(_WIN32)
    kb->old_flags = 0;
#endif
}

static int runtime_apply_key_code(vn_u32 key_code,
                                  vn_u8* out_choice,
                                  int* out_has_choice,
                                  int* out_toggle_trace,
                                  int* out_quit) {
    if (key_code >= (vn_u32)'1' && key_code <= (vn_u32)'9') {
        if (out_choice != (vn_u8*)0) {
            *out_choice = (vn_u8)(key_code - (vn_u32)'1');
        }
        if (out_has_choice != (int*)0) {
            *out_has_choice = VN_TRUE;
        }
        return VN_OK;
    }
    if (key_code == (vn_u32)'t' || key_code == (vn_u32)'T') {
        if (out_toggle_trace != (int*)0) {
            *out_toggle_trace = VN_TRUE;
        }
        return VN_OK;
    }
    if (key_code == (vn_u32)'q' || key_code == (vn_u32)'Q') {
        if (out_quit != (int*)0) {
            *out_quit = VN_TRUE;
        }
        return VN_OK;
    }
    return VN_E_UNSUPPORTED;
}

static int keyboard_enable(KeyboardInput* kb) {
#if defined(_WIN32)
    if (kb == (KeyboardInput*)0) {
        return VN_E_INVALID_ARG;
    }
    if (kb->enabled == VN_FALSE) {
        return VN_OK;
    }
    if (kb->active == VN_TRUE) {
        return VN_OK;
    }
    kb->active = VN_TRUE;
    return VN_OK;
#else
    struct termios raw;
    int flags;

    if (kb == (KeyboardInput*)0) {
        return VN_E_INVALID_ARG;
    }
    if (kb->enabled == VN_FALSE) {
        return VN_OK;
    }
    if (kb->active == VN_TRUE) {
        return VN_OK;
    }
    if (isatty(0) == 0) {
        return VN_E_UNSUPPORTED;
    }
    if (tcgetattr(0, &kb->old_termios) != 0) {
        return VN_E_IO;
    }

    raw = kb->old_termios;
    raw.c_lflag = (tcflag_t)(raw.c_lflag & (tcflag_t)(~(ICANON | ECHO)));
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &raw) != 0) {
        return VN_E_IO;
    }

    flags = fcntl(0, F_GETFL, 0);
    if (flags < 0) {
        (void)tcsetattr(0, TCSANOW, &kb->old_termios);
        return VN_E_IO;
    }
    kb->old_flags = flags;
    if (fcntl(0, F_SETFL, flags | O_NONBLOCK) != 0) {
        (void)tcsetattr(0, TCSANOW, &kb->old_termios);
        return VN_E_IO;
    }

    kb->active = VN_TRUE;
    return VN_OK;
#endif
}

static void keyboard_disable(KeyboardInput* kb) {
#if defined(_WIN32)
    if (kb == (KeyboardInput*)0) {
        return;
    }
    kb->active = VN_FALSE;
#else
    if (kb == (KeyboardInput*)0 || kb->active == VN_FALSE) {
        return;
    }
    (void)tcsetattr(0, TCSANOW, &kb->old_termios);
    (void)fcntl(0, F_SETFL, kb->old_flags);
    kb->active = VN_FALSE;
#endif
}

static void keyboard_poll(KeyboardInput* kb,
                          vn_u8* out_choice,
                          int* out_has_choice,
                          int* out_toggle_trace,
                          int* out_quit) {
    if (out_has_choice != (int*)0) {
        *out_has_choice = VN_FALSE;
    }
    if (out_toggle_trace != (int*)0) {
        *out_toggle_trace = VN_FALSE;
    }
    if (out_quit != (int*)0) {
        *out_quit = VN_FALSE;
    }

    if (kb == (KeyboardInput*)0 || kb->active == VN_FALSE) {
        return;
    }

#if !defined(_WIN32)
    for (;;) {
        unsigned char ch;
        ssize_t read_count;
        int rc;

        read_count = read(0, &ch, 1u);
        if (read_count <= 0) {
            if (read_count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                kb->quit_requested = VN_TRUE;
                if (out_quit != (int*)0) {
                    *out_quit = VN_TRUE;
                }
            }
            break;
        }

        rc = runtime_apply_key_code((vn_u32)ch,
                                    out_choice,
                                    out_has_choice,
                                    out_toggle_trace,
                                    out_quit);
        if (rc == VN_OK && out_quit != (int*)0 && *out_quit != VN_FALSE) {
            kb->quit_requested = VN_TRUE;
        }
    }
#else
    while (_kbhit() != 0) {
        int ch;
        int rc;

        ch = _getch();
        if (ch == 0 || ch == 224) {
            if (_kbhit() != 0) {
                (void)_getch();
            }
            continue;
        }

        rc = runtime_apply_key_code((vn_u32)(unsigned char)ch,
                                    out_choice,
                                    out_has_choice,
                                    out_toggle_trace,
                                    out_quit);
        if (rc == VN_OK && out_quit != (int*)0 && *out_quit != VN_FALSE) {
            kb->quit_requested = VN_TRUE;
        }
    }
#endif
}

static int runtime_session_inject_key_code(VNRuntimeSession* session, vn_u32 key_code) {
    vn_u8 choice_index;
    int has_choice;
    int toggle_trace;
    int quit_now;
    int rc;

    if (session == (VNRuntimeSession*)0) {
        return VN_E_INVALID_ARG;
    }

    choice_index = 0u;
    has_choice = VN_FALSE;
    toggle_trace = VN_FALSE;
    quit_now = VN_FALSE;
    rc = runtime_apply_key_code(key_code,
                                &choice_index,
                                &has_choice,
                                &toggle_trace,
                                &quit_now);
    if (rc != VN_OK) {
        return rc;
    }
    if (has_choice != VN_FALSE) {
        session->injected_choice_index = choice_index;
        session->injected_has_choice = VN_TRUE;
    }
    if (toggle_trace != VN_FALSE) {
        session->injected_trace_toggle_count += 1u;
    }
    if (quit_now != VN_FALSE) {
        session->injected_quit = VN_TRUE;
    }
    return VN_OK;
}

static void runtime_session_merge_injected_input(VNRuntimeSession* session,
                                                 vn_u8* io_choice,
                                                 int* io_has_choice,
                                                 int* io_toggle_trace,
                                                 int* io_quit) {
    vn_u32 toggle_now;

    if (session == (VNRuntimeSession*)0) {
        return;
    }

    if (session->injected_has_choice != VN_FALSE) {
        if (io_choice != (vn_u8*)0) {
            *io_choice = session->injected_choice_index;
        }
        if (io_has_choice != (int*)0) {
            *io_has_choice = VN_TRUE;
        }
        session->injected_has_choice = VN_FALSE;
    }

    toggle_now = 0u;
    if (io_toggle_trace != (int*)0 && *io_toggle_trace != VN_FALSE) {
        toggle_now = 1u;
    }
    toggle_now ^= (session->injected_trace_toggle_count & 1u);
    if (io_toggle_trace != (int*)0) {
        *io_toggle_trace = (toggle_now != 0u) ? VN_TRUE : VN_FALSE;
    }
    session->injected_trace_toggle_count = 0u;

    if (session->injected_quit != VN_FALSE) {
        if (io_quit != (int*)0) {
            *io_quit = VN_TRUE;
        }
        session->injected_quit = VN_FALSE;
    }
}

static void fade_player_init(FadePlayer* fade) {
    if (fade == (FadePlayer*)0) {
        return;
    }
    fade->seen_serial = 0u;
    fade->active = 0u;
    fade->layer_mask = 0u;
    fade->alpha_current = 0u;
    fade->alpha_start = 0u;
    fade->alpha_target = 0u;
    fade->duration_ms = 0u;
    fade->elapsed_ms = 0u;
}

static void fade_player_step(FadePlayer* fade, const VNState* vm, vn_u32 dt_ms) {
    vn_u32 serial_now;

    if (fade == (FadePlayer*)0 || vm == (const VNState*)0) {
        return;
    }

    serial_now = vm_fade_serial(vm);
    if (serial_now != fade->seen_serial) {
        fade->seen_serial = serial_now;
        fade->layer_mask = vm_fade_layer_mask(vm);
        fade->alpha_start = fade->alpha_current;
        fade->alpha_target = vm_fade_target_alpha(vm);
        fade->duration_ms = vm_fade_duration_ms(vm);
        fade->elapsed_ms = 0u;
        if (fade->duration_ms == 0u) {
            fade->alpha_current = fade->alpha_target;
            fade->active = 0u;
        } else {
            fade->active = 1u;
        }
    }

    if (fade->active != 0u) {
        vn_u32 next_elapsed;
        if (dt_ms >= (vn_u32)fade->duration_ms - fade->elapsed_ms) {
            fade->elapsed_ms = (vn_u32)fade->duration_ms;
            fade->alpha_current = fade->alpha_target;
            fade->active = 0u;
        } else {
            int diff;
            int alpha_value;
            next_elapsed = fade->elapsed_ms + dt_ms;
            fade->elapsed_ms = next_elapsed;
            diff = (int)fade->alpha_target - (int)fade->alpha_start;
            alpha_value = (int)fade->alpha_start + (diff * (int)fade->elapsed_ms) / (int)fade->duration_ms;
            if (alpha_value < 0) {
                alpha_value = 0;
            } else if (alpha_value > 255) {
                alpha_value = 255;
            }
            fade->alpha_current = (vn_u8)alpha_value;
        }
    }
}

static int parse_u32_range(const char* text, long min_value, long max_value, vn_u32* out_value) {
    long value;
    char* end_ptr;

    if (text == (const char*)0 || out_value == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }

    value = strtol(text, &end_ptr, 10);
    if (end_ptr == text || *end_ptr != '\0' || value < min_value || value > max_value) {
        return VN_E_FORMAT;
    }

    *out_value = (vn_u32)value;
    return VN_OK;
}

static int parse_resolution(const char* text, vn_u16* out_w, vn_u16* out_h) {
    const char* x_ptr;
    long w;
    long h;
    char* end_ptr;

    if (text == (const char*)0 || out_w == (vn_u16*)0 || out_h == (vn_u16*)0) {
        return VN_E_INVALID_ARG;
    }
    x_ptr = strchr(text, 'x');
    if (x_ptr == (const char*)0) {
        return VN_E_FORMAT;
    }

    w = strtol(text, &end_ptr, 10);
    if (end_ptr != x_ptr || w <= 0 || w > 65535) {
        return VN_E_FORMAT;
    }
    h = strtol(x_ptr + 1, &end_ptr, 10);
    if (*end_ptr != '\0' || h <= 0 || h > 65535) {
        return VN_E_FORMAT;
    }

    *out_w = (vn_u16)w;
    *out_h = (vn_u16)h;
    return VN_OK;
}

static vn_u32 parse_backend_flag(const char* value) {
    if (value == (const char*)0) {
        return 0u;
    }
    if (strcmp(value, "scalar") == 0) {
        return VN_RENDERER_FLAG_FORCE_SCALAR;
    }
    if (strcmp(value, "avx2") == 0) {
        return VN_RENDERER_FLAG_FORCE_AVX2;
    }
    if (strcmp(value, "avx2_asm") == 0) {
        return VN_RENDERER_FLAG_FORCE_AVX2_ASM;
    }
    if (strcmp(value, "neon") == 0) {
        return VN_RENDERER_FLAG_FORCE_NEON;
    }
    if (strcmp(value, "rvv") == 0) {
        return VN_RENDERER_FLAG_FORCE_RVV;
    }
    return 0u;
}

static int parse_scene_id(const char* value, vn_u32* out_scene_id) {
    if (value == (const char*)0 || out_scene_id == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }
    if (strcmp(value, "S0") == 0) {
        *out_scene_id = VN_SCENE_S0;
        return VN_OK;
    }
    if (strcmp(value, "S1") == 0) {
        *out_scene_id = VN_SCENE_S1;
        return VN_OK;
    }
    if (strcmp(value, "S2") == 0) {
        *out_scene_id = VN_SCENE_S2;
        return VN_OK;
    }
    if (strcmp(value, "S3") == 0) {
        *out_scene_id = VN_SCENE_S3;
        return VN_OK;
    }
    if (strcmp(value, "S10") == 0) {
        *out_scene_id = VN_SCENE_S10;
        return VN_OK;
    }
    return VN_E_FORMAT;
}

static int parse_toggle_value(const char* value, int* out_enabled) {
    if (value == (const char*)0 || out_enabled == (int*)0) {
        return VN_E_INVALID_ARG;
    }
    if (strcmp(value, "1") == 0 || strcmp(value, "on") == 0 || strcmp(value, "true") == 0) {
        *out_enabled = VN_TRUE;
        return VN_OK;
    }
    if (strcmp(value, "0") == 0 || strcmp(value, "off") == 0 || strcmp(value, "false") == 0) {
        *out_enabled = VN_FALSE;
        return VN_OK;
    }
    return VN_E_FORMAT;
}

static vn_u32 scene_script_res_id(vn_u32 scene_id) {
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

static int parse_choice_seq(const char* text, ChoiceFeed* out_feed) {
    const char* p;

    if (text == (const char*)0 || out_feed == (ChoiceFeed*)0) {
        return VN_E_INVALID_ARG;
    }

    out_feed->count = 0u;
    out_feed->cursor = 0u;
    p = text;

    while (*p != '\0') {
        const char* token_start;
        const char* token_end;
        long value;
        char* parse_end;
        char tmp[16];
        vn_u32 len;

        while (*p == ' ' || *p == ',') {
            p += 1;
        }
        if (*p == '\0') {
            break;
        }

        token_start = p;
        while (*p != '\0' && *p != ',') {
            p += 1;
        }
        token_end = p;

        len = (vn_u32)(token_end - token_start);
        if (len == 0u || len >= (vn_u32)sizeof(tmp)) {
            return VN_E_FORMAT;
        }

        memcpy(tmp, token_start, len);
        tmp[len] = '\0';
        value = strtol(tmp, &parse_end, 10);
        if (parse_end == tmp || *parse_end != '\0' || value < 0l || value > 255l) {
            return VN_E_FORMAT;
        }

        if (out_feed->count >= VN_MAX_CHOICE_SEQ) {
            return VN_E_NOMEM;
        }
        out_feed->items[out_feed->count] = (vn_u8)value;
        out_feed->count += 1u;

        if (*p == ',') {
            p += 1;
        }
    }

    return VN_OK;
}

static int runtime_cli_report_error(const char* trace_id,
                                    int error_code,
                                    const char* message,
                                    const char* arg_name,
                                    const char* arg_value,
                                    int exit_code) {
    (void)fprintf(stderr,
                  "trace_id=%s error_code=%d error_name=%s message=%s",
                  trace_id,
                  error_code,
                  vn_error_name(error_code),
                  message);
    if (arg_name != (const char*)0) {
        (void)fprintf(stderr, " arg=%s", arg_name);
    }
    if (arg_value != (const char*)0) {
        (void)fprintf(stderr, " value=%s", arg_value);
    }
    (void)fprintf(stderr, "\n");
    return exit_code;
}

static int runtime_cli_report_missing_value(const char* arg_name) {
    return runtime_cli_report_error("runtime.cli.arg.missing",
                                    VN_E_INVALID_ARG,
                                    "missing value",
                                    arg_name,
                                    (const char*)0,
                                    2);
}

static int runtime_cli_report_invalid_value(const char* arg_name, const char* arg_value) {
    return runtime_cli_report_error("runtime.cli.arg.invalid",
                                    VN_E_INVALID_ARG,
                                    "invalid value",
                                    arg_name,
                                    arg_value,
                                    2);
}

static int runtime_cli_report_invalid_combo(const char* arg_name, const char* arg_value) {
    return runtime_cli_report_error("runtime.cli.arg.invalid",
                                    VN_E_INVALID_ARG,
                                    "invalid argument combination",
                                    arg_name,
                                    arg_value,
                                    2);
}

static int load_scene_script(const VNPak* pak, vn_u32 scene_id, vn_u8** out_buf, vn_u32* out_size) {
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

static void state_reset_frame_events(VNRuntimeState* state) {
    state->se_id = 0u;
    state->choice_count = 0u;
    state->choice_text_id = 0u;
}

static void state_from_vm(VNRuntimeState* state, VNState* vm) {
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

static void state_apply_fade(VNRuntimeState* state, const FadePlayer* fade) {
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

static void state_init_defaults(VNRuntimeState* state) {
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

static const char* scene_name_from_id(vn_u32 scene_id) {
    if (scene_id == VN_SCENE_S1) {
        return "S1";
    }
    if (scene_id == VN_SCENE_S2) {
        return "S2";
    }
    if (scene_id == VN_SCENE_S3) {
        return "S3";
    }
    if (scene_id == VN_SCENE_S10) {
        return "S10";
    }
    return "S0";
}

static void runtime_result_write(const VNRuntimeSession* session, VNRunResult* out_result) {
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

static void runtime_result_publish(const VNRuntimeSession* session) {
    runtime_result_write(session, &g_last_run_result);
}

static void runtime_session_cleanup(VNRuntimeSession* session) {
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

int vn_runtime_session_create(const VNRunConfig* cfg, VNRuntimeSession** out_session) {
    VNRunConfig cfg_local;
    const VNRunConfig* active_cfg;
    VNRuntimeSession* session;
    const char* pack_path;
    const char* scene_name;
    vn_u32 scene_id;
    vn_u32 force_flag;
    vn_u32 i;
    int rc;

    if (out_session == (VNRuntimeSession**)0) {
        return VN_E_INVALID_ARG;
    }
    *out_session = (VNRuntimeSession*)0;
    runtime_result_reset();

    active_cfg = cfg;
    if (active_cfg == (const VNRunConfig*)0) {
        vn_run_config_init(&cfg_local);
        active_cfg = &cfg_local;
    }

    if (active_cfg->choice_seq_count > VN_MAX_CHOICE_SEQ) {
        return VN_E_INVALID_ARG;
    }
    if (active_cfg->frames == 0u || active_cfg->dt_ms > 1000u) {
        return VN_E_INVALID_ARG;
    }
    if (active_cfg->width == 0u || active_cfg->height == 0u) {
        return VN_E_INVALID_ARG;
    }

    pack_path = active_cfg->pack_path;
    if (pack_path == (const char*)0 || pack_path[0] == '\0') {
        pack_path = "assets/demo/demo.vnpak";
    }
    scene_name = active_cfg->scene_name;
    if (scene_name == (const char*)0 || scene_name[0] == '\0') {
        scene_name = "S0";
    }

    rc = parse_scene_id(scene_name, &scene_id);
    if (rc != VN_OK) {
        return rc;
    }

    session = (VNRuntimeSession*)malloc(sizeof(VNRuntimeSession));
    if (session == (VNRuntimeSession*)0) {
        return VN_E_NOMEM;
    }
    (void)memset(session, 0, sizeof(VNRuntimeSession));

    state_init_defaults(&session->state);
    fade_player_init(&session->fade_player);
    keyboard_init(&session->keyboard);
    session->choice_feed.count = 0u;
    session->choice_feed.cursor = 0u;
    session->renderer_cfg.width = active_cfg->width;
    session->renderer_cfg.height = active_cfg->height;
    session->renderer_cfg.flags = VN_RENDERER_FLAG_SIMD;
    force_flag = parse_backend_flag(active_cfg->backend_name);
    if (force_flag != 0u) {
        session->renderer_cfg.flags &= ~(VN_RENDERER_FLAG_FORCE_SCALAR |
                                         VN_RENDERER_FLAG_FORCE_AVX2 |
                                         VN_RENDERER_FLAG_FORCE_NEON |
                                         VN_RENDERER_FLAG_FORCE_RVV |
                                         VN_RENDERER_FLAG_FORCE_AVX2_ASM);
        session->renderer_cfg.flags |= force_flag;
    }

    session->frames_limit = active_cfg->frames;
    session->dt_ms = active_cfg->dt_ms;
    session->trace = active_cfg->trace;
    session->emit_logs = active_cfg->emit_logs;
    session->hold_on_end = active_cfg->hold_on_end;
    session->perf_flags = active_cfg->perf_flags & runtime_supported_perf_flags();
    vn_dynres_init(&session->dynamic_resolution, active_cfg->width, active_cfg->height);
    session->default_choice_index = active_cfg->choice_index;
    session->keyboard.enabled = (active_cfg->keyboard != 0u) ? VN_TRUE : VN_FALSE;
    session->done = VN_FALSE;
    session->exit_code = 0;
    session->summary_emitted = VN_FALSE;
    runtime_dirty_planner_reconfigure(session, session->renderer_cfg.width, session->renderer_cfg.height);
    for (i = 0u; i < active_cfg->choice_seq_count; ++i) {
        session->choice_feed.items[i] = active_cfg->choice_seq[i];
    }
    session->choice_feed.count = active_cfg->choice_seq_count;

    session->state.scene_id = scene_id;
    session->state.clear_color = (vn_u32)(200u + (scene_id * 12u));

    rc = vnpak_open(&session->pak, pack_path);
    if (rc != VN_OK) {
        free(session);
        return rc;
    }
    session->pak_opened = VN_TRUE;
    session->state.resource_count = session->pak.resource_count;

    rc = load_scene_script(&session->pak, session->state.scene_id, &session->script_buf, &session->script_size);
    if (rc != VN_OK) {
        runtime_session_cleanup(session);
        free(session);
        return rc;
    }

    if (vm_init(&session->vm, session->script_buf, session->script_size) != VN_TRUE) {
        runtime_session_cleanup(session);
        free(session);
        return VN_E_FORMAT;
    }
    session->vm_ready = VN_TRUE;
    session->last_choice_serial = vm_choice_serial(&session->vm);

    rc = renderer_init(&session->renderer_cfg);
    if (rc != VN_OK) {
        runtime_session_cleanup(session);
        free(session);
        return rc;
    }
    session->renderer_ready = VN_TRUE;

    rc = keyboard_enable(&session->keyboard);
    if (session->keyboard.enabled == VN_TRUE && rc != VN_OK) {
        runtime_session_cleanup(session);
        free(session);
        return rc;
    }
    if (session->keyboard.active == VN_TRUE && session->emit_logs != 0u) {
        (void)printf("[keyboard] enabled: press 1-9 to select choice, t to toggle trace, q to quit\n");
    }

    runtime_result_publish(session);
    *out_session = session;
    return VN_OK;
}

int vn_runtime_session_step(VNRuntimeSession* session, VNRunResult* out_result) {
    vn_u8 applied_choice;
    vn_u32 choice_serial_now;
    vn_u32 op_count;
    double t_frame_start;
    double t_after_vm;
    double t_after_build;
    double t_after_raster;
    double frame_ms;
    double vm_ms;
    double build_ms;
    double raster_ms;
    double audio_ms;
    double rss_mb;
    int rc;
    int build_cache_hit;
    int frame_reuse_active;
    int frame_reuse_hit;
    int keyboard_has_choice;
    int keyboard_toggle_trace;
    int keyboard_quit;
    int used_choice_seq;
    int switch_rc;
    RenderOpCacheKey frame_reuse_key;

    if (session == (VNRuntimeSession*)0) {
        return VN_E_INVALID_ARG;
    }

    if (session->done == VN_FALSE && session->frames_executed < session->frames_limit) {
        runtime_dirty_stats_reset(session);
        session->state.frame_index = session->frames_executed;
        state_reset_frame_events(&session->state);

        applied_choice = session->default_choice_index;
        keyboard_has_choice = VN_FALSE;
        keyboard_toggle_trace = VN_FALSE;
        keyboard_quit = VN_FALSE;
        used_choice_seq = VN_FALSE;
        keyboard_poll(&session->keyboard,
                      &applied_choice,
                      &keyboard_has_choice,
                      &keyboard_toggle_trace,
                      &keyboard_quit);
        runtime_session_merge_injected_input(session,
                                             &applied_choice,
                                             &keyboard_has_choice,
                                             &keyboard_toggle_trace,
                                             &keyboard_quit);
        if (keyboard_toggle_trace != VN_FALSE) {
            session->trace = (session->trace == 0u) ? 1u : 0u;
        }
        if (keyboard_quit != VN_FALSE) {
            session->done = VN_TRUE;
        } else {
            if (keyboard_has_choice == VN_FALSE &&
                session->choice_feed.count > 0u &&
                session->choice_feed.cursor < session->choice_feed.count) {
                applied_choice = session->choice_feed.items[session->choice_feed.cursor];
                used_choice_seq = VN_TRUE;
            }
            vm_set_choice_index(&session->vm, applied_choice);

            t_frame_start = runtime_now_ms();
            vm_step(&session->vm, session->dt_ms);
            t_after_vm = runtime_now_ms();
            t_after_build = t_after_vm;
            t_after_raster = t_after_vm;
            state_from_vm(&session->state, &session->vm);
            fade_player_step(&session->fade_player, &session->vm, session->dt_ms);
            state_apply_fade(&session->state, &session->fade_player);

            build_cache_hit = VN_FALSE;
            frame_reuse_hit = VN_FALSE;
            frame_reuse_active = runtime_prepare_frame_reuse(session,
                                                             &session->state,
                                                             &frame_reuse_key,
                                                             &frame_reuse_hit);
            if (frame_reuse_hit != VN_FALSE) {
                op_count = session->last_op_count;
                rc = VN_OK;
                t_after_build = runtime_now_ms();
                t_after_raster = t_after_build;
            } else {
                op_count = 16u;
                rc = runtime_build_render_ops_cached(session,
                                                     &session->state,
                                                     session->ops,
                                                     &op_count,
                                                     &build_cache_hit);
                t_after_build = runtime_now_ms();
                if (rc != VN_OK) {
                    (void)fprintf(stderr, "build_render_ops failed rc=%d frame=%u\n",
                                  rc,
                                  (unsigned int)session->state.frame_index);
                    session->exit_code = 1;
                    session->done = VN_TRUE;
                } else {
                    runtime_prepare_dirty_plan(session, session->ops, op_count);
                    runtime_submit_render_ops(session, session->ops, op_count);
                    t_after_raster = runtime_now_ms();
                    runtime_commit_dirty_plan(session, session->ops, op_count);
                    if (frame_reuse_active != VN_FALSE) {
                        runtime_commit_frame_reuse(session, &frame_reuse_key);
                    }
                }
            }
            if (rc == VN_OK) {
                choice_serial_now = vm_choice_serial(&session->vm);
                if (choice_serial_now != session->last_choice_serial) {
                    session->last_choice_serial = choice_serial_now;
                    if (used_choice_seq != VN_FALSE && session->choice_feed.cursor < session->choice_feed.count) {
                        session->choice_feed.cursor += 1u;
                    }
                }

                session->frames_executed += 1u;
                session->last_op_count = op_count;

                vm_ms = t_after_vm - t_frame_start;
                build_ms = t_after_build - t_after_vm;
                raster_ms = t_after_raster - t_after_build;
                frame_ms = t_after_raster - t_frame_start;
                audio_ms = 0.0;
                rss_mb = runtime_rss_mb();

                if (session->trace != 0u && session->emit_logs != 0u) {
                    (void)printf("frame=%u frame_ms=%.3f vm_ms=%.3f build_ms=%.3f raster_ms=%.3f audio_ms=%.3f rss_mb=%.3f "
                                 "text=%u wait=%u end=%u fade=%u fade_remain=%u bgm=%u se=%u choice_count=%u choice_sel=%u choice_text=%u ",
                                 (unsigned int)session->state.frame_index,
                                 frame_ms,
                                 vm_ms,
                                 build_ms,
                                 raster_ms,
                                 audio_ms,
                                 rss_mb,
                                 (unsigned int)session->state.text_id,
                                 (unsigned int)session->state.vm_waiting,
                                 (unsigned int)session->state.vm_ended,
                                 (unsigned int)session->state.fade_alpha,
                                 (unsigned int)session->state.fade_duration_ms,
                                 (unsigned int)session->state.bgm_id,
                                 (unsigned int)session->state.se_id,
                                 (unsigned int)session->state.choice_count,
                                 (unsigned int)session->state.choice_selected_index,
                                 (unsigned int)session->state.choice_text_id);
                    (void)printf("ops=%u frame_reuse_hit=%u frame_reuse_hits=%u frame_reuse_misses=%u op_cache_hit=%u op_cache_hits=%u op_cache_misses=%u ",
                                 (unsigned int)op_count,
                                 (unsigned int)(frame_reuse_hit != VN_FALSE),
                                 (unsigned int)session->frame_reuse_hits,
                                 (unsigned int)session->frame_reuse_misses,
                                 (unsigned int)(build_cache_hit != VN_FALSE),
                                 (unsigned int)session->op_cache_hits,
                                 (unsigned int)session->op_cache_misses);
                    (void)printf("dirty_tiles=%u dirty_rects=%u dirty_full_redraw=%u dirty_tile_frames=%u dirty_tile_total=%u dirty_rect_total=%u dirty_full_redraws=%u "
                                 "render_width=%u render_height=%u dynres_tier=%s dynres_switches=%u\n",
                                 (unsigned int)session->dirty_tile_count,
                                 (unsigned int)session->dirty_rect_count,
                                 (unsigned int)session->dirty_full_redraw,
                                 (unsigned int)session->dirty_tile_frames,
                                 (unsigned int)session->dirty_tile_total,
                                 (unsigned int)session->dirty_rect_total,
                                 (unsigned int)session->dirty_full_redraws,
                                 (unsigned int)session->renderer_cfg.width,
                                 (unsigned int)session->renderer_cfg.height,
                                 vn_dynres_tier_name(vn_dynres_get_current_tier(&session->dynamic_resolution)),
                                 (unsigned int)vn_dynres_get_switch_count(&session->dynamic_resolution));
                }

                switch_rc = runtime_dynamic_resolution_maybe_switch(session, frame_ms);
                if (switch_rc != VN_OK) {
                    session->exit_code = 1;
                    session->done = VN_TRUE;
                }

                if (session->state.vm_error != 0u) {
                    session->exit_code = 1;
                    session->done = VN_TRUE;
                } else if (session->state.vm_ended != 0u && session->hold_on_end == 0u) {
                    session->done = VN_TRUE;
                }
                if (session->frames_executed >= session->frames_limit) {
                    session->done = VN_TRUE;
                }
            }
        }
    } else {
        session->done = VN_TRUE;
    }

    if (session->done != VN_FALSE &&
        session->summary_emitted == VN_FALSE &&
        session->exit_code == 0 &&
        session->emit_logs != 0u) {
        (void)printf("vn_runtime ok trace_id=runtime.run.ok backend=%s resolution=%ux%u scene=%s frames=%u dt=%u resources=%u text=%u wait=%u end=%u "
                     "fade=%u fade_remain=%u bgm=%u se=%u choice=%u choice_sel=%u choice_text=%u err=%u ops=%u keyboard=%u perf_flags=0x%X ",
                     renderer_backend_name(),
                     (unsigned int)session->renderer_cfg.width,
                     (unsigned int)session->renderer_cfg.height,
                     scene_name_from_id(session->state.scene_id),
                     (unsigned int)session->frames_executed,
                     (unsigned int)session->dt_ms,
                     (unsigned int)session->state.resource_count,
                     (unsigned int)session->state.text_id,
                     (unsigned int)session->state.vm_waiting,
                     (unsigned int)session->state.vm_ended,
                     (unsigned int)session->state.fade_alpha,
                     (unsigned int)session->state.fade_duration_ms,
                     (unsigned int)session->state.bgm_id,
                     (unsigned int)session->state.se_id,
                     (unsigned int)session->state.choice_count,
                     (unsigned int)session->state.choice_selected_index,
                     (unsigned int)session->state.choice_text_id,
                     (unsigned int)session->state.vm_error,
                     (unsigned int)session->last_op_count,
                     (unsigned int)session->keyboard.active,
                     (unsigned int)session->perf_flags);
        (void)printf("frame_reuse_hits=%u frame_reuse_misses=%u op_cache_hits=%u op_cache_misses=%u dirty_tiles=%u dirty_rects=%u dirty_full_redraw=%u "
                     "dirty_tile_frames=%u dirty_tile_total=%u dirty_rect_total=%u dirty_full_redraws=%u ",
                     (unsigned int)session->frame_reuse_hits,
                     (unsigned int)session->frame_reuse_misses,
                     (unsigned int)session->op_cache_hits,
                     (unsigned int)session->op_cache_misses,
                     (unsigned int)session->dirty_tile_count,
                     (unsigned int)session->dirty_rect_count,
                     (unsigned int)session->dirty_full_redraw,
                     (unsigned int)session->dirty_tile_frames,
                     (unsigned int)session->dirty_tile_total,
                     (unsigned int)session->dirty_rect_total,
                     (unsigned int)session->dirty_full_redraws);
        (void)printf("dynres_tier=%s dynres_switches=%u\n",
                     vn_dynres_tier_name(vn_dynres_get_current_tier(&session->dynamic_resolution)),
                     (unsigned int)vn_dynres_get_switch_count(&session->dynamic_resolution));
        session->summary_emitted = VN_TRUE;
    }

    runtime_result_publish(session);
    if (out_result != (VNRunResult*)0) {
        *out_result = g_last_run_result;
    }
    if (session->done != VN_FALSE && session->exit_code != 0) {
        return session->exit_code;
    }
    return VN_OK;
}

int vn_runtime_session_is_done(const VNRuntimeSession* session) {
    if (session == (const VNRuntimeSession*)0) {
        return VN_TRUE;
    }
    return (session->done != VN_FALSE) ? VN_TRUE : VN_FALSE;
}

int vn_runtime_session_set_choice(VNRuntimeSession* session, vn_u8 choice_index) {
    if (session == (VNRuntimeSession*)0) {
        return VN_E_INVALID_ARG;
    }
    session->default_choice_index = choice_index;
    return VN_OK;
}

int vn_runtime_session_inject_input(VNRuntimeSession* session, const VNInputEvent* event) {
    if (session == (VNRuntimeSession*)0 || event == (const VNInputEvent*)0) {
        return VN_E_INVALID_ARG;
    }

    if (event->kind == VN_INPUT_KIND_CHOICE) {
        if (event->value0 > 255u) {
            return VN_E_INVALID_ARG;
        }
        session->injected_choice_index = (vn_u8)(event->value0 & 0xFFu);
        session->injected_has_choice = VN_TRUE;
        return VN_OK;
    }
    if (event->kind == VN_INPUT_KIND_KEY) {
        return runtime_session_inject_key_code(session, event->value0);
    }
    if (event->kind == VN_INPUT_KIND_TRACE_TOGGLE) {
        session->injected_trace_toggle_count += 1u;
        return VN_OK;
    }
    if (event->kind == VN_INPUT_KIND_QUIT) {
        session->injected_quit = VN_TRUE;
        return VN_OK;
    }
    return VN_E_UNSUPPORTED;
}

int vn_runtime_session_destroy(VNRuntimeSession* session) {
    if (session == (VNRuntimeSession*)0) {
        return VN_OK;
    }
    runtime_session_cleanup(session);
    free(session);
    return VN_OK;
}

int vn_runtime_run_cli(int argc, char** argv) {
    VNRunConfig run_cfg;
    VNRuntimeSession* session;
    ChoiceFeed choice_feed;
    const char* load_save_path;
    const char* load_save_conflict_arg;
    vn_u32 scene_id;
    vn_u32 rc_u32;
    int i;
    int rc;
    int sleep_between_frames;

    vn_run_config_init(&run_cfg);
    choice_feed.count = 0u;
    choice_feed.cursor = 0u;
    load_save_path = (const char*)0;
    load_save_conflict_arg = (const char*)0;
    session = (VNRuntimeSession*)0;

    for (i = 1; i < argc; ++i) {
        const char* arg;
        arg = argv[i];

        if (strcmp(arg, "--load-save") == 0) {
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--load-save");
            }
            i += 1;
            load_save_path = argv[i];
        } else if (strncmp(arg, "--load-save=", 12) == 0) {
            load_save_path = arg + 12;
        } else if (strcmp(arg, "--backend") == 0) {
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--backend");
            }
            i += 1;
            run_cfg.backend_name = argv[i];
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--backend";
            }
        } else if (strncmp(arg, "--backend=", 10) == 0) {
            run_cfg.backend_name = arg + 10;
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--backend";
            }
        } else if (strcmp(arg, "--resolution") == 0) {
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--resolution");
            }
            i += 1;
            rc = parse_resolution(argv[i], &run_cfg.width, &run_cfg.height);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--resolution", argv[i]);
            }
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--resolution";
            }
        } else if (strncmp(arg, "--resolution=", 13) == 0) {
            rc = parse_resolution(arg + 13, &run_cfg.width, &run_cfg.height);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--resolution", arg + 13);
            }
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--resolution";
            }
        } else if (strcmp(arg, "--scene") == 0) {
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--scene");
            }
            i += 1;
            run_cfg.scene_name = argv[i];
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--scene";
            }
        } else if (strncmp(arg, "--scene=", 8) == 0) {
            run_cfg.scene_name = arg + 8;
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--scene";
            }
        } else if (strcmp(arg, "--pack") == 0) {
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--pack");
            }
            i += 1;
            run_cfg.pack_path = argv[i];
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--pack";
            }
        } else if (strncmp(arg, "--pack=", 7) == 0) {
            run_cfg.pack_path = arg + 7;
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--pack";
            }
        } else if (strcmp(arg, "--choice-index") == 0) {
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--choice-index");
            }
            i += 1;
            rc = parse_u32_range(argv[i], 0l, 255l, &rc_u32);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--choice-index", argv[i]);
            }
            run_cfg.choice_index = (vn_u8)(rc_u32 & 0xFFu);
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--choice-index";
            }
        } else if (strncmp(arg, "--choice-index=", 15) == 0) {
            rc = parse_u32_range(arg + 15, 0l, 255l, &rc_u32);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--choice-index", arg + 15);
            }
            run_cfg.choice_index = (vn_u8)(rc_u32 & 0xFFu);
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--choice-index";
            }
        } else if (strcmp(arg, "--choice-seq") == 0) {
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--choice-seq");
            }
            i += 1;
            rc = parse_choice_seq(argv[i], &choice_feed);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--choice-seq", argv[i]);
            }
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--choice-seq";
            }
        } else if (strncmp(arg, "--choice-seq=", 13) == 0) {
            rc = parse_choice_seq(arg + 13, &choice_feed);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--choice-seq", arg + 13);
            }
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--choice-seq";
            }
        } else if (strcmp(arg, "--frames") == 0) {
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--frames");
            }
            i += 1;
            rc = parse_u32_range(argv[i], 1l, 1000000l, &run_cfg.frames);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--frames", argv[i]);
            }
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--frames";
            }
        } else if (strncmp(arg, "--frames=", 9) == 0) {
            rc = parse_u32_range(arg + 9, 1l, 1000000l, &run_cfg.frames);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--frames", arg + 9);
            }
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--frames";
            }
        } else if (strcmp(arg, "--dt-ms") == 0) {
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--dt-ms");
            }
            i += 1;
            rc = parse_u32_range(argv[i], 0l, 1000l, &run_cfg.dt_ms);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--dt-ms", argv[i]);
            }
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--dt-ms";
            }
        } else if (strncmp(arg, "--dt-ms=", 8) == 0) {
            rc = parse_u32_range(arg + 8, 0l, 1000l, &run_cfg.dt_ms);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--dt-ms", arg + 8);
            }
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--dt-ms";
            }
        } else if (strcmp(arg, "--keyboard") == 0) {
            run_cfg.keyboard = 1u;
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--keyboard";
            }
        } else if (strcmp(arg, "--trace") == 0) {
            run_cfg.trace = 1u;
        } else if (strcmp(arg, "--hold-end") == 0) {
            run_cfg.hold_on_end = 1u;
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--hold-end";
            }
        } else if (strcmp(arg, "--perf-op-cache") == 0) {
            int enabled;
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--perf-op-cache");
            }
            i += 1;
            rc = parse_toggle_value(argv[i], &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-op-cache", argv[i]);
            }
            runtime_perf_flag_set(&run_cfg.perf_flags, VN_RUNTIME_PERF_OP_CACHE, enabled);
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--perf-op-cache";
            }
        } else if (strncmp(arg, "--perf-op-cache=", 16) == 0) {
            int enabled;
            rc = parse_toggle_value(arg + 16, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-op-cache", arg + 16);
            }
            runtime_perf_flag_set(&run_cfg.perf_flags, VN_RUNTIME_PERF_OP_CACHE, enabled);
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--perf-op-cache";
            }
        } else if (strcmp(arg, "--perf-dirty-tile") == 0) {
            int enabled;
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--perf-dirty-tile");
            }
            i += 1;
            rc = parse_toggle_value(argv[i], &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-dirty-tile", argv[i]);
            }
            runtime_perf_flag_set(&run_cfg.perf_flags, VN_RUNTIME_PERF_DIRTY_TILE, enabled);
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--perf-dirty-tile";
            }
        } else if (strncmp(arg, "--perf-dirty-tile=", 18) == 0) {
            int enabled;
            rc = parse_toggle_value(arg + 18, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-dirty-tile", arg + 18);
            }
            runtime_perf_flag_set(&run_cfg.perf_flags, VN_RUNTIME_PERF_DIRTY_TILE, enabled);
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--perf-dirty-tile";
            }
        } else if (strcmp(arg, "--perf-dynamic-resolution") == 0) {
            int enabled;
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--perf-dynamic-resolution");
            }
            i += 1;
            rc = parse_toggle_value(argv[i], &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-dynamic-resolution", argv[i]);
            }
            runtime_perf_flag_set(&run_cfg.perf_flags, VN_RUNTIME_PERF_DYNAMIC_RESOLUTION, enabled);
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--perf-dynamic-resolution";
            }
        } else if (strncmp(arg, "--perf-dynamic-resolution=", 26) == 0) {
            int enabled;
            rc = parse_toggle_value(arg + 26, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-dynamic-resolution", arg + 26);
            }
            runtime_perf_flag_set(&run_cfg.perf_flags, VN_RUNTIME_PERF_DYNAMIC_RESOLUTION, enabled);
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--perf-dynamic-resolution";
            }
        } else if (strcmp(arg, "--perf-frame-reuse") == 0) {
            int enabled;
            if ((i + 1) >= argc) {
                return runtime_cli_report_missing_value("--perf-frame-reuse");
            }
            i += 1;
            rc = parse_toggle_value(argv[i], &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-frame-reuse", argv[i]);
            }
            runtime_perf_flag_set(&run_cfg.perf_flags, VN_RUNTIME_PERF_FRAME_REUSE, enabled);
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--perf-frame-reuse";
            }
        } else if (strncmp(arg, "--perf-frame-reuse=", 19) == 0) {
            int enabled;
            rc = parse_toggle_value(arg + 19, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-frame-reuse", arg + 19);
            }
            runtime_perf_flag_set(&run_cfg.perf_flags, VN_RUNTIME_PERF_FRAME_REUSE, enabled);
            if (load_save_conflict_arg == (const char*)0) {
                load_save_conflict_arg = "--perf-frame-reuse";
            }
        } else if (strcmp(arg, "--quiet") == 0) {
            run_cfg.emit_logs = 0u;
        }
    }

    if (load_save_path != (const char*)0) {
        if (load_save_conflict_arg != (const char*)0) {
            return runtime_cli_report_invalid_combo("--load-save", load_save_conflict_arg);
        }
        rc = vn_runtime_session_load_from_file(load_save_path, &session);
        if (rc != VN_OK) {
            return runtime_cli_report_error("runtime.run.failed",
                                            rc,
                                            "vn_runtime_session_load_from_file failed",
                                            "save",
                                            load_save_path,
                                            1);
        }
        if (run_cfg.trace != 0u) {
            session->trace = 1u;
        }
        if (run_cfg.emit_logs == 0u) {
            session->emit_logs = 0u;
        }

        sleep_between_frames = VN_FALSE;
        if (session->keyboard.active != VN_FALSE && session->dt_ms > 0u) {
            sleep_between_frames = VN_TRUE;
        }
        rc = VN_OK;
        while (vn_runtime_session_is_done(session) == VN_FALSE) {
            rc = vn_runtime_session_step(session, (VNRunResult*)0);
            if (rc != VN_OK) {
                break;
            }
            if (sleep_between_frames != VN_FALSE &&
                vn_runtime_session_is_done(session) == VN_FALSE) {
                vn_platform_sleep_ms((unsigned int)session->dt_ms);
            }
        }
        if (rc == VN_OK && session->exit_code != 0) {
            rc = session->exit_code;
        }
        (void)vn_runtime_session_destroy(session);
        if (rc != 0) {
            return runtime_cli_report_error("runtime.run.failed",
                                            rc,
                                            "vn_runtime_session_load_from_file run failed",
                                            "save",
                                            load_save_path,
                                            1);
        }
        return 0;
    }

    run_cfg.choice_seq_count = choice_feed.count;
    if (choice_feed.count > 0u) {
        for (i = 0; i < (int)choice_feed.count; ++i) {
            run_cfg.choice_seq[i] = choice_feed.items[(vn_u32)i];
        }
    }

    rc = parse_scene_id(run_cfg.scene_name, &scene_id);
    if (rc != VN_OK) {
        return runtime_cli_report_error("runtime.cli.scene.invalid",
                                        VN_E_INVALID_ARG,
                                        "invalid scene",
                                        "scene",
                                        run_cfg.scene_name,
                                        2);
    }

    rc = vn_runtime_run(&run_cfg, (VNRunResult*)0);
    if (rc != 0) {
        return runtime_cli_report_error("runtime.run.failed",
                                        rc,
                                        "vn_runtime_run failed",
                                        "scene",
                                        run_cfg.scene_name,
                                        1);
    }
    return 0;
}

int vn_runtime_run(const VNRunConfig* run_cfg, VNRunResult* out_result) {
    VNRuntimeSession* session;
    VNRunResult step_result;
    int rc;
    int step_rc;
    int sleep_between_frames;

    runtime_result_reset();
    rc = vn_runtime_session_create(run_cfg, &session);
    if (rc != VN_OK) {
        if (out_result != (VNRunResult*)0) {
            *out_result = g_last_run_result;
        }
        return rc;
    }

    step_result = g_last_run_result;
    rc = VN_OK;
    sleep_between_frames = VN_FALSE;
    if (run_cfg != (const VNRunConfig*)0 &&
        run_cfg->keyboard != 0u &&
        run_cfg->dt_ms > 0u) {
        sleep_between_frames = VN_TRUE;
    }
    while (vn_runtime_session_is_done(session) == VN_FALSE) {
        step_rc = vn_runtime_session_step(session, &step_result);
        if (step_rc != VN_OK) {
            rc = step_rc;
            break;
        }
        if (sleep_between_frames != VN_FALSE &&
            vn_runtime_session_is_done(session) == VN_FALSE) {
            vn_platform_sleep_ms((unsigned int)run_cfg->dt_ms);
        }
    }

    if (rc == VN_OK && session->exit_code != 0) {
        rc = session->exit_code;
    }
    if (out_result != (VNRunResult*)0) {
        *out_result = g_last_run_result;
    }
    (void)vn_runtime_session_destroy(session);
    return rc;
}
