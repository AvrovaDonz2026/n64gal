#ifndef VN_RUNTIME_H
#define VN_RUNTIME_H

#include "vn_types.h"

#define VN_RUNTIME_API_VERSION "v1-draft"
#define VN_RUNTIME_API_STABILITY "public v1-draft (pre-1.0)"
#define VN_RUNTIME_MAX_CHOICE_SEQ 64u
#define VN_RUNTIME_SNAPSHOT_PATH_MAX 512u
#define VN_RUNTIME_SNAPSHOT_BACKEND_MAX 16u
#define VN_RUNTIME_SNAPSHOT_CALL_STACK_MAX 16u
#define VN_RUNTIME_PERF_OP_CACHE    (1u << 0)
#define VN_RUNTIME_PERF_FRAME_REUSE (1u << 1)
#define VN_RUNTIME_PERF_DIRTY_TILE         (1u << 2)
#define VN_RUNTIME_PERF_DYNAMIC_RESOLUTION (1u << 3)
#define VN_RUNTIME_PERF_DEFAULT_FLAGS (VN_RUNTIME_PERF_OP_CACHE | VN_RUNTIME_PERF_FRAME_REUSE)

typedef struct {
    const char* pack_path;
    const char* scene_name;
    const char* backend_name;
    vn_u16 width;
    vn_u16 height;
    vn_u32 frames;
    vn_u32 dt_ms;
    vn_u32 trace;
    vn_u32 keyboard;
    vn_u32 emit_logs;
    vn_u32 hold_on_end;
    vn_u32 perf_flags;
    vn_u8 choice_index;
    vn_u8 choice_seq[VN_RUNTIME_MAX_CHOICE_SEQ];
    vn_u32 choice_seq_count;
} VNRunConfig;

#define VN_INPUT_KIND_CHOICE       1u
#define VN_INPUT_KIND_KEY          2u
#define VN_INPUT_KIND_TRACE_TOGGLE 3u
#define VN_INPUT_KIND_QUIT         4u

typedef struct {
    vn_u32 kind;
    vn_u32 value0;
    vn_u32 value1;
} VNInputEvent;

typedef struct {
    const char* runtime_api_version;
    const char* runtime_api_stability;
    const char* preview_protocol_version;
    vn_u32 vnpak_read_min_version;
    vn_u32 vnpak_read_max_version;
    vn_u32 vnpak_write_default_version;
    vn_u32 vnsave_latest_version;
    const char* vnsave_api_stability;
    const char* host_os;
    const char* host_arch;
    const char* host_compiler;
} VNRuntimeBuildInfo;

typedef struct {
    char pack_path[VN_RUNTIME_SNAPSHOT_PATH_MAX];
    char backend_name[VN_RUNTIME_SNAPSHOT_BACKEND_MAX];
    vn_u32 scene_id;
    vn_u16 base_width;
    vn_u16 base_height;
    vn_u32 frames_limit;
    vn_u32 frames_executed;
    vn_u32 dt_ms;
    vn_u32 trace;
    vn_u32 emit_logs;
    vn_u32 hold_on_end;
    vn_u32 perf_flags;
    vn_u32 keyboard_enabled;
    vn_u8 default_choice_index;
    vn_u8 choice_feed_items[VN_RUNTIME_MAX_CHOICE_SEQ];
    vn_u32 choice_feed_count;
    vn_u32 choice_feed_cursor;
    vn_u32 dynamic_resolution_tier;
    vn_u32 dynamic_resolution_switches;
    vn_u32 vm_pc_offset;
    vn_u32 vm_wait_ms;
    vn_u16 vm_call_stack[VN_RUNTIME_SNAPSHOT_CALL_STACK_MAX];
    vn_u8 vm_call_sp;
    vn_u16 vm_current_text_id;
    vn_u16 vm_current_text_speed_ms;
    vn_u8 vm_fade_layer_mask;
    vn_u8 vm_fade_target_alpha;
    vn_u16 vm_fade_duration_ms;
    vn_u16 vm_current_bgm_id;
    vn_u8 vm_current_bgm_loop;
    vn_u16 vm_pending_se_id;
    vn_u8 vm_pending_se_flag;
    vn_u8 vm_last_choice_count;
    vn_u8 vm_last_choice_selected_index;
    vn_u8 vm_external_choice_valid;
    vn_u8 vm_external_choice_index;
    vn_u16 vm_last_choice_text_id;
    vn_u32 vm_choice_serial;
    vn_u32 vm_fade_serial;
    vn_u32 vm_flags;
    vn_u32 fade_seen_serial;
    vn_u8 fade_active;
    vn_u8 fade_layer_mask;
    vn_u8 fade_alpha_current;
    vn_u8 fade_alpha_start;
    vn_u8 fade_alpha_target;
    vn_u16 fade_duration_ms;
    vn_u32 fade_elapsed_ms;
} VNRuntimeSessionSnapshot;

typedef struct {
    vn_u32 frames_executed;
    vn_u32 text_id;
    vn_u32 vm_waiting;
    vn_u32 vm_ended;
    vn_u32 vm_error;
    vn_u32 fade_alpha;
    vn_u32 fade_remain_ms;
    vn_u32 bgm_id;
    vn_u32 se_id;
    vn_u32 choice_count;
    vn_u32 choice_selected_index;
    vn_u32 choice_text_id;
    vn_u32 op_count;
    const char* backend_name;
    vn_u32 perf_flags_effective;
    vn_u32 frame_reuse_hits;
    vn_u32 frame_reuse_misses;
    vn_u32 op_cache_hits;
    vn_u32 op_cache_misses;
    vn_u32 dirty_tile_count;
    vn_u32 dirty_rect_count;
    vn_u32 dirty_full_redraw;
    vn_u32 dirty_tile_frames;
    vn_u32 dirty_tile_total;
    vn_u32 dirty_rect_total;
    vn_u32 dirty_full_redraws;
    vn_u16 render_width;
    vn_u16 render_height;
    vn_u32 dynamic_resolution_tier;
    vn_u32 dynamic_resolution_switches;
} VNRunResult;

typedef struct VNRuntimeSession VNRuntimeSession;

void vn_run_config_init(VNRunConfig* cfg);
void vn_runtime_query_build_info(VNRuntimeBuildInfo* out_info);
int vn_runtime_session_capture_snapshot(const VNRuntimeSession* session,
                                        VNRuntimeSessionSnapshot* out_snapshot);
int vn_runtime_session_create_from_snapshot(const VNRuntimeSessionSnapshot* snapshot,
                                            VNRuntimeSession** out_session);
int vn_runtime_session_save_to_file(const VNRuntimeSession* session,
                                    const char* path,
                                    vn_u32 slot_id,
                                    vn_u32 timestamp_s);
int vn_runtime_session_load_from_file(const char* path,
                                      VNRuntimeSession** out_session);
int vn_runtime_run(const VNRunConfig* cfg, VNRunResult* out_result);
int vn_runtime_run_cli(int argc, char** argv);
int vn_runtime_session_create(const VNRunConfig* cfg, VNRuntimeSession** out_session);
int vn_runtime_session_step(VNRuntimeSession* session, VNRunResult* out_result);
int vn_runtime_session_is_done(const VNRuntimeSession* session);
int vn_runtime_session_set_choice(VNRuntimeSession* session, vn_u8 choice_index);
int vn_runtime_session_inject_input(VNRuntimeSession* session, const VNInputEvent* event);
int vn_runtime_session_destroy(VNRuntimeSession* session);

#endif
