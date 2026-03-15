#include <stdio.h>
#include <string.h>

#if defined(_WIN32)

int main(void) {
    (void)printf("host_embed_template_linux_tty skipped on windows\n");
    return 0;
}

#else

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include "vn_error.h"
#include "vn_runtime.h"

typedef struct {
    int active;
    struct termios old_termios;
    int old_flags;
} LinuxTTYInput;

static int linux_tty_begin(LinuxTTYInput* input) {
    struct termios raw;
    int flags;

    if (input == (LinuxTTYInput*)0) {
        return VN_E_INVALID_ARG;
    }
    input->active = 0;
    if (!isatty(STDIN_FILENO)) {
        return VN_OK;
    }
    if (tcgetattr(STDIN_FILENO, &input->old_termios) != 0) {
        return VN_E_IO;
    }
    raw = input->old_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return VN_E_IO;
    }
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags < 0) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &input->old_termios);
        return VN_E_IO;
    }
    input->old_flags = flags;
    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) != 0) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &input->old_termios);
        return VN_E_IO;
    }
    input->active = 1;
    return VN_OK;
}

static void linux_tty_end(LinuxTTYInput* input) {
    if (input == (LinuxTTYInput*)0 || input->active == 0) {
        return;
    }
    (void)fcntl(STDIN_FILENO, F_SETFL, input->old_flags);
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &input->old_termios);
    input->active = 0;
}

static int linux_tty_maybe_inject(VNRuntimeSession* session) {
    char ch;
    VNInputEvent event;
    int got;

    got = (int)read(STDIN_FILENO, &ch, 1u);
    if (got <= 0) {
        return VN_OK;
    }
    event.kind = 0u;
    event.value0 = 0u;
    event.value1 = 0u;
    if (ch >= '1' && ch <= '9') {
        event.kind = VN_INPUT_KIND_CHOICE;
        event.value0 = (vn_u32)(unsigned char)(ch - '1');
    } else if (ch == 't' || ch == 'T') {
        event.kind = VN_INPUT_KIND_TRACE_TOGGLE;
    } else if (ch == 'q' || ch == 'Q') {
        event.kind = VN_INPUT_KIND_QUIT;
    } else {
        return VN_OK;
    }
    return vn_runtime_session_inject_input(session, &event);
}

int main(void) {
    VNRunConfig cfg;
    VNRunResult res;
    VNRuntimeSession* session;
    LinuxTTYInput input;
    int rc;

    (void)memset(&res, 0, sizeof(res));
    (void)memset(&input, 0, sizeof(input));
    session = (VNRuntimeSession*)0;
    vn_run_config_init(&cfg);
    cfg.pack_path = "templates/minimal-vn/build/minimal.vnpak";
    cfg.scene_name = "S0";
    cfg.backend_name = "auto";
    cfg.frames = 12u;
    cfg.dt_ms = 16u;
    cfg.emit_logs = 0u;

    rc = linux_tty_begin(&input);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "linux_tty_begin failed rc=%d\n", rc);
        return 1;
    }
    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK) {
        linux_tty_end(&input);
        (void)fprintf(stderr, "session create failed rc=%d\n", rc);
        return 1;
    }
    while (vn_runtime_session_is_done(session) == VN_FALSE) {
        rc = linux_tty_maybe_inject(session);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "linux_tty inject failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            linux_tty_end(&input);
            return 1;
        }
        rc = vn_runtime_session_step(session, &res);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "session step failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            linux_tty_end(&input);
            return 1;
        }
    }
    (void)printf("host_embed_template_linux_tty ok backend=%s frames=%u text=%u\n",
                 (res.backend_name != (const char*)0) ? res.backend_name : "unknown",
                 (unsigned int)res.frames_executed,
                 (unsigned int)res.text_id);
    (void)vn_runtime_session_destroy(session);
    linux_tty_end(&input);
    return 0;
}

#endif
