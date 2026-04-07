#include <string.h>
#if defined(_WIN32)
#include <conio.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#endif

#include "runtime_internal.h"

void keyboard_init(KeyboardInput* kb) {
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

int keyboard_enable(KeyboardInput* kb) {
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

void keyboard_disable(KeyboardInput* kb) {
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

void keyboard_poll(KeyboardInput* kb,
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

int runtime_session_inject_key_code(VNRuntimeSession* session, vn_u32 key_code) {
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

void runtime_session_merge_injected_input(VNRuntimeSession* session,
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

void fade_player_init(FadePlayer* fade) {
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

void fade_player_step(FadePlayer* fade, const VNState* vm, vn_u32 dt_ms) {
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
