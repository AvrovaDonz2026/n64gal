#include <stdio.h>
#include <string.h>

#include "vn_error.h"
#include "vn_runtime.h"

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
        rc = vn_runtime_session_step(session, &res);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "session step failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
    }

    (void)printf("host_embed_template ok backend=%s frames=%u text=%u\n",
                 (res.backend_name != (const char*)0) ? res.backend_name : "unknown",
                 (unsigned int)res.frames_executed,
                 (unsigned int)res.text_id);
    (void)vn_runtime_session_destroy(session);
    return 0;
}
