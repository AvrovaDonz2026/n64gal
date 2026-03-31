#ifndef VN_PREVIEW_INTERNAL_H
#define VN_PREVIEW_INTERNAL_H

#include <stdio.h>

#include "vn_error.h"
#include "vn_preview.h"
#include "vn_runtime.h"

#define VN_PREVIEW_PATH_MAX 512u
#define VN_PREVIEW_TEXT_MAX 160u
#define VN_PREVIEW_NAME_MAX 32u
#define VN_PREVIEW_LINE_MAX 1024u
#define VN_PREVIEW_MAX_COMMANDS 128u
#define VN_PREVIEW_MAX_EVENTS 256u

#define VN_PREVIEW_EVENT_COMMAND 1
#define VN_PREVIEW_EVENT_FRAME 2
#define VN_PREVIEW_EVENT_RELOAD 3
#define VN_PREVIEW_EVENT_ERROR 4

#define VN_PREVIEW_CMD_RUN_TO_END 1
#define VN_PREVIEW_CMD_STEP_FRAME 2
#define VN_PREVIEW_CMD_RELOAD_SCENE 3
#define VN_PREVIEW_CMD_SET_CHOICE 4
#define VN_PREVIEW_CMD_INJECT_CHOICE 5
#define VN_PREVIEW_CMD_INJECT_KEY 6
#define VN_PREVIEW_CMD_INJECT_TRACE_TOGGLE 7
#define VN_PREVIEW_CMD_INJECT_QUIT 8
#define VN_PREVIEW_EXIT_HELP 99

typedef struct {
    int kind;
    vn_u32 value;
} VNPreviewCommand;

typedef struct {
    int kind;
    char name[VN_PREVIEW_NAME_MAX];
    const char* trace_id;
    vn_u32 value;
    vn_u32 has_result;
    double host_step_ms;
    VNRunResult result;
} VNPreviewEvent;

typedef struct {
    VNRunConfig cfg;
    vn_u32 trace_enabled;
    char project_dir[VN_PREVIEW_PATH_MAX];
    char request_path[VN_PREVIEW_PATH_MAX];
    char request_dir[VN_PREVIEW_PATH_MAX];
    char response_path[VN_PREVIEW_PATH_MAX];
    char pack_path[VN_PREVIEW_PATH_MAX];
    char resolved_pack_path[VN_PREVIEW_PATH_MAX];
    char scene_name[VN_PREVIEW_NAME_MAX];
    char backend_name[VN_PREVIEW_NAME_MAX];
    VNPreviewCommand commands[VN_PREVIEW_MAX_COMMANDS];
    vn_u32 command_count;
} VNPreviewRequest;

typedef struct {
    vn_u32 valid;
    double host_step_ms;
    VNRunResult result;
} VNPreviewFrameSample;

typedef struct {
    int status_code;
    int error_code;
    const char* error_name;
    const char* trace_id;
    char error_message[VN_PREVIEW_TEXT_MAX];
    vn_u32 command_count;
    vn_u32 reload_count;
    vn_u32 events_truncated;
    vn_u32 frame_samples;
    vn_u32 has_final_state;
    vn_u32 session_done;
    double total_step_ms;
    double max_step_ms;
    VNPreviewFrameSample first_frame;
    VNPreviewFrameSample last_frame;
    VNRunResult final_state;
    VNPreviewEvent events[VN_PREVIEW_MAX_EVENTS];
    vn_u32 event_count;
} VNPreviewReport;

void preview_report_init(VNPreviewReport* report);
int preview_parse_cli(VNPreviewRequest* req,
                      VNPreviewReport* report,
                      int argc,
                      char** argv);
int preview_write_response(const VNPreviewRequest* req,
                           const VNPreviewReport* report);
void preview_error(VNPreviewReport* report,
                   int error_code,
                   const char* message,
                   int status_code);
void preview_report_add_event(VNPreviewReport* report,
                              int kind,
                              const char* name,
                              vn_u32 value,
                              double host_step_ms,
                              const VNRunResult* result);
void preview_report_add_frame(VNPreviewReport* report,
                              const VNRunResult* result,
                              double host_step_ms,
                              vn_u32 trace_enabled);
int preview_step_frames(VNRuntimeSession* session,
                        vn_u32 count,
                        VNPreviewReport* report,
                        vn_u32 trace_enabled);
void preview_json_write_string(FILE* fp, const char* text);
void preview_json_write_result(FILE* fp, const VNRunResult* result);
void preview_json_write_frame(FILE* fp,
                              const VNPreviewFrameSample* frame);
double preview_now_ms(void);
const char* preview_trace_id_for_error(int status_code, int error_code);
const char* preview_trace_id_for_event(int kind);

#endif
