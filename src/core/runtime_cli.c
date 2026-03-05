#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#endif

#include "vn_renderer.h"
#include "vn_frontend.h"
#include "vn_pack.h"
#include "vn_vm.h"
#include "vn_runtime.h"
#include "vn_error.h"

#define VN_MAX_CHOICE_SEQ 64u

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

static VNRunResult g_last_run_result;

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
    cfg->choice_index = 0u;
    cfg->choice_seq_count = 0u;
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

static int keyboard_enable(KeyboardInput* kb) {
#if defined(_WIN32)
    (void)kb;
    return VN_E_UNSUPPORTED;
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
    (void)kb;
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

        if (ch >= (unsigned char)'1' && ch <= (unsigned char)'9') {
            if (out_choice != (vn_u8*)0) {
                *out_choice = (vn_u8)(ch - (unsigned char)'1');
            }
            if (out_has_choice != (int*)0) {
                *out_has_choice = VN_TRUE;
            }
            continue;
        }
        if (ch == (unsigned char)'t' || ch == (unsigned char)'T') {
            if (out_toggle_trace != (int*)0) {
                *out_toggle_trace = VN_TRUE;
            }
            continue;
        }
        if (ch == (unsigned char)'q' || ch == (unsigned char)'Q') {
            kb->quit_requested = VN_TRUE;
            if (out_quit != (int*)0) {
                *out_quit = VN_TRUE;
            }
            continue;
        }
    }
#endif
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

int vn_runtime_run_cli(int argc, char** argv) {
    RendererConfig cfg;
    VNRuntimeState state;
    VNRenderOp ops[16];
    VNPak pak;
    VNState vm;
    ChoiceFeed choice_feed;
    FadePlayer fade_player;
    KeyboardInput keyboard;
    vn_u8* script_buf;
    vn_u32 script_size;
    const char* pack_path;
    const char* scene_name;
    vn_u32 frames;
    vn_u32 dt_ms;
    vn_u32 frame;
    vn_u32 frames_executed;
    vn_u32 trace;
    vn_u32 emit_logs;
    vn_u32 op_count;
    vn_u32 last_choice_serial;
    vn_u32 rc_u32;
    vn_u8 default_choice_index;
    int rc;
    int i;
    int pak_opened;
    int vm_ready;
    int keyboard_has_choice;
    int keyboard_toggle_trace;
    int keyboard_quit;
    int used_choice_seq;
    int exit_code;

    cfg.width = 600;
    cfg.height = 800;
    cfg.flags = VN_RENDERER_FLAG_SIMD;

    state_init_defaults(&state);

    choice_feed.count = 0u;
    choice_feed.cursor = 0u;
    fade_player_init(&fade_player);
    keyboard_init(&keyboard);

    script_buf = (vn_u8*)0;
    script_size = 0u;
    pack_path = "assets/demo/demo.vnpak";
    scene_name = "S0";
    frames = 1u;
    dt_ms = 16u;
    trace = 0u;

    pak.path = (const char*)0;
    pak.version = 0u;
    pak.resource_count = 0u;
    pak.entries = (ResourceEntry*)0;
    pak_opened = VN_FALSE;
    vm_ready = VN_FALSE;
    last_choice_serial = 0u;
    frames_executed = 0u;
    op_count = 0u;
    default_choice_index = 0u;
    keyboard_has_choice = VN_FALSE;
    keyboard_toggle_trace = VN_FALSE;
    keyboard_quit = VN_FALSE;
    used_choice_seq = VN_FALSE;
    exit_code = 0;
    emit_logs = 1u;
    runtime_result_reset();

    for (i = 1; i < argc; ++i) {
        const char* arg;
        arg = argv[i];

        if (strcmp(arg, "--backend") == 0) {
            vn_u32 force_flag;
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --backend\n");
                return 2;
            }
            i += 1;
            force_flag = parse_backend_flag(argv[i]);
            if (force_flag != 0u) {
                cfg.flags &= ~(VN_RENDERER_FLAG_FORCE_SCALAR |
                               VN_RENDERER_FLAG_FORCE_AVX2 |
                               VN_RENDERER_FLAG_FORCE_NEON |
                               VN_RENDERER_FLAG_FORCE_RVV);
                cfg.flags |= force_flag;
            }
        } else if (strncmp(arg, "--backend=", 10) == 0) {
            vn_u32 force_flag;
            force_flag = parse_backend_flag(arg + 10);
            if (force_flag != 0u) {
                cfg.flags &= ~(VN_RENDERER_FLAG_FORCE_SCALAR |
                               VN_RENDERER_FLAG_FORCE_AVX2 |
                               VN_RENDERER_FLAG_FORCE_NEON |
                               VN_RENDERER_FLAG_FORCE_RVV);
                cfg.flags |= force_flag;
            }
        } else if (strcmp(arg, "--resolution") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --resolution\n");
                return 2;
            }
            i += 1;
            rc = parse_resolution(argv[i], &cfg.width, &cfg.height);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid resolution: %s\n", argv[i]);
                return 2;
            }
        } else if (strncmp(arg, "--resolution=", 13) == 0) {
            rc = parse_resolution(arg + 13, &cfg.width, &cfg.height);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid resolution: %s\n", arg + 13);
                return 2;
            }
        } else if (strcmp(arg, "--scene") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --scene\n");
                return 2;
            }
            i += 1;
            scene_name = argv[i];
        } else if (strncmp(arg, "--scene=", 8) == 0) {
            scene_name = arg + 8;
        } else if (strcmp(arg, "--pack") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --pack\n");
                return 2;
            }
            i += 1;
            pack_path = argv[i];
        } else if (strncmp(arg, "--pack=", 7) == 0) {
            pack_path = arg + 7;
        } else if (strcmp(arg, "--choice-index") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --choice-index\n");
                return 2;
            }
            i += 1;
            rc = parse_u32_range(argv[i], 0l, 255l, &rc_u32);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --choice-index: %s\n", argv[i]);
                return 2;
            }
            default_choice_index = (vn_u8)(rc_u32 & 0xFFu);
        } else if (strncmp(arg, "--choice-index=", 15) == 0) {
            rc = parse_u32_range(arg + 15, 0l, 255l, &rc_u32);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --choice-index: %s\n", arg + 15);
                return 2;
            }
            default_choice_index = (vn_u8)(rc_u32 & 0xFFu);
        } else if (strcmp(arg, "--choice-seq") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --choice-seq\n");
                return 2;
            }
            i += 1;
            rc = parse_choice_seq(argv[i], &choice_feed);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --choice-seq: %s\n", argv[i]);
                return 2;
            }
        } else if (strncmp(arg, "--choice-seq=", 13) == 0) {
            rc = parse_choice_seq(arg + 13, &choice_feed);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --choice-seq: %s\n", arg + 13);
                return 2;
            }
        } else if (strcmp(arg, "--frames") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --frames\n");
                return 2;
            }
            i += 1;
            rc = parse_u32_range(argv[i], 1l, 1000000l, &frames);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --frames: %s\n", argv[i]);
                return 2;
            }
        } else if (strncmp(arg, "--frames=", 9) == 0) {
            rc = parse_u32_range(arg + 9, 1l, 1000000l, &frames);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --frames: %s\n", arg + 9);
                return 2;
            }
        } else if (strcmp(arg, "--dt-ms") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --dt-ms\n");
                return 2;
            }
            i += 1;
            rc = parse_u32_range(argv[i], 0l, 1000l, &dt_ms);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --dt-ms: %s\n", argv[i]);
                return 2;
            }
        } else if (strncmp(arg, "--dt-ms=", 8) == 0) {
            rc = parse_u32_range(arg + 8, 0l, 1000l, &dt_ms);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --dt-ms: %s\n", arg + 8);
                return 2;
            }
        } else if (strcmp(arg, "--keyboard") == 0) {
            keyboard.enabled = VN_TRUE;
        } else if (strcmp(arg, "--trace") == 0) {
            trace = 1u;
        } else if (strcmp(arg, "--quiet") == 0) {
            emit_logs = 0u;
        }
    }

    rc = parse_scene_id(scene_name, &state.scene_id);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "invalid scene: %s\n", scene_name);
        return 2;
    }

    state.clear_color = (vn_u32)(200u + (state.scene_id * 12u));

    rc = vnpak_open(&pak, pack_path);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vnpak_open failed rc=%d path=%s\n", rc, pack_path);
        return 1;
    }
    pak_opened = VN_TRUE;
    state.resource_count = pak.resource_count;

    rc = load_scene_script(&pak, state.scene_id, &script_buf, &script_size);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "load_scene_script failed rc=%d scene=%s\n", rc, scene_name);
        vnpak_close(&pak);
        return 1;
    }

    if (vm_init(&vm, script_buf, script_size) != VN_TRUE) {
        (void)fprintf(stderr, "vm_init failed scene=%s\n", scene_name);
        free(script_buf);
        vnpak_close(&pak);
        return 1;
    }
    vm_ready = VN_TRUE;
    last_choice_serial = vm_choice_serial(&vm);

    rc = renderer_init(&cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "renderer_init failed rc=%d\n", rc);
        free(script_buf);
        vnpak_close(&pak);
        return 1;
    }

    rc = keyboard_enable(&keyboard);
    if (keyboard.enabled == VN_TRUE && rc != VN_OK) {
        (void)fprintf(stderr, "keyboard init failed rc=%d (enable requires tty)\n", rc);
        renderer_shutdown();
        free(script_buf);
        vnpak_close(&pak);
        return 1;
    }
    if (keyboard.active == VN_TRUE && emit_logs != 0u) {
        (void)printf("[keyboard] enabled: press 1-9 to select choice, t to toggle trace, q to quit\n");
    }

    for (frame = 0u; frame < frames; ++frame) {
        vn_u32 choice_serial_now;
        vn_u8 applied_choice;

        state.frame_index = frame;
        state_reset_frame_events(&state);

        applied_choice = default_choice_index;
        used_choice_seq = VN_FALSE;
        keyboard_poll(&keyboard,
                      &applied_choice,
                      &keyboard_has_choice,
                      &keyboard_toggle_trace,
                      &keyboard_quit);
        if (keyboard_toggle_trace != VN_FALSE) {
            trace = (trace == 0u) ? 1u : 0u;
        }
        if (keyboard_quit != VN_FALSE) {
            break;
        }

        if (keyboard_has_choice != VN_FALSE) {
            used_choice_seq = VN_FALSE;
        } else if (choice_feed.count > 0u && choice_feed.cursor < choice_feed.count) {
            applied_choice = choice_feed.items[choice_feed.cursor];
            used_choice_seq = VN_TRUE;
        }
        vm_set_choice_index(&vm, applied_choice);

        vm_step(&vm, dt_ms);
        state_from_vm(&state, &vm);
        fade_player_step(&fade_player, &vm, dt_ms);
        state_apply_fade(&state, &fade_player);

        op_count = 16u;
        rc = build_render_ops(&state, ops, &op_count);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "build_render_ops failed rc=%d frame=%u\n", rc, (unsigned int)frame);
            exit_code = 1;
            break;
        }

        renderer_begin_frame();
        renderer_submit(ops, op_count);
        renderer_end_frame();

        choice_serial_now = vm_choice_serial(&vm);
        if (choice_serial_now != last_choice_serial) {
            last_choice_serial = choice_serial_now;
            if (used_choice_seq != VN_FALSE && choice_feed.cursor < choice_feed.count) {
                choice_feed.cursor += 1u;
            }
        }

        frames_executed = frame + 1u;

        if (trace != 0u && emit_logs != 0u) {
            (void)printf("frame=%u text=%u wait=%u end=%u fade=%u fade_remain=%u bgm=%u se=%u choice_count=%u choice_sel=%u choice_text=%u ops=%u\n",
                         (unsigned int)state.frame_index,
                         (unsigned int)state.text_id,
                         (unsigned int)state.vm_waiting,
                         (unsigned int)state.vm_ended,
                         (unsigned int)state.fade_alpha,
                         (unsigned int)state.fade_duration_ms,
                         (unsigned int)state.bgm_id,
                         (unsigned int)state.se_id,
                         (unsigned int)state.choice_count,
                         (unsigned int)state.choice_selected_index,
                         (unsigned int)state.choice_text_id,
                         (unsigned int)op_count);
        }

        if (state.vm_ended != 0u || state.vm_error != 0u) {
            if (state.vm_error != 0u) {
                exit_code = 1;
            }
            break;
        }
    }

    if (exit_code == 0 && emit_logs != 0u) {
        (void)printf("vn_runtime ok backend=%s resolution=%ux%u scene=%s frames=%u dt=%u resources=%u text=%u wait=%u end=%u fade=%u fade_remain=%u bgm=%u se=%u choice=%u choice_sel=%u choice_text=%u err=%u ops=%u keyboard=%u\n",
                     renderer_backend_name(),
                     (unsigned int)cfg.width,
                     (unsigned int)cfg.height,
                     scene_name,
                     (unsigned int)frames_executed,
                     (unsigned int)dt_ms,
                     (unsigned int)state.resource_count,
                     (unsigned int)state.text_id,
                     (unsigned int)state.vm_waiting,
                     (unsigned int)state.vm_ended,
                     (unsigned int)state.fade_alpha,
                     (unsigned int)state.fade_duration_ms,
                     (unsigned int)state.bgm_id,
                     (unsigned int)state.se_id,
                     (unsigned int)state.choice_count,
                     (unsigned int)state.choice_selected_index,
                     (unsigned int)state.choice_text_id,
                     (unsigned int)state.vm_error,
                     (unsigned int)op_count,
                     (unsigned int)keyboard.active);
    }

    g_last_run_result.frames_executed = frames_executed;
    g_last_run_result.text_id = state.text_id;
    g_last_run_result.vm_waiting = state.vm_waiting;
    g_last_run_result.vm_ended = state.vm_ended;
    g_last_run_result.vm_error = state.vm_error;
    g_last_run_result.fade_alpha = state.fade_alpha;
    g_last_run_result.fade_remain_ms = state.fade_duration_ms;
    g_last_run_result.bgm_id = state.bgm_id;
    g_last_run_result.se_id = state.se_id;
    g_last_run_result.choice_count = state.choice_count;
    g_last_run_result.choice_selected_index = state.choice_selected_index;
    g_last_run_result.choice_text_id = state.choice_text_id;
    g_last_run_result.op_count = op_count;
    g_last_run_result.backend_name = renderer_backend_name();

    renderer_shutdown();
    keyboard_disable(&keyboard);
    if (vm_ready != VN_FALSE) {
        free(script_buf);
    }
    if (pak_opened == VN_TRUE) {
        vnpak_close(&pak);
    }
    return exit_code;
}

int vn_runtime_run(const VNRunConfig* run_cfg, VNRunResult* out_result) {
    VNRunConfig cfg_local;
    const VNRunConfig* cfg;
    char arg_backend[64];
    char arg_scene[64];
    char arg_pack[512];
    char arg_resolution[64];
    char arg_frames[64];
    char arg_dt[64];
    char arg_choice_index[64];
    char arg_choice_seq[256];
    char* argv[16];
    int argc;
    int rc;

    cfg = run_cfg;
    if (cfg == (const VNRunConfig*)0) {
        vn_run_config_init(&cfg_local);
        cfg = &cfg_local;
    }

    argc = 0;
    argv[argc++] = (char*)"vn_runtime";

    if (cfg->backend_name != (const char*)0 && cfg->backend_name[0] != '\0' &&
        strcmp(cfg->backend_name, "auto") != 0) {
        (void)sprintf(arg_backend, "--backend=%s", cfg->backend_name);
        argv[argc++] = arg_backend;
    }

    if (cfg->scene_name != (const char*)0 && cfg->scene_name[0] != '\0') {
        (void)sprintf(arg_scene, "--scene=%s", cfg->scene_name);
        argv[argc++] = arg_scene;
    }

    if (cfg->pack_path != (const char*)0 && cfg->pack_path[0] != '\0') {
        (void)sprintf(arg_pack, "--pack=%s", cfg->pack_path);
        argv[argc++] = arg_pack;
    }

    (void)sprintf(arg_resolution, "--resolution=%ux%u",
                  (unsigned int)cfg->width,
                  (unsigned int)cfg->height);
    argv[argc++] = arg_resolution;

    (void)sprintf(arg_frames, "--frames=%u", (unsigned int)cfg->frames);
    argv[argc++] = arg_frames;

    (void)sprintf(arg_dt, "--dt-ms=%u", (unsigned int)cfg->dt_ms);
    argv[argc++] = arg_dt;

    (void)sprintf(arg_choice_index, "--choice-index=%u", (unsigned int)cfg->choice_index);
    argv[argc++] = arg_choice_index;

    if (cfg->choice_seq_count > 0u) {
        vn_u32 i;
        int offset;
        offset = 0;
        arg_choice_seq[0] = '\0';
        for (i = 0u; i < cfg->choice_seq_count; ++i) {
            char tmp[8];
            (void)sprintf(tmp, "%u", (unsigned int)cfg->choice_seq[i]);
            if (offset != 0) {
                if (offset + 1 >= (int)sizeof(arg_choice_seq)) {
                    break;
                }
                arg_choice_seq[offset] = ',';
                offset += 1;
                arg_choice_seq[offset] = '\0';
            }
            if (offset + (int)strlen(tmp) >= (int)sizeof(arg_choice_seq)) {
                break;
            }
            (void)strcat(arg_choice_seq, tmp);
            offset += (int)strlen(tmp);
        }
        if (arg_choice_seq[0] != '\0') {
            static char arg_choice_seq_wrap[272];
            (void)sprintf(arg_choice_seq_wrap, "--choice-seq=%s", arg_choice_seq);
            argv[argc++] = arg_choice_seq_wrap;
        }
    }

    if (cfg->trace != 0u) {
        argv[argc++] = (char*)"--trace";
    }
    if (cfg->keyboard != 0u) {
        argv[argc++] = (char*)"--keyboard";
    }
    if (cfg->emit_logs == 0u) {
        argv[argc++] = (char*)"--quiet";
    }

    rc = vn_runtime_run_cli(argc, argv);
    if (out_result != (VNRunResult*)0) {
        *out_result = g_last_run_result;
    }
    return rc;
}
