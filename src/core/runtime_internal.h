#ifndef VN_RUNTIME_INTERNAL_H
#define VN_RUNTIME_INTERNAL_H

#if !defined(_WIN32)
#include <termios.h>
#endif

#include "vn_renderer.h"
#include "vn_frontend.h"
#include "../frontend/dirty_tiles.h"
#include "vn_pack.h"
#include "vn_vm.h"
#include "vn_runtime.h"
#include "vn_error.h"
#include "vn_save.h"
#include "dynamic_resolution.h"

#define VN_MAX_CHOICE_SEQ 64u
#define VN_OP_CACHE_CAP 320u

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

vn_u32 parse_backend_flag(const char* value);
const char* scene_name_from_id(vn_u32 scene_id);
void state_apply_fade(VNRuntimeState* state, const FadePlayer* fade);
void runtime_render_cache_invalidate(VNRuntimeSession* session);
void runtime_dirty_planner_reconfigure(VNRuntimeSession* session,
                                       vn_u16 width,
                                       vn_u16 height);
int runtime_renderer_reconfigure(VNRuntimeSession* session,
                                 vn_u16 width,
                                 vn_u16 height);
vn_u32 runtime_supported_perf_flags(void);
void runtime_result_publish(const VNRuntimeSession* session);

#endif
