#include <stdio.h>

#include "vn_error.h"
#include "vn_runtime.h"

int main(void) {
    VNRunConfig cfg;
    VNRunResult res;
    VNRuntimeSession* session;
    int rc;
    vn_u32 frames_seen;

    session = (VNRuntimeSession*)0;
    frames_seen = 0u;
    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.backend_name = "auto";
    cfg.width = 600u;
    cfg.height = 800u;
    cfg.frames = 12u;
    cfg.dt_ms = 16u;
    cfg.emit_logs = 0u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.hold_on_end = 0u;
    cfg.choice_index = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vn_runtime_session_create failed rc=%d\n", rc);
        return 1;
    }

    rc = vn_runtime_session_set_choice(session, 1u);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vn_runtime_session_set_choice failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }

    while (vn_runtime_session_is_done(session) == VN_FALSE) {
        rc = vn_runtime_session_step(session, &res);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "vn_runtime_session_step failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
        frames_seen = res.frames_executed;
    }

    (void)printf("host_embed ok backend=%s frames=%u text=%u wait=%u end=%u choice=%u ops=%u\n",
                 res.backend_name,
                 (unsigned int)frames_seen,
                 (unsigned int)res.text_id,
                 (unsigned int)res.vm_waiting,
                 (unsigned int)res.vm_ended,
                 (unsigned int)res.choice_selected_index,
                 (unsigned int)res.op_count);

    rc = vn_runtime_session_destroy(session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vn_runtime_session_destroy failed rc=%d\n", rc);
        return 1;
    }

    return 0;
}
