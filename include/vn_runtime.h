#ifndef VN_RUNTIME_H
#define VN_RUNTIME_H

#include "vn_types.h"

#define VN_RUNTIME_MAX_CHOICE_SEQ 64u

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
    vn_u8 choice_index;
    vn_u8 choice_seq[VN_RUNTIME_MAX_CHOICE_SEQ];
    vn_u32 choice_seq_count;
} VNRunConfig;

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
} VNRunResult;

void vn_run_config_init(VNRunConfig* cfg);
int vn_runtime_run(const VNRunConfig* cfg, VNRunResult* out_result);
int vn_runtime_run_cli(int argc, char** argv);

#endif
