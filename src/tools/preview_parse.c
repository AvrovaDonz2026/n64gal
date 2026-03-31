#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vn_error.h"
#include "preview_internal.h"
#include "../core/platform.h"

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
static int preview_load_request_file(VNPreviewRequest* req,
                                     VNPreviewReport* report,
                                     const char* path);
static void preview_resolve_pack_path(VNPreviewRequest* req);

int preview_parse_cli(VNPreviewRequest* req,
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
            report->error_name = vn_error_name(VN_OK);
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
            report->error_name = vn_error_name(VN_OK);
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
