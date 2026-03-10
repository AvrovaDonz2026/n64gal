#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vn_error.h"
#include "vn_preview.h"
#include "vn_runtime.h"
#include "../core/platform.h"

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

static void preview_request_init(VNPreviewRequest* req);
static void preview_report_init(VNPreviewReport* report);
static int preview_parse_cli(VNPreviewRequest* req,
                             VNPreviewReport* report,
                             int argc,
                             char** argv);
static int preview_load_request_file(VNPreviewRequest* req,
                                     VNPreviewReport* report,
                                     const char* path);
static int preview_run_request(const VNPreviewRequest* req,
                               VNPreviewReport* report);
static int preview_write_response(const VNPreviewRequest* req,
                                  const VNPreviewReport* report);
static void preview_print_usage(void);
static void preview_str_copy(char* dst, size_t dst_size, const char* src);
static char* preview_trim(char* text);
static int preview_parse_u32_range(const char* text,
                                   unsigned long min_value,
                                   unsigned long max_value,
                                   vn_u32* out_value);
static int preview_parse_bool(const char* text, vn_u32* out_value);
static int preview_parse_resolution(const char* text,
                                    vn_u16* out_width,
                                    vn_u16* out_height);
static int preview_parse_choice_seq(const char* text,
                                    vn_u8* out_items,
                                    vn_u32* out_count);
static int preview_parse_command(const char* text,
                                 VNPreviewCommand* out_command);
static int preview_add_command(VNPreviewRequest* req,
                               const VNPreviewCommand* command,
                               VNPreviewReport* report);
static void preview_error(VNPreviewReport* report,
                          int error_code,
                          const char* message,
                          int status_code);
static const char* preview_error_name(int error_code);
static void preview_resolve_pack_path(VNPreviewRequest* req);
static void preview_report_add_event(VNPreviewReport* report,
                                     int kind,
                                     const char* name,
                                     vn_u32 value,
                                     double host_step_ms,
                                     const VNRunResult* result);
static void preview_report_add_frame(VNPreviewReport* report,
                                     const VNRunResult* result,
                                     double host_step_ms,
                                     vn_u32 trace_enabled);
static int preview_step_frames(VNRuntimeSession* session,
                               vn_u32 count,
                               VNPreviewReport* report,
                               vn_u32 trace_enabled);
static void preview_json_write_string(FILE* fp, const char* text);
static void preview_json_write_result(FILE* fp, const VNRunResult* result);
static void preview_json_write_frame(FILE* fp,
                                     const VNPreviewFrameSample* frame);
static double preview_now_ms(void);

int vn_preview_run_cli(int argc, char** argv) {
    VNPreviewRequest req;
    VNPreviewReport report;
    int rc;

    preview_request_init(&req);
    preview_report_init(&report);

    rc = preview_parse_cli(&req, &report, argc, argv);
    if (rc == VN_PREVIEW_EXIT_HELP) {
        return 0;
    }
    if (rc == 0) {
        rc = preview_run_request(&req, &report);
    }

    if (rc != 0 && report.error_name == (const char*)0) {
        preview_error(&report, rc, "preview execution failed", 1);
    }

    if (report.error_name == (const char*)0) {
        report.error_name = preview_error_name(report.error_code);
    }

    if (preview_write_response(&req, &report) != 0 && rc == 0) {
        rc = 1;
    }
    return rc;
}

static void preview_request_init(VNPreviewRequest* req) {
    if (req == (VNPreviewRequest*)0) {
        return;
    }
    (void)memset(req, 0, sizeof(VNPreviewRequest));
    vn_run_config_init(&req->cfg);
    req->cfg.trace = 0u;
    req->cfg.emit_logs = 0u;
    req->trace_enabled = 0u;
}

static void preview_report_init(VNPreviewReport* report) {
    if (report == (VNPreviewReport*)0) {
        return;
    }
    (void)memset(report, 0, sizeof(VNPreviewReport));
    report->status_code = 0;
    report->error_code = VN_OK;
    report->error_name = preview_error_name(VN_OK);
}

static int preview_parse_cli(VNPreviewRequest* req,
                             VNPreviewReport* report,
                             int argc,
                             char** argv) {
    int i;
    const char* arg;
    VNPreviewCommand command;
    vn_u32 parsed_u32;
    int rc;

    if (req == (VNPreviewRequest*)0 || report == (VNPreviewReport*)0 || argv == (char**)0) {
        preview_error(report, VN_E_INVALID_ARG, "null preview cli arguments", 2);
        return 2;
    }

    for (i = 1; i < argc; ++i) {
        arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            preview_print_usage();
            report->status_code = 0;
            report->error_code = VN_OK;
            report->error_name = preview_error_name(VN_OK);
            return VN_PREVIEW_EXIT_HELP;
        }
    }

    for (i = 1; i < argc; ++i) {
        arg = argv[i];
        if (strcmp(arg, "--request") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --request", 2);
                return 2;
            }
            i += 1;
            preview_str_copy(req->request_path, sizeof(req->request_path), argv[i]);
        } else if (strncmp(arg, "--request=", 10) == 0) {
            preview_str_copy(req->request_path, sizeof(req->request_path), arg + 10);
        } else if (strcmp(arg, "--response") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --response", 2);
                return 2;
            }
            i += 1;
            preview_str_copy(req->response_path, sizeof(req->response_path), argv[i]);
        } else if (strncmp(arg, "--response=", 11) == 0) {
            preview_str_copy(req->response_path, sizeof(req->response_path), arg + 11);
        }
    }

    if (req->request_path[0] != '\0') {
        rc = preview_load_request_file(req, report, req->request_path);
        if (rc != 0) {
            if (report->status_code == 0) {
                preview_error(report, rc, "failed to parse request file", 2);
            }
            return 2;
        }
    }

    for (i = 1; i < argc; ++i) {
        arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            preview_print_usage();
            report->status_code = 0;
            report->error_code = VN_OK;
            report->error_name = preview_error_name(VN_OK);
            return VN_PREVIEW_EXIT_HELP;
        } else if (strcmp(arg, "--request") == 0 || strcmp(arg, "--response") == 0) {
            i += 1;
            if (i >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for preview flag", 2);
                return 2;
            }
        } else if (strncmp(arg, "--request=", 10) == 0 || strncmp(arg, "--response=", 11) == 0) {
        } else if (strcmp(arg, "--project-dir") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --project-dir", 2);
                return 2;
            }
            i += 1;
            preview_str_copy(req->project_dir, sizeof(req->project_dir), argv[i]);
        } else if (strncmp(arg, "--project-dir=", 14) == 0) {
            preview_str_copy(req->project_dir, sizeof(req->project_dir), arg + 14);
        } else if (strcmp(arg, "--pack") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --pack", 2);
                return 2;
            }
            i += 1;
            preview_str_copy(req->pack_path, sizeof(req->pack_path), argv[i]);
        } else if (strncmp(arg, "--pack=", 7) == 0) {
            preview_str_copy(req->pack_path, sizeof(req->pack_path), arg + 7);
        } else if (strcmp(arg, "--scene") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --scene", 2);
                return 2;
            }
            i += 1;
            preview_str_copy(req->scene_name, sizeof(req->scene_name), argv[i]);
        } else if (strncmp(arg, "--scene=", 8) == 0) {
            preview_str_copy(req->scene_name, sizeof(req->scene_name), arg + 8);
        } else if (strcmp(arg, "--backend") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --backend", 2);
                return 2;
            }
            i += 1;
            preview_str_copy(req->backend_name, sizeof(req->backend_name), argv[i]);
        } else if (strncmp(arg, "--backend=", 10) == 0) {
            preview_str_copy(req->backend_name, sizeof(req->backend_name), arg + 10);
        } else if (strcmp(arg, "--resolution") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --resolution", 2);
                return 2;
            }
            i += 1;
            rc = preview_parse_resolution(argv[i], &req->cfg.width, &req->cfg.height);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --resolution", 2);
                return 2;
            }
        } else if (strncmp(arg, "--resolution=", 13) == 0) {
            rc = preview_parse_resolution(arg + 13, &req->cfg.width, &req->cfg.height);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --resolution", 2);
                return 2;
            }
        } else if (strcmp(arg, "--width") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --width", 2);
                return 2;
            }
            i += 1;
            rc = preview_parse_u32_range(argv[i], 1ul, 65535ul, &parsed_u32);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --width", 2);
                return 2;
            }
            req->cfg.width = (vn_u16)parsed_u32;
        } else if (strncmp(arg, "--width=", 8) == 0) {
            rc = preview_parse_u32_range(arg + 8, 1ul, 65535ul, &parsed_u32);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --width", 2);
                return 2;
            }
            req->cfg.width = (vn_u16)parsed_u32;
        } else if (strcmp(arg, "--height") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --height", 2);
                return 2;
            }
            i += 1;
            rc = preview_parse_u32_range(argv[i], 1ul, 65535ul, &parsed_u32);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --height", 2);
                return 2;
            }
            req->cfg.height = (vn_u16)parsed_u32;
        } else if (strncmp(arg, "--height=", 9) == 0) {
            rc = preview_parse_u32_range(arg + 9, 1ul, 65535ul, &parsed_u32);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --height", 2);
                return 2;
            }
            req->cfg.height = (vn_u16)parsed_u32;
        } else if (strcmp(arg, "--frames") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --frames", 2);
                return 2;
            }
            i += 1;
            rc = preview_parse_u32_range(argv[i], 1ul, 1000000ul, &req->cfg.frames);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --frames", 2);
                return 2;
            }
        } else if (strncmp(arg, "--frames=", 9) == 0) {
            rc = preview_parse_u32_range(arg + 9, 1ul, 1000000ul, &req->cfg.frames);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --frames", 2);
                return 2;
            }
        } else if (strcmp(arg, "--dt-ms") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --dt-ms", 2);
                return 2;
            }
            i += 1;
            rc = preview_parse_u32_range(argv[i], 0ul, 1000ul, &req->cfg.dt_ms);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --dt-ms", 2);
                return 2;
            }
        } else if (strncmp(arg, "--dt-ms=", 8) == 0) {
            rc = preview_parse_u32_range(arg + 8, 0ul, 1000ul, &req->cfg.dt_ms);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --dt-ms", 2);
                return 2;
            }
        } else if (strcmp(arg, "--choice-index") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --choice-index", 2);
                return 2;
            }
            i += 1;
            rc = preview_parse_u32_range(argv[i], 0ul, 255ul, &parsed_u32);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --choice-index", 2);
                return 2;
            }
            req->cfg.choice_index = (vn_u8)(parsed_u32 & 0xFFu);
        } else if (strncmp(arg, "--choice-index=", 15) == 0) {
            rc = preview_parse_u32_range(arg + 15, 0ul, 255ul, &parsed_u32);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --choice-index", 2);
                return 2;
            }
            req->cfg.choice_index = (vn_u8)(parsed_u32 & 0xFFu);
        } else if (strcmp(arg, "--choice-seq") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --choice-seq", 2);
                return 2;
            }
            i += 1;
            rc = preview_parse_choice_seq(argv[i], req->cfg.choice_seq, &req->cfg.choice_seq_count);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --choice-seq", 2);
                return 2;
            }
        } else if (strncmp(arg, "--choice-seq=", 13) == 0) {
            rc = preview_parse_choice_seq(arg + 13, req->cfg.choice_seq, &req->cfg.choice_seq_count);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --choice-seq", 2);
                return 2;
            }
        } else if (strcmp(arg, "--trace") == 0) {
            req->trace_enabled = 1u;
        } else if (strcmp(arg, "--hold-end") == 0) {
            req->cfg.hold_on_end = 1u;
        } else if (strcmp(arg, "--command") == 0) {
            if ((i + 1) >= argc) {
                preview_error(report, VN_E_INVALID_ARG, "missing value for --command", 2);
                return 2;
            }
            i += 1;
            rc = preview_parse_command(argv[i], &command);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --command", 2);
                return 2;
            }
            rc = preview_add_command(req, &command, report);
            if (rc != VN_OK) {
                return 2;
            }
        } else if (strncmp(arg, "--command=", 10) == 0) {
            rc = preview_parse_command(arg + 10, &command);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid --command", 2);
                return 2;
            }
            rc = preview_add_command(req, &command, report);
            if (rc != VN_OK) {
                return 2;
            }
        } else {
            preview_error(report, VN_E_INVALID_ARG, "unknown preview argument", 2);
            return 2;
        }
    }

    if (req->scene_name[0] != '\0') {
        req->cfg.scene_name = req->scene_name;
    }
    if (req->backend_name[0] != '\0') {
        req->cfg.backend_name = req->backend_name;
    }
    preview_resolve_pack_path(req);
    if (req->resolved_pack_path[0] != '\0') {
        req->cfg.pack_path = req->resolved_pack_path;
    }
    report->command_count = req->command_count;
    return 0;
}

static int preview_load_request_file(VNPreviewRequest* req,
                                     VNPreviewReport* report,
                                     const char* path) {
    FILE* fp;
    char line[VN_PREVIEW_LINE_MAX];
    char* key;
    char* value;
    char* eq;
    int rc;
    vn_u32 parsed_u32;
    VNPreviewCommand command;

    if (req == (VNPreviewRequest*)0 || report == (VNPreviewReport*)0 || path == (const char*)0) {
        return VN_E_INVALID_ARG;
    }

    vn_platform_path_dirname(path, req->request_dir, sizeof(req->request_dir));
    fp = fopen(path, "r");
    if (fp == (FILE*)0) {
        preview_error(report, VN_E_IO, "failed to open request file", 2);
        return VN_E_IO;
    }

    rc = VN_OK;
    while (fgets(line, (int)sizeof(line), fp) != (char*)0) {
        key = preview_trim(line);
        if (key[0] == '\0' || key[0] == '#' || key[0] == ';') {
            continue;
        }
        eq = strchr(key, '=');
        if (eq == (char*)0) {
            preview_error(report, VN_E_FORMAT, "request line must use key=value", 2);
            rc = VN_E_FORMAT;
            break;
        }
        *eq = '\0';
        value = preview_trim(eq + 1);
        key = preview_trim(key);

        if (strcmp(key, "preview_protocol") == 0) {
            if (strcmp(value, "v1") != 0) {
                preview_error(report, VN_E_UNSUPPORTED, "unsupported preview protocol version", 2);
                rc = VN_E_UNSUPPORTED;
                break;
            }
        } else if (strcmp(key, "project_dir") == 0) {
            preview_str_copy(req->project_dir, sizeof(req->project_dir), value);
        } else if (strcmp(key, "pack") == 0 || strcmp(key, "pack_path") == 0) {
            preview_str_copy(req->pack_path, sizeof(req->pack_path), value);
        } else if (strcmp(key, "scene") == 0 || strcmp(key, "scene_name") == 0) {
            preview_str_copy(req->scene_name, sizeof(req->scene_name), value);
        } else if (strcmp(key, "backend") == 0) {
            preview_str_copy(req->backend_name, sizeof(req->backend_name), value);
        } else if (strcmp(key, "resolution") == 0) {
            rc = preview_parse_resolution(value, &req->cfg.width, &req->cfg.height);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid request resolution", 2);
                break;
            }
        } else if (strcmp(key, "width") == 0) {
            rc = preview_parse_u32_range(value, 1ul, 65535ul, &parsed_u32);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid request width", 2);
                break;
            }
            req->cfg.width = (vn_u16)parsed_u32;
        } else if (strcmp(key, "height") == 0) {
            rc = preview_parse_u32_range(value, 1ul, 65535ul, &parsed_u32);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid request height", 2);
                break;
            }
            req->cfg.height = (vn_u16)parsed_u32;
        } else if (strcmp(key, "frames") == 0) {
            rc = preview_parse_u32_range(value, 1ul, 1000000ul, &req->cfg.frames);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid request frames", 2);
                break;
            }
        } else if (strcmp(key, "dt_ms") == 0 || strcmp(key, "dt-ms") == 0) {
            rc = preview_parse_u32_range(value, 0ul, 1000ul, &req->cfg.dt_ms);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid request dt_ms", 2);
                break;
            }
        } else if (strcmp(key, "trace") == 0) {
            rc = preview_parse_bool(value, &req->trace_enabled);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid request trace", 2);
                break;
            }
        } else if (strcmp(key, "hold_on_end") == 0 || strcmp(key, "hold-end") == 0) {
            rc = preview_parse_bool(value, &req->cfg.hold_on_end);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid request hold_on_end", 2);
                break;
            }
        } else if (strcmp(key, "choice_index") == 0 || strcmp(key, "choice-index") == 0) {
            rc = preview_parse_u32_range(value, 0ul, 255ul, &parsed_u32);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid request choice_index", 2);
                break;
            }
            req->cfg.choice_index = (vn_u8)(parsed_u32 & 0xFFu);
        } else if (strcmp(key, "choice_seq") == 0 || strcmp(key, "choice-seq") == 0) {
            rc = preview_parse_choice_seq(value, req->cfg.choice_seq, &req->cfg.choice_seq_count);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid request choice_seq", 2);
                break;
            }
        } else if (strcmp(key, "response") == 0 || strcmp(key, "response_path") == 0) {
            preview_str_copy(req->response_path, sizeof(req->response_path), value);
        } else if (strcmp(key, "command") == 0) {
            rc = preview_parse_command(value, &command);
            if (rc != VN_OK) {
                preview_error(report, rc, "invalid request command", 2);
                break;
            }
            rc = preview_add_command(req, &command, report);
            if (rc != VN_OK) {
                break;
            }
        } else {
            preview_error(report, VN_E_INVALID_ARG, "unknown request key", 2);
            rc = VN_E_INVALID_ARG;
            break;
        }
    }

    (void)fclose(fp);
    if (rc == VN_OK) {
        if (req->scene_name[0] != '\0') {
            req->cfg.scene_name = req->scene_name;
        }
        if (req->backend_name[0] != '\0') {
            req->cfg.backend_name = req->backend_name;
        }
        preview_resolve_pack_path(req);
        if (req->resolved_pack_path[0] != '\0') {
            req->cfg.pack_path = req->resolved_pack_path;
        }
        report->command_count = req->command_count;
    }
    return rc;
}

static int preview_run_request(const VNPreviewRequest* req,
                               VNPreviewReport* report) {
    VNRuntimeSession* session;
    VNPreviewCommand command;
    vn_u32 i;
    int rc;

    if (req == (const VNPreviewRequest*)0 || report == (VNPreviewReport*)0) {
        preview_error(report, VN_E_INVALID_ARG, "null preview request", 1);
        return 1;
    }

    session = (VNRuntimeSession*)0;
    rc = vn_runtime_session_create(&req->cfg, &session);
    if (rc != VN_OK) {
        preview_error(report, rc, "vn_runtime_session_create failed", 1);
        return 1;
    }

    if (req->command_count == 0u) {
        preview_report_add_event(report, VN_PREVIEW_EVENT_COMMAND, "run_to_end", 0u, 0.0, (const VNRunResult*)0);
        rc = preview_step_frames(session, req->cfg.frames, report, req->trace_enabled);
        if (rc != VN_OK) {
            preview_error(report, rc, "preview run_to_end failed", 1);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
    } else {
        for (i = 0u; i < req->command_count; ++i) {
            command = req->commands[i];
            if (command.kind == VN_PREVIEW_CMD_RUN_TO_END) {
                preview_report_add_event(report, VN_PREVIEW_EVENT_COMMAND, "run_to_end", 0u, 0.0, (const VNRunResult*)0);
                rc = preview_step_frames(session, req->cfg.frames, report, req->trace_enabled);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview run_to_end failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_STEP_FRAME) {
                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "step_frame",
                                         command.value,
                                         0.0,
                                         (const VNRunResult*)0);
                rc = preview_step_frames(session,
                                         (command.value == 0u) ? 1u : command.value,
                                         report,
                                         req->trace_enabled);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview step_frame failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_RELOAD_SCENE) {
                preview_report_add_event(report, VN_PREVIEW_EVENT_RELOAD, "reload_scene", 0u, 0.0, (const VNRunResult*)0);
                report->reload_count += 1u;
                report->session_done = 0u;
                report->has_final_state = 0u;
                (void)memset(&report->final_state, 0, sizeof(VNRunResult));
                (void)vn_runtime_session_destroy(session);
                session = (VNRuntimeSession*)0;
                rc = vn_runtime_session_create(&req->cfg, &session);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview reload_scene failed", 1);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_SET_CHOICE) {
                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "set_choice",
                                         command.value,
                                         0.0,
                                         (const VNRunResult*)0);
                rc = vn_runtime_session_set_choice(session, (vn_u8)(command.value & 0xFFu));
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview set_choice failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_INJECT_CHOICE) {
                VNInputEvent input_event;

                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "inject_input",
                                         command.value,
                                         0.0,
                                         (const VNRunResult*)0);
                input_event.kind = VN_INPUT_KIND_CHOICE;
                input_event.value0 = command.value;
                input_event.value1 = 0u;
                rc = vn_runtime_session_inject_input(session, &input_event);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview inject_input failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_INJECT_KEY) {
                VNInputEvent input_event;

                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "inject_input.key",
                                         command.value,
                                         0.0,
                                         (const VNRunResult*)0);
                input_event.kind = VN_INPUT_KIND_KEY;
                input_event.value0 = command.value;
                input_event.value1 = 0u;
                rc = vn_runtime_session_inject_input(session, &input_event);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview inject_input key failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_INJECT_TRACE_TOGGLE) {
                VNInputEvent input_event;

                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "inject_input.trace_toggle",
                                         0u,
                                         0.0,
                                         (const VNRunResult*)0);
                input_event.kind = VN_INPUT_KIND_TRACE_TOGGLE;
                input_event.value0 = 0u;
                input_event.value1 = 0u;
                rc = vn_runtime_session_inject_input(session, &input_event);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview inject_input trace_toggle failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_INJECT_QUIT) {
                VNInputEvent input_event;

                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "inject_input.quit",
                                         0u,
                                         0.0,
                                         (const VNRunResult*)0);
                input_event.kind = VN_INPUT_KIND_QUIT;
                input_event.value0 = 0u;
                input_event.value1 = 0u;
                rc = vn_runtime_session_inject_input(session, &input_event);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview inject_input quit failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            }
        }
    }

    if (session != (VNRuntimeSession*)0) {
        report->session_done = (vn_u32)vn_runtime_session_is_done(session);
        (void)vn_runtime_session_destroy(session);
    }
    report->status_code = 0;
    report->error_code = VN_OK;
    report->error_name = preview_error_name(VN_OK);
    return 0;
}

static int preview_write_response(const VNPreviewRequest* req,
                                  const VNPreviewReport* report) {
    FILE* fp;
    vn_u32 i;

    if (req == (const VNPreviewRequest*)0 || report == (const VNPreviewReport*)0) {
        return 1;
    }

    if (req->response_path[0] != '\0') {
        fp = fopen(req->response_path, "w");
        if (fp == (FILE*)0) {
            (void)fprintf(stderr, "failed to open preview response: %s\n", req->response_path);
            return 1;
        }
    } else {
        fp = stdout;
    }

    (void)fprintf(fp, "{\n");
    (void)fprintf(fp, "  \"preview_protocol\":\"v1\",\n");
    (void)fprintf(fp, "  \"status\":\"%s\",\n", (report->status_code == 0) ? "ok" : "error");
    (void)fprintf(fp, "  \"error_code\":%d,\n", report->error_code);
    (void)fprintf(fp, "  \"error_name\":");
    preview_json_write_string(fp, report->error_name);
    (void)fprintf(fp, ",\n  \"error_message\":");
    preview_json_write_string(fp, report->error_message);
    (void)fprintf(fp, ",\n  \"host_os\":");
    preview_json_write_string(fp, vn_platform_host_os_name());
    (void)fprintf(fp, ",\n  \"host_arch\":");
    preview_json_write_string(fp, vn_platform_host_arch_name());
    (void)fprintf(fp, ",\n  \"request\":{\n");
    (void)fprintf(fp, "    \"project_dir\":");
    preview_json_write_string(fp, req->project_dir);
    (void)fprintf(fp, ",\n    \"pack_path\":");
    preview_json_write_string(fp, req->cfg.pack_path);
    (void)fprintf(fp, ",\n    \"scene_name\":");
    preview_json_write_string(fp, req->cfg.scene_name);
    (void)fprintf(fp, ",\n    \"backend_name\":");
    preview_json_write_string(fp, req->cfg.backend_name);
    (void)fprintf(fp, ",\n    \"width\":%u,\n", (unsigned int)req->cfg.width);
    (void)fprintf(fp, "    \"height\":%u,\n", (unsigned int)req->cfg.height);
    (void)fprintf(fp, "    \"frames\":%u,\n", (unsigned int)req->cfg.frames);
    (void)fprintf(fp, "    \"dt_ms\":%u,\n", (unsigned int)req->cfg.dt_ms);
    (void)fprintf(fp, "    \"trace\":%u,\n", (unsigned int)req->trace_enabled);
    (void)fprintf(fp, "    \"hold_on_end\":%u,\n", (unsigned int)req->cfg.hold_on_end);
    (void)fprintf(fp, "    \"choice_index\":%u,\n", (unsigned int)req->cfg.choice_index);
    (void)fprintf(fp, "    \"choice_seq_count\":%u,\n", (unsigned int)req->cfg.choice_seq_count);
    (void)fprintf(fp, "    \"command_count\":%u\n", (unsigned int)req->command_count);
    (void)fprintf(fp, "  },\n");
    (void)fprintf(fp, "  \"summary\":{\n");
    (void)fprintf(fp, "    \"reload_count\":%u,\n", (unsigned int)report->reload_count);
    (void)fprintf(fp, "    \"frame_samples\":%u,\n", (unsigned int)report->frame_samples);
    (void)fprintf(fp, "    \"session_done\":%u,\n", (unsigned int)report->session_done);
    (void)fprintf(fp, "    \"events_truncated\":%u\n", (unsigned int)report->events_truncated);
    (void)fprintf(fp, "  },\n");
    (void)fprintf(fp, "  \"perf_summary\":{\n");
    (void)fprintf(fp, "    \"samples\":%u,\n", (unsigned int)report->frame_samples);
    (void)fprintf(fp, "    \"total_step_ms\":%.3f,\n", report->total_step_ms);
    (void)fprintf(fp, "    \"avg_step_ms\":%.3f,\n",
                  (report->frame_samples == 0u) ? 0.0 : (report->total_step_ms / (double)report->frame_samples));
    (void)fprintf(fp, "    \"max_step_ms\":%.3f\n", report->max_step_ms);
    (void)fprintf(fp, "  },\n");
    (void)fprintf(fp, "  \"first_frame\":");
    preview_json_write_frame(fp, &report->first_frame);
    (void)fprintf(fp, ",\n  \"last_frame\":");
    preview_json_write_frame(fp, &report->last_frame);
    (void)fprintf(fp, ",\n  \"final_state\":");
    if (report->has_final_state != 0u) {
        preview_json_write_result(fp, &report->final_state);
    } else {
        (void)fprintf(fp, "null");
    }
    (void)fprintf(fp, ",\n  \"events\":[\n");
    for (i = 0u; i < report->event_count; ++i) {
        const VNPreviewEvent* event;

        event = &report->events[i];
        (void)fprintf(fp, "    {\"kind\":");
        preview_json_write_string(fp, event->name);
        (void)fprintf(fp, ",\"type\":%d,\"value\":%u", event->kind, (unsigned int)event->value);
        if (event->kind == VN_PREVIEW_EVENT_FRAME) {
            (void)fprintf(fp, ",\"host_step_ms\":%.3f,\"result\":", event->host_step_ms);
            preview_json_write_result(fp, &event->result);
        }
        (void)fprintf(fp, "}");
        if ((i + 1u) < report->event_count) {
            (void)fprintf(fp, ",");
        }
        (void)fprintf(fp, "\n");
    }
    (void)fprintf(fp, "  ]\n}\n");

    if (fp != stdout) {
        (void)fclose(fp);
    }
    return 0;
}

static void preview_print_usage(void) {
    (void)printf("usage: vn_previewd [--request FILE] [--response FILE] [options]\n");
    (void)printf("  --project-dir PATH\n");
    (void)printf("  --pack PATH\n");
    (void)printf("  --scene NAME\n");
    (void)printf("  --backend auto|scalar|avx2|avx2_asm|neon|rvv\n");
    (void)printf("  --resolution WIDTHxHEIGHT\n");
    (void)printf("  --frames N --dt-ms N --trace --hold-end\n");
    (void)printf("  --command run_to_end|step_frame[:N]|reload_scene|set_choice:N|inject_input:choice:N\n");
    (void)printf("  --command inject_input:key:C|inject_input:trace_toggle|inject_input:quit\n");
}

static void preview_str_copy(char* dst, size_t dst_size, const char* src) {
    size_t i;

    if (dst == (char*)0 || dst_size == 0u) {
        return;
    }
    if (src == (const char*)0) {
        dst[0] = '\0';
        return;
    }
    i = 0u;
    while (i + 1u < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i += 1u;
    }
    dst[i] = '\0';
}

static char* preview_trim(char* text) {
    char* end;

    if (text == (char*)0) {
        return (char*)0;
    }
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text += 1;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end -= 1;
    }
    *end = '\0';
    return text;
}

static int preview_parse_u32_range(const char* text,
                                   unsigned long min_value,
                                   unsigned long max_value,
                                   vn_u32* out_value) {
    char* end;
    unsigned long value;

    if (text == (const char*)0 || out_value == (vn_u32*)0 || text[0] == '\0') {
        return VN_E_INVALID_ARG;
    }
    value = strtoul(text, &end, 10);
    if (*end != '\0' || value < min_value || value > max_value) {
        return VN_E_INVALID_ARG;
    }
    *out_value = (vn_u32)value;
    return VN_OK;
}

static int preview_parse_bool(const char* text, vn_u32* out_value) {
    if (text == (const char*)0 || out_value == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }
    if (strcmp(text, "1") == 0 || strcmp(text, "true") == 0 || strcmp(text, "on") == 0 || strcmp(text, "yes") == 0) {
        *out_value = 1u;
        return VN_OK;
    }
    if (strcmp(text, "0") == 0 || strcmp(text, "false") == 0 || strcmp(text, "off") == 0 || strcmp(text, "no") == 0) {
        *out_value = 0u;
        return VN_OK;
    }
    return VN_E_INVALID_ARG;
}

static int preview_parse_resolution(const char* text,
                                    vn_u16* out_width,
                                    vn_u16* out_height) {
    const char* sep;
    char width_text[32];
    char height_text[32];
    size_t width_len;
    vn_u32 parsed_width;
    vn_u32 parsed_height;
    int rc;

    if (text == (const char*)0 || out_width == (vn_u16*)0 || out_height == (vn_u16*)0) {
        return VN_E_INVALID_ARG;
    }
    sep = strchr(text, 'x');
    if (sep == (const char*)0) {
        sep = strchr(text, 'X');
    }
    if (sep == (const char*)0) {
        return VN_E_INVALID_ARG;
    }
    width_len = (size_t)(sep - text);
    if (width_len == 0u || width_len + 1u >= sizeof(width_text)) {
        return VN_E_INVALID_ARG;
    }
    (void)memcpy(width_text, text, width_len);
    width_text[width_len] = '\0';
    preview_str_copy(height_text, sizeof(height_text), sep + 1);
    rc = preview_parse_u32_range(width_text, 1ul, 65535ul, &parsed_width);
    if (rc != VN_OK) {
        return rc;
    }
    rc = preview_parse_u32_range(height_text, 1ul, 65535ul, &parsed_height);
    if (rc != VN_OK) {
        return rc;
    }
    *out_width = (vn_u16)parsed_width;
    *out_height = (vn_u16)parsed_height;
    return VN_OK;
}

static int preview_parse_choice_seq(const char* text,
                                    vn_u8* out_items,
                                    vn_u32* out_count) {
    char buffer[VN_PREVIEW_LINE_MAX];
    char* token;
    vn_u32 count;
    vn_u32 value;
    int rc;

    if (text == (const char*)0 || out_items == (vn_u8*)0 || out_count == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }
    preview_str_copy(buffer, sizeof(buffer), text);
    count = 0u;
    token = strtok(buffer, ",");
    while (token != (char*)0) {
        if (count >= VN_RUNTIME_MAX_CHOICE_SEQ) {
            return VN_E_INVALID_ARG;
        }
        rc = preview_parse_u32_range(preview_trim(token), 0ul, 255ul, &value);
        if (rc != VN_OK) {
            return rc;
        }
        out_items[count] = (vn_u8)(value & 0xFFu);
        count += 1u;
        token = strtok((char*)0, ",");
    }
    *out_count = count;
    return VN_OK;
}

static int preview_parse_command(const char* text,
                                 VNPreviewCommand* out_command) {
    vn_u32 value;
    int rc;

    if (text == (const char*)0 || out_command == (VNPreviewCommand*)0) {
        return VN_E_INVALID_ARG;
    }
    out_command->kind = 0;
    out_command->value = 0u;
    if (strcmp(text, "run_to_end") == 0) {
        out_command->kind = VN_PREVIEW_CMD_RUN_TO_END;
        return VN_OK;
    }
    if (strcmp(text, "reload_scene") == 0) {
        out_command->kind = VN_PREVIEW_CMD_RELOAD_SCENE;
        return VN_OK;
    }
    if (strcmp(text, "step_frame") == 0) {
        out_command->kind = VN_PREVIEW_CMD_STEP_FRAME;
        out_command->value = 1u;
        return VN_OK;
    }
    if (strncmp(text, "step_frame:", 11) == 0) {
        rc = preview_parse_u32_range(text + 11, 1ul, 1000000ul, &value);
        if (rc != VN_OK) {
            return rc;
        }
        out_command->kind = VN_PREVIEW_CMD_STEP_FRAME;
        out_command->value = value;
        return VN_OK;
    }
    if (strncmp(text, "set_choice:", 11) == 0) {
        rc = preview_parse_u32_range(text + 11, 0ul, 255ul, &value);
        if (rc != VN_OK) {
            return rc;
        }
        out_command->kind = VN_PREVIEW_CMD_SET_CHOICE;
        out_command->value = value;
        return VN_OK;
    }
    if (strncmp(text, "inject_input:choice:", 20) == 0) {
        rc = preview_parse_u32_range(text + 20, 0ul, 255ul, &value);
        if (rc != VN_OK) {
            return rc;
        }
        out_command->kind = VN_PREVIEW_CMD_INJECT_CHOICE;
        out_command->value = value;
        return VN_OK;
    }
    if (strncmp(text, "inject_input:key:", 17) == 0) {
        if (text[17] == '\0' || text[18] != '\0') {
            return VN_E_INVALID_ARG;
        }
        out_command->kind = VN_PREVIEW_CMD_INJECT_KEY;
        out_command->value = (vn_u32)(unsigned char)text[17];
        return VN_OK;
    }
    if (strcmp(text, "inject_input:trace_toggle") == 0) {
        out_command->kind = VN_PREVIEW_CMD_INJECT_TRACE_TOGGLE;
        return VN_OK;
    }
    if (strcmp(text, "inject_input:quit") == 0) {
        out_command->kind = VN_PREVIEW_CMD_INJECT_QUIT;
        return VN_OK;
    }
    return VN_E_INVALID_ARG;
}

static int preview_add_command(VNPreviewRequest* req,
                               const VNPreviewCommand* command,
                               VNPreviewReport* report) {
    if (req == (VNPreviewRequest*)0 || command == (const VNPreviewCommand*)0) {
        return VN_E_INVALID_ARG;
    }
    if (req->command_count >= VN_PREVIEW_MAX_COMMANDS) {
        if (report != (VNPreviewReport*)0) {
            preview_error(report, VN_E_INVALID_ARG, "too many preview commands", 2);
        }
        return VN_E_INVALID_ARG;
    }
    req->commands[req->command_count] = *command;
    req->command_count += 1u;
    return VN_OK;
}

static void preview_error(VNPreviewReport* report,
                          int error_code,
                          const char* message,
                          int status_code) {
    if (report == (VNPreviewReport*)0) {
        return;
    }
    report->status_code = status_code;
    report->error_code = error_code;
    report->error_name = preview_error_name(error_code);
    preview_str_copy(report->error_message, sizeof(report->error_message), message);
    preview_report_add_event(report, VN_PREVIEW_EVENT_ERROR, report->error_name, (vn_u32)(unsigned int)(error_code & 0x7FFFFFFF), 0.0, (const VNRunResult*)0);
}

static const char* preview_error_name(int error_code) {
    if (error_code == VN_OK) {
        return "VN_OK";
    }
    if (error_code == VN_E_INVALID_ARG) {
        return "VN_E_INVALID_ARG";
    }
    if (error_code == VN_E_IO) {
        return "VN_E_IO";
    }
    if (error_code == VN_E_FORMAT) {
        return "VN_E_FORMAT";
    }
    if (error_code == VN_E_UNSUPPORTED) {
        return "VN_E_UNSUPPORTED";
    }
    if (error_code == VN_E_NOMEM) {
        return "VN_E_NOMEM";
    }
    if (error_code == VN_E_SCRIPT_BOUNDS) {
        return "VN_E_SCRIPT_BOUNDS";
    }
    if (error_code == VN_E_RENDER_STATE) {
        return "VN_E_RENDER_STATE";
    }
    if (error_code == VN_E_AUDIO_DEVICE) {
        return "VN_E_AUDIO_DEVICE";
    }
    return "VN_E_UNKNOWN";
}

static void preview_resolve_pack_path(VNPreviewRequest* req) {
    const char* leaf;
    const char* base_dir;

    if (req == (VNPreviewRequest*)0) {
        return;
    }
    leaf = req->pack_path;
    if (leaf[0] == '\0') {
        leaf = "assets/demo/demo.vnpak";
    }
    base_dir = req->project_dir;
    if (base_dir[0] == '\0') {
        base_dir = req->request_dir;
    }
    vn_platform_path_join(req->resolved_pack_path,
                      sizeof(req->resolved_pack_path),
                      base_dir,
                      leaf);
    if (req->scene_name[0] == '\0') {
        preview_str_copy(req->scene_name, sizeof(req->scene_name), "S0");
        req->cfg.scene_name = req->scene_name;
    }
    if (req->backend_name[0] == '\0') {
        preview_str_copy(req->backend_name, sizeof(req->backend_name), "auto");
        req->cfg.backend_name = req->backend_name;
    }
}

static void preview_report_add_event(VNPreviewReport* report,
                                     int kind,
                                     const char* name,
                                     vn_u32 value,
                                     double host_step_ms,
                                     const VNRunResult* result) {
    VNPreviewEvent* event;

    if (report == (VNPreviewReport*)0) {
        return;
    }
    if (report->event_count >= VN_PREVIEW_MAX_EVENTS) {
        report->events_truncated = 1u;
        return;
    }
    event = &report->events[report->event_count];
    (void)memset(event, 0, sizeof(VNPreviewEvent));
    event->kind = kind;
    preview_str_copy(event->name, sizeof(event->name), name);
    event->value = value;
    event->host_step_ms = host_step_ms;
    if (result != (const VNRunResult*)0) {
        event->has_result = 1u;
        event->result = *result;
    }
    report->event_count += 1u;
}

static void preview_report_add_frame(VNPreviewReport* report,
                                     const VNRunResult* result,
                                     double host_step_ms,
                                     vn_u32 trace_enabled) {
    if (report == (VNPreviewReport*)0 || result == (const VNRunResult*)0) {
        return;
    }
    if (report->first_frame.valid == 0u) {
        report->first_frame.valid = 1u;
        report->first_frame.host_step_ms = host_step_ms;
        report->first_frame.result = *result;
    }
    report->last_frame.valid = 1u;
    report->last_frame.host_step_ms = host_step_ms;
    report->last_frame.result = *result;
    report->has_final_state = 1u;
    report->final_state = *result;
    report->frame_samples += 1u;
    report->total_step_ms += host_step_ms;
    if (host_step_ms > report->max_step_ms) {
        report->max_step_ms = host_step_ms;
    }
    if (trace_enabled != 0u) {
        preview_report_add_event(report,
                                 VN_PREVIEW_EVENT_FRAME,
                                 "frame",
                                 result->frames_executed,
                                 host_step_ms,
                                 result);
    }
}

static int preview_step_frames(VNRuntimeSession* session,
                               vn_u32 count,
                               VNPreviewReport* report,
                               vn_u32 trace_enabled) {
    vn_u32 i;
    vn_u32 prev_frames;
    int rc;
    double t0;
    double t1;
    VNRunResult result;

    if (session == (VNRuntimeSession*)0 || report == (VNPreviewReport*)0) {
        return VN_E_INVALID_ARG;
    }
    (void)memset(&result, 0, sizeof(result));
    for (i = 0u; i < count; ++i) {
        if (vn_runtime_session_is_done(session) != VN_FALSE) {
            report->session_done = 1u;
            break;
        }
        prev_frames = result.frames_executed;
        t0 = preview_now_ms();
        rc = vn_runtime_session_step(session, &result);
        t1 = preview_now_ms();
        if (rc != VN_OK) {
            return rc;
        }
        if (result.frames_executed > prev_frames) {
            preview_report_add_frame(report, &result, t1 - t0, trace_enabled);
        }
        report->session_done = (vn_u32)vn_runtime_session_is_done(session);
        if (report->session_done != 0u) {
            break;
        }
    }
    return VN_OK;
}

static void preview_json_write_string(FILE* fp, const char* text) {
    const unsigned char* p;

    if (fp == (FILE*)0) {
        return;
    }
    if (text == (const char*)0) {
        (void)fprintf(fp, "null");
        return;
    }
    (void)fputc('"', fp);
    p = (const unsigned char*)text;
    while (*p != '\0') {
        if (*p == '"' || *p == '\\') {
            (void)fputc('\\', fp);
            (void)fputc((int)*p, fp);
        } else if (*p == '\n') {
            (void)fprintf(fp, "\\n");
        } else if (*p == '\r') {
            (void)fprintf(fp, "\\r");
        } else if (*p == '\t') {
            (void)fprintf(fp, "\\t");
        } else if (*p < 32u) {
            (void)fprintf(fp, " ");
        } else {
            (void)fputc((int)*p, fp);
        }
        p += 1;
    }
    (void)fputc('"', fp);
}

static void preview_json_write_result(FILE* fp, const VNRunResult* result) {
    if (fp == (FILE*)0 || result == (const VNRunResult*)0) {
        (void)fprintf(fp, "null");
        return;
    }
    (void)fprintf(fp, "{");
    (void)fprintf(fp, "\"frames_executed\":%u,", (unsigned int)result->frames_executed);
    (void)fprintf(fp, "\"text_id\":%u,", (unsigned int)result->text_id);
    (void)fprintf(fp, "\"vm_waiting\":%u,", (unsigned int)result->vm_waiting);
    (void)fprintf(fp, "\"vm_ended\":%u,", (unsigned int)result->vm_ended);
    (void)fprintf(fp, "\"vm_error\":%u,", (unsigned int)result->vm_error);
    (void)fprintf(fp, "\"fade_alpha\":%u,", (unsigned int)result->fade_alpha);
    (void)fprintf(fp, "\"fade_remain_ms\":%u,", (unsigned int)result->fade_remain_ms);
    (void)fprintf(fp, "\"bgm_id\":%u,", (unsigned int)result->bgm_id);
    (void)fprintf(fp, "\"se_id\":%u,", (unsigned int)result->se_id);
    (void)fprintf(fp, "\"choice_count\":%u,", (unsigned int)result->choice_count);
    (void)fprintf(fp, "\"choice_selected_index\":%u,", (unsigned int)result->choice_selected_index);
    (void)fprintf(fp, "\"choice_text_id\":%u,", (unsigned int)result->choice_text_id);
    (void)fprintf(fp, "\"op_count\":%u,", (unsigned int)result->op_count);
    (void)fprintf(fp, "\"perf_flags_effective\":%u,", (unsigned int)result->perf_flags_effective);
    (void)fprintf(fp, "\"frame_reuse_hits\":%u,", (unsigned int)result->frame_reuse_hits);
    (void)fprintf(fp, "\"frame_reuse_misses\":%u,", (unsigned int)result->frame_reuse_misses);
    (void)fprintf(fp, "\"op_cache_hits\":%u,", (unsigned int)result->op_cache_hits);
    (void)fprintf(fp, "\"op_cache_misses\":%u,", (unsigned int)result->op_cache_misses);
    (void)fprintf(fp, "\"dirty_tile_count\":%u,", (unsigned int)result->dirty_tile_count);
    (void)fprintf(fp, "\"dirty_rect_count\":%u,", (unsigned int)result->dirty_rect_count);
    (void)fprintf(fp, "\"dirty_full_redraw\":%u,", (unsigned int)result->dirty_full_redraw);
    (void)fprintf(fp, "\"dirty_tile_frames\":%u,", (unsigned int)result->dirty_tile_frames);
    (void)fprintf(fp, "\"dirty_tile_total\":%u,", (unsigned int)result->dirty_tile_total);
    (void)fprintf(fp, "\"dirty_rect_total\":%u,", (unsigned int)result->dirty_rect_total);
    (void)fprintf(fp, "\"dirty_full_redraws\":%u,", (unsigned int)result->dirty_full_redraws);
    (void)fprintf(fp, "\"render_width\":%u,", (unsigned int)result->render_width);
    (void)fprintf(fp, "\"render_height\":%u,", (unsigned int)result->render_height);
    (void)fprintf(fp, "\"dynamic_resolution_tier\":%u,", (unsigned int)result->dynamic_resolution_tier);
    (void)fprintf(fp, "\"dynamic_resolution_switches\":%u,", (unsigned int)result->dynamic_resolution_switches);
    (void)fprintf(fp, "\"backend_name\":");
    preview_json_write_string(fp, result->backend_name);
    (void)fprintf(fp, "}");
}

static void preview_json_write_frame(FILE* fp,
                                     const VNPreviewFrameSample* frame) {
    if (fp == (FILE*)0 || frame == (const VNPreviewFrameSample*)0 || frame->valid == 0u) {
        (void)fprintf(fp, "null");
        return;
    }
    (void)fprintf(fp, "{\"host_step_ms\":%.3f,\"result\":", frame->host_step_ms);
    preview_json_write_result(fp, &frame->result);
    (void)fprintf(fp, "}");
}

static double preview_now_ms(void) {
    return vn_platform_now_ms();
}
