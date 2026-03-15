#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)

int main(void) {
    (void)printf("host_embed_template_windows_console skipped on non-windows\n");
    return 0;
}

#else

#include <conio.h>
#include <windows.h>

#include "vn_error.h"
#include "vn_runtime.h"

static int windows_console_maybe_inject(VNRuntimeSession* session) {
    VNInputEvent event;
    int ch;

    if (_kbhit() == 0) {
        return VN_OK;
    }
    ch = _getch();
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
    int rc;

    (void)memset(&res, 0, sizeof(res));
    session = (VNRuntimeSession*)0;
    vn_run_config_init(&cfg);
    cfg.pack_path = "templates/minimal-vn/build/minimal.vnpak";
    cfg.scene_name = "S0";
    cfg.backend_name = "auto";
    cfg.frames = 12u;
    cfg.dt_ms = 16u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "session create failed rc=%d\n", rc);
        return 1;
    }
    while (vn_runtime_session_is_done(session) == VN_FALSE) {
        rc = windows_console_maybe_inject(session);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "windows_console inject failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
        rc = vn_runtime_session_step(session, &res);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "session step failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
        if (cfg.dt_ms > 0u) {
            Sleep((DWORD)cfg.dt_ms);
        }
    }
    (void)printf("host_embed_template_windows_console ok backend=%s frames=%u text=%u\n",
                 (res.backend_name != (const char*)0) ? res.backend_name : "unknown",
                 (unsigned int)res.frames_executed,
                 (unsigned int)res.text_id);
    (void)vn_runtime_session_destroy(session);
    return 0;
}

#endif
