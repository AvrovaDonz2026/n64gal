#include <stdio.h>
#include <string.h>

#include "../../src/core/dynamic_resolution.h"
#include "vn_runtime.h"

int main(void) {
    VNRunConfig cfg;
    VNRunResult res;
    int rc;

    vn_run_config_init(&cfg);
    cfg.backend_name = "scalar";
    cfg.scene_name = "S3";
    cfg.width = 1200u;
    cfg.height = 1600u;
    cfg.frames = 128u;
    cfg.dt_ms = 16u;
    cfg.emit_logs = 0u;
    cfg.hold_on_end = 1u;
    cfg.perf_flags = VN_RUNTIME_PERF_DYNAMIC_RESOLUTION;

    vn_dynres_set_test_overrides(8u, VN_DYNRES_UP_WINDOW, 0.01, 0.0);
    rc = vn_runtime_run(&cfg, &res);
    vn_dynres_reset_test_overrides();
    if (rc != 0) {
        (void)fprintf(stderr, "vn_runtime_run failed rc=%d\n", rc);
        return 1;
    }
    if ((res.perf_flags_effective & VN_RUNTIME_PERF_DYNAMIC_RESOLUTION) == 0u) {
        (void)fprintf(stderr, "dynamic resolution perf flag missing\n");
        return 1;
    }
    if (strcmp(res.backend_name, "scalar") != 0) {
        (void)fprintf(stderr, "expected scalar backend got=%s\n", res.backend_name);
        return 1;
    }
    if (res.dynamic_resolution_switches == 0u) {
        (void)fprintf(stderr, "expected at least one dynamic resolution switch\n");
        return 1;
    }
    if (res.dynamic_resolution_tier == 0u) {
        (void)fprintf(stderr, "expected tier to move below R0\n");
        return 1;
    }
    if (res.render_width >= cfg.width || res.render_height >= cfg.height) {
        (void)fprintf(stderr,
                      "expected reduced render size got=%ux%u base=%ux%u\n",
                      (unsigned int)res.render_width,
                      (unsigned int)res.render_height,
                      (unsigned int)cfg.width,
                      (unsigned int)cfg.height);
        return 1;
    }
    if (res.frames_executed != cfg.frames) {
        (void)fprintf(stderr,
                      "expected frames=%u got=%u\n",
                      (unsigned int)cfg.frames,
                      (unsigned int)res.frames_executed);
        return 1;
    }

    (void)printf("test_runtime_dynamic_resolution ok tier=%u resolution=%ux%u switches=%u\n",
                 (unsigned int)res.dynamic_resolution_tier,
                 (unsigned int)res.render_width,
                 (unsigned int)res.render_height,
                 (unsigned int)res.dynamic_resolution_switches);
    return 0;
}
