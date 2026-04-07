#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_internal.h"
#include "platform.h"

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

static void runtime_parse_perf_flag_set(vn_u32* perf_flags, vn_u32 flag, int enabled) {
    if (perf_flags == (vn_u32*)0) {
        return;
    }
    if (enabled != VN_FALSE) {
        *perf_flags |= flag;
    } else {
        *perf_flags &= ~flag;
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

static int parse_u32_any(const char* text, vn_u32* out_value) {
    unsigned long value;
    char* end_ptr;

    if (text == (const char*)0 || out_value == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }

    value = strtoul(text, &end_ptr, 10);
    if (end_ptr == text || *end_ptr != '\0' || value > 0xFFFFFFFFul) {
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

vn_u32 parse_backend_flag(const char* value) {
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

int parse_scene_id(const char* value, vn_u32* out_scene_id) {
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

int runtime_cli_report_error(const char* trace_id,
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

int runtime_cli_report_missing_value(const char* arg_name) {
    return runtime_cli_report_error("runtime.cli.arg.missing",
                                    VN_E_INVALID_ARG,
                                    "missing value",
                                    arg_name,
                                    (const char*)0,
                                    2);
}

int runtime_cli_report_invalid_value(const char* arg_name, const char* arg_value) {
    return runtime_cli_report_error("runtime.cli.arg.invalid",
                                    VN_E_INVALID_ARG,
                                    "invalid value",
                                    arg_name,
                                    arg_value,
                                    2);
}

int runtime_cli_report_invalid_combo(const char* arg_name, const char* arg_value) {
    return runtime_cli_report_error("runtime.cli.arg.invalid",
                                    VN_E_INVALID_ARG,
                                    "invalid argument combination",
                                    arg_name,
                                    arg_value,
                                    2);
}

const char* scene_name_from_id(vn_u32 scene_id) {
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

void runtime_cli_options_init(VNRuntimeCliOptions* options) {
    if (options == (VNRuntimeCliOptions*)0) {
        return;
    }

    vn_run_config_init(&options->run_cfg);
    options->choice_feed.count = 0u;
    options->choice_feed.cursor = 0u;
    options->load_save_path = (const char*)0;
    options->save_out_path = (const char*)0;
    options->load_save_conflict_arg = (const char*)0;
    options->save_metadata_arg = (const char*)0;
    options->save_slot_id = 0u;
    options->save_timestamp_s = 0u;
}

static void runtime_cli_mark_load_conflict(VNRuntimeCliOptions* options, const char* arg_name) {
    if (options->load_save_conflict_arg == (const char*)0) {
        options->load_save_conflict_arg = arg_name;
    }
}

static void runtime_cli_mark_save_metadata(VNRuntimeCliOptions* options, const char* arg_name) {
    if (options->save_metadata_arg == (const char*)0) {
        options->save_metadata_arg = arg_name;
    }
}

static int runtime_cli_take_next_value(int argc,
                                       char** argv,
                                       int* io_index,
                                       const char* arg_name,
                                       const char** out_value) {
    if ((io_index == (int*)0) || out_value == (const char**)0) {
        return runtime_cli_report_invalid_value(arg_name, (const char*)0);
    }
    if ((*io_index + 1) >= argc) {
        return runtime_cli_report_missing_value(arg_name);
    }
    *io_index += 1;
    *out_value = argv[*io_index];
    return VN_OK;
}

int runtime_cli_parse_options(VNRuntimeCliOptions* options, int argc, char** argv) {
    int i;

    if (options == (VNRuntimeCliOptions*)0 || argv == (char**)0 || argc < 0) {
        return runtime_cli_report_error("runtime.cli.arg.invalid",
                                        VN_E_INVALID_ARG,
                                        "invalid argv",
                                        "argv",
                                        (const char*)0,
                                        2);
    }

    runtime_cli_options_init(options);

    for (i = 1; i < argc; ++i) {
        const char* arg;

        arg = argv[i];
        if (strcmp(arg, "--load-save") == 0) {
            const char* value = (const char*)0;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--load-save", &value);
            if (rc != VN_OK) {
                return rc;
            }
            options->load_save_path = value;
        } else if (strncmp(arg, "--load-save=", 12) == 0) {
            options->load_save_path = arg + 12;
        } else if (strcmp(arg, "--save-out") == 0) {
            const char* value = (const char*)0;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--save-out", &value);
            if (rc != VN_OK) {
                return rc;
            }
            options->save_out_path = value;
        } else if (strncmp(arg, "--save-out=", 11) == 0) {
            options->save_out_path = arg + 11;
        } else if (strcmp(arg, "--save-slot") == 0) {
            const char* value = (const char*)0;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--save-slot", &value);
            if (rc != VN_OK) {
                return rc;
            }
            rc = parse_u32_any(value, &options->save_slot_id);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--save-slot", value);
            }
            runtime_cli_mark_save_metadata(options, "--save-slot");
        } else if (strncmp(arg, "--save-slot=", 12) == 0) {
            int rc;

            rc = parse_u32_any(arg + 12, &options->save_slot_id);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--save-slot", arg + 12);
            }
            runtime_cli_mark_save_metadata(options, "--save-slot");
        } else if (strcmp(arg, "--save-timestamp") == 0) {
            const char* value = (const char*)0;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--save-timestamp", &value);
            if (rc != VN_OK) {
                return rc;
            }
            rc = parse_u32_any(value, &options->save_timestamp_s);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--save-timestamp", value);
            }
            runtime_cli_mark_save_metadata(options, "--save-timestamp");
        } else if (strncmp(arg, "--save-timestamp=", 17) == 0) {
            int rc;

            rc = parse_u32_any(arg + 17, &options->save_timestamp_s);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--save-timestamp", arg + 17);
            }
            runtime_cli_mark_save_metadata(options, "--save-timestamp");
        } else if (strcmp(arg, "--backend") == 0) {
            const char* value = (const char*)0;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--backend", &value);
            if (rc != VN_OK) {
                return rc;
            }
            options->run_cfg.backend_name = value;
            runtime_cli_mark_load_conflict(options, "--backend");
        } else if (strncmp(arg, "--backend=", 10) == 0) {
            options->run_cfg.backend_name = arg + 10;
            runtime_cli_mark_load_conflict(options, "--backend");
        } else if (strcmp(arg, "--resolution") == 0) {
            const char* value = (const char*)0;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--resolution", &value);
            if (rc != VN_OK) {
                return rc;
            }
            rc = parse_resolution(value, &options->run_cfg.width, &options->run_cfg.height);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--resolution", value);
            }
            runtime_cli_mark_load_conflict(options, "--resolution");
        } else if (strncmp(arg, "--resolution=", 13) == 0) {
            int rc;

            rc = parse_resolution(arg + 13, &options->run_cfg.width, &options->run_cfg.height);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--resolution", arg + 13);
            }
            runtime_cli_mark_load_conflict(options, "--resolution");
        } else if (strcmp(arg, "--scene") == 0) {
            const char* value = (const char*)0;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--scene", &value);
            if (rc != VN_OK) {
                return rc;
            }
            options->run_cfg.scene_name = value;
            runtime_cli_mark_load_conflict(options, "--scene");
        } else if (strncmp(arg, "--scene=", 8) == 0) {
            options->run_cfg.scene_name = arg + 8;
            runtime_cli_mark_load_conflict(options, "--scene");
        } else if (strcmp(arg, "--pack") == 0) {
            const char* value = (const char*)0;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--pack", &value);
            if (rc != VN_OK) {
                return rc;
            }
            options->run_cfg.pack_path = value;
            runtime_cli_mark_load_conflict(options, "--pack");
        } else if (strncmp(arg, "--pack=", 7) == 0) {
            options->run_cfg.pack_path = arg + 7;
            runtime_cli_mark_load_conflict(options, "--pack");
        } else if (strcmp(arg, "--choice-index") == 0) {
            const char* value = (const char*)0;
            vn_u32 parsed;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--choice-index", &value);
            if (rc != VN_OK) {
                return rc;
            }
            rc = parse_u32_range(value, 0l, 255l, &parsed);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--choice-index", value);
            }
            options->run_cfg.choice_index = (vn_u8)(parsed & 0xFFu);
            runtime_cli_mark_load_conflict(options, "--choice-index");
        } else if (strncmp(arg, "--choice-index=", 15) == 0) {
            vn_u32 parsed;
            int rc;

            rc = parse_u32_range(arg + 15, 0l, 255l, &parsed);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--choice-index", arg + 15);
            }
            options->run_cfg.choice_index = (vn_u8)(parsed & 0xFFu);
            runtime_cli_mark_load_conflict(options, "--choice-index");
        } else if (strcmp(arg, "--choice-seq") == 0) {
            const char* value = (const char*)0;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--choice-seq", &value);
            if (rc != VN_OK) {
                return rc;
            }
            rc = parse_choice_seq(value, &options->choice_feed);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--choice-seq", value);
            }
            runtime_cli_mark_load_conflict(options, "--choice-seq");
        } else if (strncmp(arg, "--choice-seq=", 13) == 0) {
            int rc;

            rc = parse_choice_seq(arg + 13, &options->choice_feed);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--choice-seq", arg + 13);
            }
            runtime_cli_mark_load_conflict(options, "--choice-seq");
        } else if (strcmp(arg, "--frames") == 0) {
            const char* value = (const char*)0;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--frames", &value);
            if (rc != VN_OK) {
                return rc;
            }
            rc = parse_u32_range(value, 1l, 1000000l, &options->run_cfg.frames);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--frames", value);
            }
            runtime_cli_mark_load_conflict(options, "--frames");
        } else if (strncmp(arg, "--frames=", 9) == 0) {
            int rc;

            rc = parse_u32_range(arg + 9, 1l, 1000000l, &options->run_cfg.frames);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--frames", arg + 9);
            }
            runtime_cli_mark_load_conflict(options, "--frames");
        } else if (strcmp(arg, "--dt-ms") == 0) {
            const char* value = (const char*)0;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--dt-ms", &value);
            if (rc != VN_OK) {
                return rc;
            }
            rc = parse_u32_range(value, 0l, 1000l, &options->run_cfg.dt_ms);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--dt-ms", value);
            }
            runtime_cli_mark_load_conflict(options, "--dt-ms");
        } else if (strncmp(arg, "--dt-ms=", 8) == 0) {
            int rc;

            rc = parse_u32_range(arg + 8, 0l, 1000l, &options->run_cfg.dt_ms);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--dt-ms", arg + 8);
            }
            runtime_cli_mark_load_conflict(options, "--dt-ms");
        } else if (strcmp(arg, "--keyboard") == 0) {
            options->run_cfg.keyboard = 1u;
            runtime_cli_mark_load_conflict(options, "--keyboard");
        } else if (strcmp(arg, "--trace") == 0) {
            options->run_cfg.trace = 1u;
        } else if (strcmp(arg, "--hold-end") == 0) {
            options->run_cfg.hold_on_end = 1u;
            runtime_cli_mark_load_conflict(options, "--hold-end");
        } else if (strcmp(arg, "--perf-op-cache") == 0) {
            const char* value = (const char*)0;
            int enabled;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--perf-op-cache", &value);
            if (rc != VN_OK) {
                return rc;
            }
            rc = parse_toggle_value(value, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-op-cache", value);
            }
            runtime_parse_perf_flag_set(&options->run_cfg.perf_flags, VN_RUNTIME_PERF_OP_CACHE, enabled);
            runtime_cli_mark_load_conflict(options, "--perf-op-cache");
        } else if (strncmp(arg, "--perf-op-cache=", 16) == 0) {
            int enabled;
            int rc;

            rc = parse_toggle_value(arg + 16, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-op-cache", arg + 16);
            }
            runtime_parse_perf_flag_set(&options->run_cfg.perf_flags, VN_RUNTIME_PERF_OP_CACHE, enabled);
            runtime_cli_mark_load_conflict(options, "--perf-op-cache");
        } else if (strcmp(arg, "--perf-dirty-tile") == 0) {
            const char* value = (const char*)0;
            int enabled;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--perf-dirty-tile", &value);
            if (rc != VN_OK) {
                return rc;
            }
            rc = parse_toggle_value(value, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-dirty-tile", value);
            }
            runtime_parse_perf_flag_set(&options->run_cfg.perf_flags, VN_RUNTIME_PERF_DIRTY_TILE, enabled);
            runtime_cli_mark_load_conflict(options, "--perf-dirty-tile");
        } else if (strncmp(arg, "--perf-dirty-tile=", 18) == 0) {
            int enabled;
            int rc;

            rc = parse_toggle_value(arg + 18, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-dirty-tile", arg + 18);
            }
            runtime_parse_perf_flag_set(&options->run_cfg.perf_flags, VN_RUNTIME_PERF_DIRTY_TILE, enabled);
            runtime_cli_mark_load_conflict(options, "--perf-dirty-tile");
        } else if (strcmp(arg, "--perf-dynamic-resolution") == 0) {
            const char* value = (const char*)0;
            int enabled;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--perf-dynamic-resolution", &value);
            if (rc != VN_OK) {
                return rc;
            }
            rc = parse_toggle_value(value, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-dynamic-resolution", value);
            }
            runtime_parse_perf_flag_set(&options->run_cfg.perf_flags, VN_RUNTIME_PERF_DYNAMIC_RESOLUTION, enabled);
            runtime_cli_mark_load_conflict(options, "--perf-dynamic-resolution");
        } else if (strncmp(arg, "--perf-dynamic-resolution=", 26) == 0) {
            int enabled;
            int rc;

            rc = parse_toggle_value(arg + 26, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-dynamic-resolution", arg + 26);
            }
            runtime_parse_perf_flag_set(&options->run_cfg.perf_flags, VN_RUNTIME_PERF_DYNAMIC_RESOLUTION, enabled);
            runtime_cli_mark_load_conflict(options, "--perf-dynamic-resolution");
        } else if (strcmp(arg, "--perf-frame-reuse") == 0) {
            const char* value = (const char*)0;
            int enabled;
            int rc;

            rc = runtime_cli_take_next_value(argc, argv, &i, "--perf-frame-reuse", &value);
            if (rc != VN_OK) {
                return rc;
            }
            rc = parse_toggle_value(value, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-frame-reuse", value);
            }
            runtime_parse_perf_flag_set(&options->run_cfg.perf_flags, VN_RUNTIME_PERF_FRAME_REUSE, enabled);
            runtime_cli_mark_load_conflict(options, "--perf-frame-reuse");
        } else if (strncmp(arg, "--perf-frame-reuse=", 19) == 0) {
            int enabled;
            int rc;

            rc = parse_toggle_value(arg + 19, &enabled);
            if (rc != VN_OK) {
                return runtime_cli_report_invalid_value("--perf-frame-reuse", arg + 19);
            }
            runtime_parse_perf_flag_set(&options->run_cfg.perf_flags, VN_RUNTIME_PERF_FRAME_REUSE, enabled);
            runtime_cli_mark_load_conflict(options, "--perf-frame-reuse");
        } else if (strcmp(arg, "--quiet") == 0) {
            options->run_cfg.emit_logs = 0u;
        }
    }

    return VN_OK;
}

static int runtime_cli_step_until_done(VNRuntimeSession* session,
                                       int sleep_between_frames,
                                       vn_u32 sleep_dt_ms) {
    int rc;

    if (session == (VNRuntimeSession*)0) {
        return VN_E_INVALID_ARG;
    }

    rc = VN_OK;
    while (vn_runtime_session_is_done(session) == VN_FALSE) {
        rc = vn_runtime_session_step(session, (VNRunResult*)0);
        if (rc != VN_OK) {
            break;
        }
        if (sleep_between_frames != VN_FALSE &&
            vn_runtime_session_is_done(session) == VN_FALSE) {
            vn_platform_sleep_ms((unsigned int)sleep_dt_ms);
        }
    }

    if (rc == VN_OK && session->exit_code != 0) {
        rc = session->exit_code;
    }
    return rc;
}

static int runtime_cli_run_loaded_session(const VNRuntimeCliOptions* options) {
    VNRuntimeSession* session;
    int rc;

    session = (VNRuntimeSession*)0;
    rc = vn_runtime_session_load_from_file(options->load_save_path, &session);
    if (rc != VN_OK) {
        return runtime_cli_report_error("runtime.run.failed",
                                        rc,
                                        "vn_runtime_session_load_from_file failed",
                                        "save",
                                        options->load_save_path,
                                        1);
    }

    if (options->run_cfg.trace != 0u) {
        session->trace = 1u;
    }
    if (options->run_cfg.emit_logs == 0u) {
        session->emit_logs = 0u;
    }

    rc = runtime_cli_step_until_done(session,
                                     (session->keyboard.active != VN_FALSE && session->dt_ms > 0u) ? VN_TRUE : VN_FALSE,
                                     session->dt_ms);
    if (rc == VN_OK && options->save_out_path != (const char*)0) {
        rc = vn_runtime_session_save_to_file(session,
                                             options->save_out_path,
                                             options->save_slot_id,
                                             options->save_timestamp_s);
    }
    (void)vn_runtime_session_destroy(session);

    if (rc != 0) {
        return runtime_cli_report_error("runtime.run.failed",
                                        rc,
                                        "vn_runtime_session_load_from_file run failed",
                                        "save",
                                        options->load_save_path,
                                        1);
    }
    return 0;
}

static void runtime_cli_copy_choice_feed(VNRunConfig* run_cfg, const ChoiceFeed* choice_feed) {
    vn_u32 i;

    if (run_cfg == (VNRunConfig*)0 || choice_feed == (const ChoiceFeed*)0) {
        return;
    }

    run_cfg->choice_seq_count = choice_feed->count;
    for (i = 0u; i < choice_feed->count; ++i) {
        run_cfg->choice_seq[i] = choice_feed->items[i];
    }
}

static int runtime_cli_run_fresh_session(const VNRuntimeCliOptions* options) {
    VNRunConfig run_cfg;
    VNRuntimeSession* session;
    vn_u32 scene_id;
    int rc;

    run_cfg = options->run_cfg;
    runtime_cli_copy_choice_feed(&run_cfg, &options->choice_feed);

    rc = parse_scene_id(run_cfg.scene_name, &scene_id);
    if (rc != VN_OK) {
        return runtime_cli_report_error("runtime.cli.scene.invalid",
                                        VN_E_INVALID_ARG,
                                        "invalid scene",
                                        "scene",
                                        run_cfg.scene_name,
                                        2);
    }
    (void)scene_id;

    if (options->save_out_path == (const char*)0) {
        rc = vn_runtime_run(&run_cfg, (VNRunResult*)0);
    } else {
        session = (VNRuntimeSession*)0;
        rc = vn_runtime_session_create(&run_cfg, &session);
        if (rc == VN_OK) {
            rc = runtime_cli_step_until_done(session,
                                             (run_cfg.keyboard != 0u && run_cfg.dt_ms > 0u) ? VN_TRUE : VN_FALSE,
                                             run_cfg.dt_ms);
            if (rc == VN_OK) {
                rc = vn_runtime_session_save_to_file(session,
                                                     options->save_out_path,
                                                     options->save_slot_id,
                                                     options->save_timestamp_s);
            }
            (void)vn_runtime_session_destroy(session);
        }
    }

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

int vn_runtime_run_cli(int argc, char** argv) {
    VNRuntimeCliOptions options;
    int rc;

    rc = runtime_cli_parse_options(&options, argc, argv);
    if (rc != VN_OK) {
        return rc;
    }

    if (options.load_save_path != (const char*)0) {
        if (options.load_save_conflict_arg != (const char*)0) {
            return runtime_cli_report_invalid_combo("--load-save", options.load_save_conflict_arg);
        }
        return runtime_cli_run_loaded_session(&options);
    }

    if (options.save_out_path == (const char*)0 && options.save_metadata_arg != (const char*)0) {
        return runtime_cli_report_invalid_combo(options.save_metadata_arg, "--save-out");
    }

    return runtime_cli_run_fresh_session(&options);
}
