#include <stdio.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "runtime_internal.h"
#include "platform.h"

static VNRunResult g_last_run_result;

static double runtime_now_ms(void) {
    return vn_platform_now_ms();
}

static double runtime_rss_mb(void) {
#if !defined(_WIN32)
    FILE* fp;
    long rss_pages;
    long page_size;

    fp = fopen("/proc/self/statm", "r");
    if (fp == (FILE*)0) {
        return 0.0;
    }
    rss_pages = 0L;
    if (fscanf(fp, "%*s %ld", &rss_pages) != 1) {
        (void)fclose(fp);
        return 0.0;
    }
    (void)fclose(fp);

    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || rss_pages < 0L) {
        return 0.0;
    }
    return ((double)rss_pages * (double)page_size) / (1024.0 * 1024.0);
#else
    return 0.0;
#endif
}

void runtime_result_reset(void) {
    g_last_run_result.frames_executed = 0u;
    g_last_run_result.text_id = 0u;
    g_last_run_result.vm_waiting = 0u;
    g_last_run_result.vm_ended = 0u;
    g_last_run_result.vm_error = 0u;
    g_last_run_result.fade_alpha = 0u;
    g_last_run_result.fade_remain_ms = 0u;
    g_last_run_result.bgm_id = 0u;
    g_last_run_result.se_id = 0u;
    g_last_run_result.choice_count = 0u;
    g_last_run_result.choice_selected_index = 0u;
    g_last_run_result.choice_text_id = 0u;
    g_last_run_result.op_count = 0u;
    g_last_run_result.backend_name = "none";
    g_last_run_result.perf_flags_effective = 0u;
    g_last_run_result.frame_reuse_hits = 0u;
    g_last_run_result.frame_reuse_misses = 0u;
    g_last_run_result.op_cache_hits = 0u;
    g_last_run_result.op_cache_misses = 0u;
    g_last_run_result.dirty_tile_count = 0u;
    g_last_run_result.dirty_rect_count = 0u;
    g_last_run_result.dirty_full_redraw = 0u;
    g_last_run_result.dirty_tile_frames = 0u;
    g_last_run_result.dirty_tile_total = 0u;
    g_last_run_result.dirty_rect_total = 0u;
    g_last_run_result.dirty_full_redraws = 0u;
    g_last_run_result.render_width = 0u;
    g_last_run_result.render_height = 0u;
    g_last_run_result.dynamic_resolution_tier = 0u;
    g_last_run_result.dynamic_resolution_switches = 0u;
}

void runtime_result_publish(const VNRuntimeSession* session) {
    runtime_result_write(session, &g_last_run_result);
}

int vn_runtime_session_step(VNRuntimeSession* session, VNRunResult* out_result) {
    vn_u8 applied_choice;
    vn_u32 choice_serial_now;
    vn_u32 op_count;
    double t_frame_start;
    double t_after_vm;
    double t_after_build;
    double t_after_raster;
    double frame_ms;
    double vm_ms;
    double build_ms;
    double raster_ms;
    double audio_ms;
    double rss_mb;
    int rc;
    int build_cache_hit;
    int frame_reuse_active;
    int frame_reuse_hit;
    int keyboard_has_choice;
    int keyboard_toggle_trace;
    int keyboard_quit;
    int used_choice_seq;
    int switch_rc;
    RenderOpCacheKey frame_reuse_key;

    if (session == (VNRuntimeSession*)0) {
        return VN_E_INVALID_ARG;
    }

    if (session->done == VN_FALSE && session->frames_executed < session->frames_limit) {
        runtime_dirty_stats_reset(session);
        session->state.frame_index = session->frames_executed;
        state_reset_frame_events(&session->state);

        applied_choice = session->default_choice_index;
        keyboard_has_choice = VN_FALSE;
        keyboard_toggle_trace = VN_FALSE;
        keyboard_quit = VN_FALSE;
        used_choice_seq = VN_FALSE;
        keyboard_poll(&session->keyboard,
                      &applied_choice,
                      &keyboard_has_choice,
                      &keyboard_toggle_trace,
                      &keyboard_quit);
        runtime_session_merge_injected_input(session,
                                             &applied_choice,
                                             &keyboard_has_choice,
                                             &keyboard_toggle_trace,
                                             &keyboard_quit);
        if (keyboard_toggle_trace != VN_FALSE) {
            session->trace = (session->trace == 0u) ? 1u : 0u;
        }
        if (keyboard_quit != VN_FALSE) {
            session->done = VN_TRUE;
        } else {
            if (keyboard_has_choice == VN_FALSE &&
                session->choice_feed.count > 0u &&
                session->choice_feed.cursor < session->choice_feed.count) {
                applied_choice = session->choice_feed.items[session->choice_feed.cursor];
                used_choice_seq = VN_TRUE;
            }
            vm_set_choice_index(&session->vm, applied_choice);

            t_frame_start = runtime_now_ms();
            vm_step(&session->vm, session->dt_ms);
            t_after_vm = runtime_now_ms();
            t_after_build = t_after_vm;
            t_after_raster = t_after_vm;
            state_from_vm(&session->state, &session->vm);
            fade_player_step(&session->fade_player, &session->vm, session->dt_ms);
            state_apply_fade(&session->state, &session->fade_player);

            build_cache_hit = VN_FALSE;
            frame_reuse_hit = VN_FALSE;
            frame_reuse_active = runtime_prepare_frame_reuse(session,
                                                             &session->state,
                                                             &frame_reuse_key,
                                                             &frame_reuse_hit);
            if (frame_reuse_hit != VN_FALSE) {
                op_count = session->last_op_count;
                rc = VN_OK;
                t_after_build = runtime_now_ms();
                t_after_raster = t_after_build;
            } else {
                op_count = 16u;
                rc = runtime_build_render_ops_cached(session,
                                                     &session->state,
                                                     session->ops,
                                                     &op_count,
                                                     &build_cache_hit);
                t_after_build = runtime_now_ms();
                if (rc != VN_OK) {
                    (void)fprintf(stderr, "build_render_ops failed rc=%d frame=%u\n",
                                  rc,
                                  (unsigned int)session->state.frame_index);
                    session->exit_code = 1;
                    session->done = VN_TRUE;
                } else {
                    runtime_prepare_dirty_plan(session, session->ops, op_count);
                    runtime_submit_render_ops(session, session->ops, op_count);
                    t_after_raster = runtime_now_ms();
                    runtime_commit_dirty_plan(session, session->ops, op_count);
                    if (frame_reuse_active != VN_FALSE) {
                        runtime_commit_frame_reuse(session, &frame_reuse_key);
                    }
                }
            }
            if (rc == VN_OK) {
                choice_serial_now = vm_choice_serial(&session->vm);
                if (choice_serial_now != session->last_choice_serial) {
                    session->last_choice_serial = choice_serial_now;
                    if (used_choice_seq != VN_FALSE && session->choice_feed.cursor < session->choice_feed.count) {
                        session->choice_feed.cursor += 1u;
                    }
                }

                session->frames_executed += 1u;
                session->last_op_count = op_count;

                vm_ms = t_after_vm - t_frame_start;
                build_ms = t_after_build - t_after_vm;
                raster_ms = t_after_raster - t_after_build;
                frame_ms = t_after_raster - t_frame_start;
                audio_ms = 0.0;
                rss_mb = runtime_rss_mb();

                if (session->trace != 0u && session->emit_logs != 0u) {
                    (void)printf("frame=%u frame_ms=%.3f vm_ms=%.3f build_ms=%.3f raster_ms=%.3f audio_ms=%.3f rss_mb=%.3f "
                                 "text=%u wait=%u end=%u fade=%u fade_remain=%u bgm=%u se=%u choice_count=%u choice_sel=%u choice_text=%u ",
                                 (unsigned int)session->state.frame_index,
                                 frame_ms,
                                 vm_ms,
                                 build_ms,
                                 raster_ms,
                                 audio_ms,
                                 rss_mb,
                                 (unsigned int)session->state.text_id,
                                 (unsigned int)session->state.vm_waiting,
                                 (unsigned int)session->state.vm_ended,
                                 (unsigned int)session->state.fade_alpha,
                                 (unsigned int)session->state.fade_duration_ms,
                                 (unsigned int)session->state.bgm_id,
                                 (unsigned int)session->state.se_id,
                                 (unsigned int)session->state.choice_count,
                                 (unsigned int)session->state.choice_selected_index,
                                 (unsigned int)session->state.choice_text_id);
                    (void)printf("ops=%u frame_reuse_hit=%u frame_reuse_hits=%u frame_reuse_misses=%u op_cache_hit=%u op_cache_hits=%u op_cache_misses=%u ",
                                 (unsigned int)op_count,
                                 (unsigned int)(frame_reuse_hit != VN_FALSE),
                                 (unsigned int)session->frame_reuse_hits,
                                 (unsigned int)session->frame_reuse_misses,
                                 (unsigned int)(build_cache_hit != VN_FALSE),
                                 (unsigned int)session->op_cache_hits,
                                 (unsigned int)session->op_cache_misses);
                    (void)printf("dirty_tiles=%u dirty_rects=%u dirty_full_redraw=%u dirty_tile_frames=%u dirty_tile_total=%u dirty_rect_total=%u dirty_full_redraws=%u "
                                 "render_width=%u render_height=%u dynres_tier=%s dynres_switches=%u\n",
                                 (unsigned int)session->dirty_tile_count,
                                 (unsigned int)session->dirty_rect_count,
                                 (unsigned int)session->dirty_full_redraw,
                                 (unsigned int)session->dirty_tile_frames,
                                 (unsigned int)session->dirty_tile_total,
                                 (unsigned int)session->dirty_rect_total,
                                 (unsigned int)session->dirty_full_redraws,
                                 (unsigned int)session->renderer_cfg.width,
                                 (unsigned int)session->renderer_cfg.height,
                                 vn_dynres_tier_name(vn_dynres_get_current_tier(&session->dynamic_resolution)),
                                 (unsigned int)vn_dynres_get_switch_count(&session->dynamic_resolution));
                }

                switch_rc = runtime_dynamic_resolution_maybe_switch(session, frame_ms);
                if (switch_rc != VN_OK) {
                    session->exit_code = 1;
                    session->done = VN_TRUE;
                }

                if (session->state.vm_error != 0u) {
                    session->exit_code = 1;
                    session->done = VN_TRUE;
                } else if (session->state.vm_ended != 0u && session->hold_on_end == 0u) {
                    session->done = VN_TRUE;
                }
                if (session->frames_executed >= session->frames_limit) {
                    session->done = VN_TRUE;
                }
            }
        }
    } else {
        session->done = VN_TRUE;
    }

    if (session->done != VN_FALSE &&
        session->summary_emitted == VN_FALSE &&
        session->exit_code == 0 &&
        session->emit_logs != 0u) {
        (void)printf("vn_runtime ok trace_id=runtime.run.ok backend=%s resolution=%ux%u scene=%s frames=%u dt=%u resources=%u text=%u wait=%u end=%u "
                     "fade=%u fade_remain=%u bgm=%u se=%u choice=%u choice_sel=%u choice_text=%u err=%u ops=%u keyboard=%u perf_flags=0x%X ",
                     renderer_backend_name(),
                     (unsigned int)session->renderer_cfg.width,
                     (unsigned int)session->renderer_cfg.height,
                     scene_name_from_id(session->state.scene_id),
                     (unsigned int)session->frames_executed,
                     (unsigned int)session->dt_ms,
                     (unsigned int)session->state.resource_count,
                     (unsigned int)session->state.text_id,
                     (unsigned int)session->state.vm_waiting,
                     (unsigned int)session->state.vm_ended,
                     (unsigned int)session->state.fade_alpha,
                     (unsigned int)session->state.fade_duration_ms,
                     (unsigned int)session->state.bgm_id,
                     (unsigned int)session->state.se_id,
                     (unsigned int)session->state.choice_count,
                     (unsigned int)session->state.choice_selected_index,
                     (unsigned int)session->state.choice_text_id,
                     (unsigned int)session->state.vm_error,
                     (unsigned int)session->last_op_count,
                     (unsigned int)session->keyboard.active,
                     (unsigned int)session->perf_flags);
        (void)printf("frame_reuse_hits=%u frame_reuse_misses=%u op_cache_hits=%u op_cache_misses=%u dirty_tiles=%u dirty_rects=%u dirty_full_redraw=%u "
                     "dirty_tile_frames=%u dirty_tile_total=%u dirty_rect_total=%u dirty_full_redraws=%u ",
                     (unsigned int)session->frame_reuse_hits,
                     (unsigned int)session->frame_reuse_misses,
                     (unsigned int)session->op_cache_hits,
                     (unsigned int)session->op_cache_misses,
                     (unsigned int)session->dirty_tile_count,
                     (unsigned int)session->dirty_rect_count,
                     (unsigned int)session->dirty_full_redraw,
                     (unsigned int)session->dirty_tile_frames,
                     (unsigned int)session->dirty_tile_total,
                     (unsigned int)session->dirty_rect_total,
                     (unsigned int)session->dirty_full_redraws);
        (void)printf("dynres_tier=%s dynres_switches=%u\n",
                     vn_dynres_tier_name(vn_dynres_get_current_tier(&session->dynamic_resolution)),
                     (unsigned int)vn_dynres_get_switch_count(&session->dynamic_resolution));
        session->summary_emitted = VN_TRUE;
    }

    runtime_result_publish(session);
    if (out_result != (VNRunResult*)0) {
        *out_result = g_last_run_result;
    }
    if (session->done != VN_FALSE && session->exit_code != 0) {
        return session->exit_code;
    }
    return VN_OK;
}

int vn_runtime_run(const VNRunConfig* run_cfg, VNRunResult* out_result) {
    VNRuntimeSession* session;
    VNRunResult step_result;
    int rc;
    int step_rc;
    int sleep_between_frames;

    runtime_result_reset();
    rc = vn_runtime_session_create(run_cfg, &session);
    if (rc != VN_OK) {
        if (out_result != (VNRunResult*)0) {
            *out_result = g_last_run_result;
        }
        return rc;
    }

    step_result = g_last_run_result;
    rc = VN_OK;
    sleep_between_frames = VN_FALSE;
    if (run_cfg != (const VNRunConfig*)0 &&
        run_cfg->keyboard != 0u &&
        run_cfg->dt_ms > 0u) {
        sleep_between_frames = VN_TRUE;
    }
    while (vn_runtime_session_is_done(session) == VN_FALSE) {
        step_rc = vn_runtime_session_step(session, &step_result);
        if (step_rc != VN_OK) {
            rc = step_rc;
            break;
        }
        if (sleep_between_frames != VN_FALSE &&
            vn_runtime_session_is_done(session) == VN_FALSE) {
            vn_platform_sleep_ms((unsigned int)run_cfg->dt_ms);
        }
    }

    if (rc == VN_OK && session->exit_code != 0) {
        rc = session->exit_code;
    }
    if (out_result != (VNRunResult*)0) {
        *out_result = g_last_run_result;
    }
    (void)vn_runtime_session_destroy(session);
    return rc;
}
