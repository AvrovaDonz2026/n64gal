#include <stdio.h>

#include "vn_runtime.h"

int main(void) {
    VNRunConfig cfg;
    VNRunResult res;
    int rc;

    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 8u;
    cfg.dt_ms = 16u;
    cfg.choice_index = 1u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_run(&cfg, &res);
    if (rc != 0) {
        (void)fprintf(stderr, "vn_runtime_run failed rc=%d\n", rc);
        return 1;
    }
    if (res.frames_executed == 0u) {
        (void)fprintf(stderr, "no frames executed\n");
        return 1;
    }
    if (res.choice_selected_index != 1u) {
        (void)fprintf(stderr, "choice index mismatch got=%u\n", (unsigned int)res.choice_selected_index);
        return 1;
    }
    if (res.backend_name == (const char*)0) {
        (void)fprintf(stderr, "backend_name missing\n");
        return 1;
    }

    (void)printf("test_runtime_api ok backend=%s\n", res.backend_name);
    return 0;
}
