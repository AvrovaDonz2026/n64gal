#include <stdio.h>
#include <string.h>

#include "vn_error.h"
#include "vn_runtime.h"

int main(void) {
    VNRunConfig cfg;
    VNRunResult res;
    VNRuntimeSession* session;
    int rc;
    vn_u32 guard;

    session = (VNRuntimeSession*)0;
    memset((void*)&res, 0, sizeof(res));
    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 8u;
    cfg.dt_ms = 16u;
    cfg.choice_index = 0u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "vn_runtime_session_create failed rc=%d\n", rc);
        return 1;
    }

    rc = vn_runtime_session_set_choice(session, 1u);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vn_runtime_session_set_choice failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }

    guard = 0u;
    while (vn_runtime_session_is_done(session) == VN_FALSE) {
        rc = vn_runtime_session_step(session, &res);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "vn_runtime_session_step failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
        guard += 1u;
        if (guard > 64u) {
            (void)fprintf(stderr, "runtime session did not finish\n");
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
    }

    if (res.frames_executed == 0u) {
        (void)fprintf(stderr, "no frames executed\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (res.choice_selected_index != 1u) {
        (void)fprintf(stderr, "choice index mismatch got=%u\n", (unsigned int)res.choice_selected_index);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (res.backend_name == (const char*)0) {
        (void)fprintf(stderr, "backend name missing\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }

    rc = vn_runtime_session_destroy(session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vn_runtime_session_destroy failed rc=%d\n", rc);
        return 1;
    }

    rc = vn_runtime_session_create((const VNRunConfig*)0, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "default create failed rc=%d\n", rc);
        return 1;
    }
    rc = vn_runtime_session_destroy(session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "default destroy failed rc=%d\n", rc);
        return 1;
    }

    if (vn_runtime_session_create(&cfg, (VNRuntimeSession**)0) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null out_session\n");
        return 1;
    }
    if (vn_runtime_session_step((VNRuntimeSession*)0, &res) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null session step\n");
        return 1;
    }
    if (vn_runtime_session_set_choice((VNRuntimeSession*)0, 0u) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null session set_choice\n");
        return 1;
    }
    if (vn_runtime_session_is_done((const VNRuntimeSession*)0) != VN_TRUE) {
        (void)fprintf(stderr, "expected null session as done\n");
        return 1;
    }
    if (vn_runtime_session_destroy((VNRuntimeSession*)0) != VN_OK) {
        (void)fprintf(stderr, "null destroy should be ok\n");
        return 1;
    }

    (void)printf("test_runtime_session ok\n");
    return 0;
}
