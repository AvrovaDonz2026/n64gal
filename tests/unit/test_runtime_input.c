#include <stdio.h>
#include <string.h>

#include "vn_error.h"
#include "vn_runtime.h"

static int run_until_done(VNRuntimeSession* session, VNRunResult* out_result) {
    vn_u32 guard;
    int rc;

    guard = 0u;
    while (vn_runtime_session_is_done(session) == VN_FALSE) {
        rc = vn_runtime_session_step(session, out_result);
        if (rc != VN_OK) {
            return rc;
        }
        guard += 1u;
        if (guard > 64u) {
            return VN_E_RENDER_STATE;
        }
    }
    return VN_OK;
}

int main(void) {
    VNRunConfig cfg;
    VNRunResult res;
    VNRuntimeSession* session;
    VNInputEvent input;
    int rc;

    session = (VNRuntimeSession*)0;
    (void)memset(&res, 0, sizeof(res));
    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 8u;
    cfg.dt_ms = 16u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "create for choice inject failed rc=%d\n", rc);
        return 1;
    }
    input.kind = VN_INPUT_KIND_CHOICE;
    input.value0 = 1u;
    input.value1 = 0u;
    rc = vn_runtime_session_inject_input(session, &input);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "choice inject failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    rc = run_until_done(session, &res);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "choice run failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (res.choice_selected_index != 1u) {
        (void)fprintf(stderr, "choice inject mismatch got=%u\n", (unsigned int)res.choice_selected_index);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    (void)vn_runtime_session_destroy(session);

    session = (VNRuntimeSession*)0;
    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "create for key inject failed rc=%d\n", rc);
        return 1;
    }
    input.kind = VN_INPUT_KIND_KEY;
    input.value0 = (vn_u32)(unsigned char)'2';
    input.value1 = 0u;
    rc = vn_runtime_session_inject_input(session, &input);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "key inject failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    rc = run_until_done(session, &res);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "key run failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (res.choice_selected_index != 1u) {
        (void)fprintf(stderr, "key inject mismatch got=%u\n", (unsigned int)res.choice_selected_index);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    (void)vn_runtime_session_destroy(session);

    session = (VNRuntimeSession*)0;
    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "create for quit inject failed rc=%d\n", rc);
        return 1;
    }
    input.kind = VN_INPUT_KIND_TRACE_TOGGLE;
    input.value0 = 0u;
    input.value1 = 0u;
    rc = vn_runtime_session_inject_input(session, &input);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "trace toggle inject failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    input.kind = VN_INPUT_KIND_QUIT;
    rc = vn_runtime_session_inject_input(session, &input);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "quit inject failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    rc = vn_runtime_session_step(session, &res);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "quit step failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (res.frames_executed != 0u) {
        (void)fprintf(stderr, "quit should stop before frame, got=%u\n", (unsigned int)res.frames_executed);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (vn_runtime_session_is_done(session) != VN_TRUE) {
        (void)fprintf(stderr, "quit should mark session done\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    (void)vn_runtime_session_destroy(session);

    session = (VNRuntimeSession*)0;
    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "create for invalid key failed rc=%d\n", rc);
        return 1;
    }
    input.kind = VN_INPUT_KIND_KEY;
    input.value0 = (vn_u32)(unsigned char)'x';
    input.value1 = 0u;
    if (vn_runtime_session_inject_input(session, &input) != VN_E_UNSUPPORTED) {
        (void)fprintf(stderr, "expected unsupported for invalid key\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (vn_runtime_session_inject_input((VNRuntimeSession*)0, &input) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected invalid arg for null session\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (vn_runtime_session_inject_input(session, (const VNInputEvent*)0) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected invalid arg for null event\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    input.kind = 99u;
    if (vn_runtime_session_inject_input(session, &input) != VN_E_UNSUPPORTED) {
        (void)fprintf(stderr, "expected unsupported for unknown kind\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    (void)vn_runtime_session_destroy(session);

    (void)printf("test_runtime_input ok\n");
    return 0;
}
