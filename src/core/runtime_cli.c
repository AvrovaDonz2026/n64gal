#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <conio.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/time.h>
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

struct VNRuntimeSession {
    RendererConfig renderer_cfg;
    VNRuntimeState state;
    VNRenderOp ops[16];
    VNPak pak;
    VNState vm;
    ChoiceFeed choice_feed;
    FadePlayer fade_player;
    KeyboardInput keyboard;
    vn_u8* script_buf;
    vn_u32 script_size;
    vn_u32 frames_limit;
    vn_u32 dt_ms;
    vn_u32 trace;
    vn_u32 emit_logs;
    vn_u32 hold_on_end;
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

static VNRunResult g_last_run_result;

static double runtime_now_ms(void) {
#if !defined(_WIN32)
    struct timeval tv;

    if (gettimeofday(&tv, (struct timezone*)0) != 0) {
        return 0.0;
    }
    return ((double)tv.tv_sec * 1000.0) + ((double)tv.tv_usec / 1000.0);
#else
    clock_t now_ticks;

    now_ticks = clock();
    if (now_ticks == (clock_t)-1) {
        return 0.0;
    }
    return ((double)now_ticks * 1000.0) / (double)CLOCKS_PER_SEC;
#endif
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
                                         VN_RENDERER_FLAG_FORCE_RVV);
        session->renderer_cfg.flags |= force_flag;
    }

    session->frames_limit = active_cfg->frames;
    session->dt_ms = active_cfg->dt_ms;
    session->trace = active_cfg->trace;
    session->emit_logs = active_cfg->emit_logs;
    session->hold_on_end = active_cfg->hold_on_end;
    session->default_choice_index = active_cfg->choice_index;
    session->keyboard.enabled = (active_cfg->keyboard != 0u) ? VN_TRUE : VN_FALSE;
    session->last_op_count = 0u;
    session->done = VN_FALSE;
    session->exit_code = 0;
    session->summary_emitted = VN_FALSE;
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
    int keyboard_has_choice;
    int keyboard_toggle_trace;
    int keyboard_quit;
    int used_choice_seq;

    if (session == (VNRuntimeSession*)0) {
        return VN_E_INVALID_ARG;
    }

    if (session->done == VN_FALSE && session->frames_executed < session->frames_limit) {
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
            state_from_vm(&session->state, &session->vm);
            fade_player_step(&session->fade_player, &session->vm, session->dt_ms);
            state_apply_fade(&session->state, &session->fade_player);

            op_count = 16u;
            rc = build_render_ops(&session->state, session->ops, &op_count);
            t_after_build = runtime_now_ms();
            if (rc != VN_OK) {
                (void)fprintf(stderr, "build_render_ops failed rc=%d frame=%u\n",
                              rc,
                              (unsigned int)session->state.frame_index);
                session->exit_code = 1;
                session->done = VN_TRUE;
            } else {
                renderer_begin_frame();
                renderer_submit(session->ops, op_count);
                renderer_end_frame();
                t_after_raster = runtime_now_ms();

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
                    (void)printf("frame=%u frame_ms=%.3f vm_ms=%.3f build_ms=%.3f raster_ms=%.3f audio_ms=%.3f rss_mb=%.3f text=%u wait=%u end=%u fade=%u fade_remain=%u bgm=%u se=%u choice_count=%u choice_sel=%u choice_text=%u ops=%u\n",
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
                                 (unsigned int)session->state.choice_text_id,
                                 (unsigned int)op_count);
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
        (void)printf("vn_runtime ok backend=%s resolution=%ux%u scene=%s frames=%u dt=%u resources=%u text=%u wait=%u end=%u fade=%u fade_remain=%u bgm=%u se=%u choice=%u choice_sel=%u choice_text=%u err=%u ops=%u keyboard=%u\n",
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
                     (unsigned int)session->keyboard.active);
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
    ChoiceFeed choice_feed;
    vn_u32 scene_id;
    vn_u32 rc_u32;
    int i;
    int rc;

    vn_run_config_init(&run_cfg);
    choice_feed.count = 0u;
    choice_feed.cursor = 0u;

    for (i = 1; i < argc; ++i) {
        const char* arg;
        arg = argv[i];

        if (strcmp(arg, "--backend") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --backend\n");
                return 2;
            }
            i += 1;
            run_cfg.backend_name = argv[i];
        } else if (strncmp(arg, "--backend=", 10) == 0) {
            run_cfg.backend_name = arg + 10;
        } else if (strcmp(arg, "--resolution") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --resolution\n");
                return 2;
            }
            i += 1;
            rc = parse_resolution(argv[i], &run_cfg.width, &run_cfg.height);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid resolution: %s\n", argv[i]);
                return 2;
            }
        } else if (strncmp(arg, "--resolution=", 13) == 0) {
            rc = parse_resolution(arg + 13, &run_cfg.width, &run_cfg.height);
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
            run_cfg.scene_name = argv[i];
        } else if (strncmp(arg, "--scene=", 8) == 0) {
            run_cfg.scene_name = arg + 8;
        } else if (strcmp(arg, "--pack") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --pack\n");
                return 2;
            }
            i += 1;
            run_cfg.pack_path = argv[i];
        } else if (strncmp(arg, "--pack=", 7) == 0) {
            run_cfg.pack_path = arg + 7;
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
            run_cfg.choice_index = (vn_u8)(rc_u32 & 0xFFu);
        } else if (strncmp(arg, "--choice-index=", 15) == 0) {
            rc = parse_u32_range(arg + 15, 0l, 255l, &rc_u32);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --choice-index: %s\n", arg + 15);
                return 2;
            }
            run_cfg.choice_index = (vn_u8)(rc_u32 & 0xFFu);
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
            rc = parse_u32_range(argv[i], 1l, 1000000l, &run_cfg.frames);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --frames: %s\n", argv[i]);
                return 2;
            }
        } else if (strncmp(arg, "--frames=", 9) == 0) {
            rc = parse_u32_range(arg + 9, 1l, 1000000l, &run_cfg.frames);
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
            rc = parse_u32_range(argv[i], 0l, 1000l, &run_cfg.dt_ms);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --dt-ms: %s\n", argv[i]);
                return 2;
            }
        } else if (strncmp(arg, "--dt-ms=", 8) == 0) {
            rc = parse_u32_range(arg + 8, 0l, 1000l, &run_cfg.dt_ms);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --dt-ms: %s\n", arg + 8);
                return 2;
            }
        } else if (strcmp(arg, "--keyboard") == 0) {
            run_cfg.keyboard = 1u;
        } else if (strcmp(arg, "--trace") == 0) {
            run_cfg.trace = 1u;
        } else if (strcmp(arg, "--hold-end") == 0) {
            run_cfg.hold_on_end = 1u;
        } else if (strcmp(arg, "--quiet") == 0) {
            run_cfg.emit_logs = 0u;
        }
    }

    run_cfg.choice_seq_count = choice_feed.count;
    if (choice_feed.count > 0u) {
        for (i = 0; i < (int)choice_feed.count; ++i) {
            run_cfg.choice_seq[i] = choice_feed.items[(vn_u32)i];
        }
    }

    rc = parse_scene_id(run_cfg.scene_name, &scene_id);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "invalid scene: %s\n", run_cfg.scene_name);
        return 2;
    }

    rc = vn_runtime_run(&run_cfg, (VNRunResult*)0);
    if (rc != 0) {
        return 1;
    }
    return 0;
}

int vn_runtime_run(const VNRunConfig* run_cfg, VNRunResult* out_result) {
    VNRuntimeSession* session;
    VNRunResult step_result;
    int rc;
    int step_rc;

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
    while (vn_runtime_session_is_done(session) == VN_FALSE) {
        step_rc = vn_runtime_session_step(session, &step_result);
        if (step_rc != VN_OK) {
            rc = step_rc;
            break;
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
