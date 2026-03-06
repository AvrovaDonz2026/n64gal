#include <stdio.h>

#include "vn_runtime.h"

static int run_cached_scene(vn_u32 perf_flags, VNRunResult* out_result) {
    VNRunConfig cfg;
    int rc;

    vn_run_config_init(&cfg);
    cfg.scene_name = "S0";
    cfg.frames = 64u;
    cfg.dt_ms = 16u;
    cfg.hold_on_end = 1u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;
    cfg.perf_flags = perf_flags;

    rc = vn_runtime_run(&cfg, out_result);
    if (rc != 0) {
        (void)fprintf(stderr,
                      "cached scene run failed rc=%d perf_flags=0x%X\n",
                      rc,
                      (unsigned int)perf_flags);
        return 1;
    }
    if (out_result->frames_executed == 0u) {
        (void)fprintf(stderr, "cached scene executed no frames\n");
        return 1;
    }
    return 0;
}

int main(void) {
    VNRunConfig cfg;
    VNRunResult res;
    VNRunResult cached_res;
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
        (void)fprintf(stderr,
                      "choice index mismatch got=%u\n",
                      (unsigned int)res.choice_selected_index);
        return 1;
    }
    if (res.backend_name == (const char*)0) {
        (void)fprintf(stderr, "backend_name missing\n");
        return 1;
    }
    if ((res.perf_flags_effective & VN_RUNTIME_PERF_OP_CACHE) == 0u) {
        (void)fprintf(stderr, "expected default op cache perf flag\n");
        return 1;
    }

    if (run_cached_scene(VN_RUNTIME_PERF_OP_CACHE, &cached_res) != 0) {
        return 1;
    }
    if (cached_res.op_cache_hits == 0u) {
        (void)fprintf(stderr, "expected op cache hits, got 0\n");
        return 1;
    }
    if (cached_res.op_cache_misses == 0u) {
        (void)fprintf(stderr, "expected op cache misses, got 0\n");
        return 1;
    }

    if (run_cached_scene(0u, &res) != 0) {
        return 1;
    }
    if (res.perf_flags_effective != 0u) {
        (void)fprintf(stderr,
                      "expected perf flags disabled, got=0x%X\n",
                      (unsigned int)res.perf_flags_effective);
        return 1;
    }
    if (res.op_cache_hits != 0u || res.op_cache_misses != 0u) {
        (void)fprintf(stderr,
                      "expected zero cache stats when disabled hits=%u misses=%u\n",
                      (unsigned int)res.op_cache_hits,
                      (unsigned int)res.op_cache_misses);
        return 1;
    }

    (void)printf("test_runtime_api ok backend=%s cache_hits=%u cache_misses=%u\n",
                 cached_res.backend_name,
                 (unsigned int)cached_res.op_cache_hits,
                 (unsigned int)cached_res.op_cache_misses);
    return 0;
}
