#include <stdio.h>

#include "vn_runtime.h"

static int run_perf_scene(const char* scene_name,
                          vn_u32 frames,
                          vn_u32 hold_on_end,
                          vn_u32 perf_flags,
                          VNRunResult* out_result) {
    VNRunConfig cfg;
    int rc;

    vn_run_config_init(&cfg);
    cfg.scene_name = scene_name;
    cfg.frames = frames;
    cfg.dt_ms = 16u;
    cfg.hold_on_end = hold_on_end;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;
    cfg.perf_flags = perf_flags;

    rc = vn_runtime_run(&cfg, out_result);
    if (rc != 0) {
        (void)fprintf(stderr,
                      "perf scene run failed rc=%d scene=%s perf_flags=0x%X\n",
                      rc,
                      scene_name,
                      (unsigned int)perf_flags);
        return 1;
    }
    if (out_result->frames_executed == 0u) {
        (void)fprintf(stderr, "perf scene executed no frames scene=%s\n", scene_name);
        return 1;
    }
    return 0;
}

int main(void) {
    VNRunConfig cfg;
    VNRunResult res;
    VNRunResult reuse_res;
    VNRunResult cache_res;
    VNRunResult dirty_res;
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
    if ((res.perf_flags_effective & VN_RUNTIME_PERF_FRAME_REUSE) == 0u) {
        (void)fprintf(stderr, "expected default frame reuse perf flag\n");
        return 1;
    }
    if ((res.perf_flags_effective & VN_RUNTIME_PERF_DIRTY_TILE) != 0u) {
        (void)fprintf(stderr, "dirty tile perf flag should be off by default\n");
        return 1;
    }

    if (run_perf_scene("S0", 64u, 1u, VN_RUNTIME_PERF_FRAME_REUSE, &reuse_res) != 0) {
        return 1;
    }
    if (reuse_res.frame_reuse_hits == 0u) {
        (void)fprintf(stderr, "expected frame reuse hits, got 0\n");
        return 1;
    }
    if (reuse_res.frame_reuse_misses == 0u) {
        (void)fprintf(stderr, "expected frame reuse misses, got 0\n");
        return 1;
    }
    if (reuse_res.op_cache_hits != 0u || reuse_res.op_cache_misses != 0u ||
        reuse_res.dirty_tile_frames != 0u || reuse_res.dirty_tile_total != 0u) {
        (void)fprintf(stderr,
                      "expected isolated frame reuse stats cache=%u/%u dirty=%u/%u\n",
                      (unsigned int)reuse_res.op_cache_hits,
                      (unsigned int)reuse_res.op_cache_misses,
                      (unsigned int)reuse_res.dirty_tile_frames,
                      (unsigned int)reuse_res.dirty_tile_total);
        return 1;
    }

    if (run_perf_scene("S0", 64u, 1u, VN_RUNTIME_PERF_OP_CACHE, &cache_res) != 0) {
        return 1;
    }
    if (cache_res.op_cache_hits == 0u) {
        (void)fprintf(stderr, "expected op cache hits, got 0\n");
        return 1;
    }
    if (cache_res.op_cache_misses == 0u) {
        (void)fprintf(stderr, "expected op cache misses, got 0\n");
        return 1;
    }
    if (cache_res.frame_reuse_hits != 0u || cache_res.frame_reuse_misses != 0u ||
        cache_res.dirty_tile_frames != 0u || cache_res.dirty_tile_total != 0u) {
        (void)fprintf(stderr,
                      "expected isolated op cache stats reuse=%u/%u dirty=%u/%u\n",
                      (unsigned int)cache_res.frame_reuse_hits,
                      (unsigned int)cache_res.frame_reuse_misses,
                      (unsigned int)cache_res.dirty_tile_frames,
                      (unsigned int)cache_res.dirty_tile_total);
        return 1;
    }

    if (run_perf_scene("S0", 64u, 1u, VN_RUNTIME_PERF_DIRTY_TILE, &dirty_res) != 0) {
        return 1;
    }
    if ((dirty_res.perf_flags_effective & VN_RUNTIME_PERF_DIRTY_TILE) == 0u) {
        (void)fprintf(stderr, "expected dirty tile perf flag\n");
        return 1;
    }
    if (dirty_res.dirty_tile_frames == 0u) {
        (void)fprintf(stderr, "expected dirty tile frames, got 0\n");
        return 1;
    }
    if (dirty_res.dirty_tile_total == 0u || dirty_res.dirty_rect_total == 0u) {
        (void)fprintf(stderr,
                      "expected dirty tile totals, got tiles=%u rects=%u\n",
                      (unsigned int)dirty_res.dirty_tile_total,
                      (unsigned int)dirty_res.dirty_rect_total);
        return 1;
    }
    if (dirty_res.dirty_full_redraws == 0u) {
        (void)fprintf(stderr, "expected at least one dirty full redraw\n");
        return 1;
    }
    if (dirty_res.frame_reuse_hits != 0u || dirty_res.frame_reuse_misses != 0u ||
        dirty_res.op_cache_hits != 0u || dirty_res.op_cache_misses != 0u) {
        (void)fprintf(stderr,
                      "expected isolated dirty tile stats reuse=%u/%u cache=%u/%u\n",
                      (unsigned int)dirty_res.frame_reuse_hits,
                      (unsigned int)dirty_res.frame_reuse_misses,
                      (unsigned int)dirty_res.op_cache_hits,
                      (unsigned int)dirty_res.op_cache_misses);
        return 1;
    }

    if (run_perf_scene("S0", 64u, 1u, 0u, &res) != 0) {
        return 1;
    }
    if (res.perf_flags_effective != 0u) {
        (void)fprintf(stderr,
                      "expected perf flags disabled, got=0x%X\n",
                      (unsigned int)res.perf_flags_effective);
        return 1;
    }
    if (res.frame_reuse_hits != 0u || res.frame_reuse_misses != 0u ||
        res.op_cache_hits != 0u || res.op_cache_misses != 0u ||
        res.dirty_tile_frames != 0u || res.dirty_tile_total != 0u ||
        res.dirty_rect_total != 0u || res.dirty_full_redraws != 0u) {
        (void)fprintf(stderr,
                      "expected zero perf stats when disabled reuse=%u/%u cache=%u/%u dirty=%u/%u/%u/%u\n",
                      (unsigned int)res.frame_reuse_hits,
                      (unsigned int)res.frame_reuse_misses,
                      (unsigned int)res.op_cache_hits,
                      (unsigned int)res.op_cache_misses,
                      (unsigned int)res.dirty_tile_frames,
                      (unsigned int)res.dirty_tile_total,
                      (unsigned int)res.dirty_rect_total,
                      (unsigned int)res.dirty_full_redraws);
        return 1;
    }

    (void)printf("test_runtime_api ok backend=%s reuse_hits=%u reuse_misses=%u cache_hits=%u cache_misses=%u dirty_frames=%u dirty_tiles=%u dirty_rects=%u\n",
                 reuse_res.backend_name,
                 (unsigned int)reuse_res.frame_reuse_hits,
                 (unsigned int)reuse_res.frame_reuse_misses,
                 (unsigned int)cache_res.op_cache_hits,
                 (unsigned int)cache_res.op_cache_misses,
                 (unsigned int)dirty_res.dirty_tile_frames,
                 (unsigned int)dirty_res.dirty_tile_total,
                 (unsigned int)dirty_res.dirty_rect_total);
    return 0;
}
