#include <stdio.h>
#include <string.h>

#include "preview_internal.h"
#include "../core/platform.h"

void preview_report_init(VNPreviewReport* report) {
    if (report == (VNPreviewReport*)0) {
        return;
    }
    (void)memset(report, 0, sizeof(VNPreviewReport));
    report->status_code = 0;
    report->error_code = VN_OK;
    report->error_name = vn_error_name(VN_OK);
    report->trace_id = "preview.ok";
}

int preview_write_response(const VNPreviewRequest* req,
                           const VNPreviewReport* report) {
    FILE* fp;
    vn_u32 i;

    if (req == (const VNPreviewRequest*)0 || report == (const VNPreviewReport*)0) {
        return 1;
    }

    if (req->response_path[0] != '\0') {
        fp = fopen(req->response_path, "w");
        if (fp == (FILE*)0) {
            (void)fprintf(stderr, "failed to open preview response: %s\n", req->response_path);
            return 1;
        }
    } else {
        fp = stdout;
    }

    (void)fprintf(fp, "{\n");
    (void)fprintf(fp, "  \"preview_protocol\":\"v1\",\n");
    (void)fprintf(fp, "  \"status\":\"%s\",\n", (report->status_code == 0) ? "ok" : "error");
    (void)fprintf(fp, "  \"error_code\":%d,\n", report->error_code);
    (void)fprintf(fp, "  \"error_name\":");
    preview_json_write_string(fp, report->error_name);
    (void)fprintf(fp, ",\n  \"trace_id\":");
    preview_json_write_string(fp, report->trace_id);
    (void)fprintf(fp, ",\n  \"error_message\":");
    preview_json_write_string(fp, report->error_message);
    (void)fprintf(fp, ",\n  \"host_os\":");
    preview_json_write_string(fp, vn_platform_host_os_name());
    (void)fprintf(fp, ",\n  \"host_arch\":");
    preview_json_write_string(fp, vn_platform_host_arch_name());
    (void)fprintf(fp, ",\n  \"request\":{\n");
    (void)fprintf(fp, "    \"project_dir\":");
    preview_json_write_string(fp, req->project_dir);
    (void)fprintf(fp, ",\n    \"pack_path\":");
    preview_json_write_string(fp, req->cfg.pack_path);
    (void)fprintf(fp, ",\n    \"scene_name\":");
    preview_json_write_string(fp, req->cfg.scene_name);
    (void)fprintf(fp, ",\n    \"backend_name\":");
    preview_json_write_string(fp, req->cfg.backend_name);
    (void)fprintf(fp, ",\n    \"width\":%u,\n", (unsigned int)req->cfg.width);
    (void)fprintf(fp, "    \"height\":%u,\n", (unsigned int)req->cfg.height);
    (void)fprintf(fp, "    \"frames\":%u,\n", (unsigned int)req->cfg.frames);
    (void)fprintf(fp, "    \"dt_ms\":%u,\n", (unsigned int)req->cfg.dt_ms);
    (void)fprintf(fp, "    \"trace\":%u,\n", (unsigned int)req->trace_enabled);
    (void)fprintf(fp, "    \"hold_on_end\":%u,\n", (unsigned int)req->cfg.hold_on_end);
    (void)fprintf(fp, "    \"choice_index\":%u,\n", (unsigned int)req->cfg.choice_index);
    (void)fprintf(fp, "    \"choice_seq_count\":%u,\n", (unsigned int)req->cfg.choice_seq_count);
    (void)fprintf(fp, "    \"command_count\":%u\n", (unsigned int)req->command_count);
    (void)fprintf(fp, "  },\n");
    (void)fprintf(fp, "  \"summary\":{\n");
    (void)fprintf(fp, "    \"reload_count\":%u,\n", (unsigned int)report->reload_count);
    (void)fprintf(fp, "    \"frame_samples\":%u,\n", (unsigned int)report->frame_samples);
    (void)fprintf(fp, "    \"session_done\":%u,\n", (unsigned int)report->session_done);
    (void)fprintf(fp, "    \"events_truncated\":%u\n", (unsigned int)report->events_truncated);
    (void)fprintf(fp, "  },\n");
    (void)fprintf(fp, "  \"perf_summary\":{\n");
    (void)fprintf(fp, "    \"samples\":%u,\n", (unsigned int)report->frame_samples);
    (void)fprintf(fp, "    \"total_step_ms\":%.3f,\n", report->total_step_ms);
    (void)fprintf(fp, "    \"avg_step_ms\":%.3f,\n",
                  (report->frame_samples == 0u) ? 0.0 : (report->total_step_ms / (double)report->frame_samples));
    (void)fprintf(fp, "    \"max_step_ms\":%.3f\n", report->max_step_ms);
    (void)fprintf(fp, "  },\n");
    (void)fprintf(fp, "  \"first_frame\":");
    preview_json_write_frame(fp, &report->first_frame);
    (void)fprintf(fp, ",\n  \"last_frame\":");
    preview_json_write_frame(fp, &report->last_frame);
    (void)fprintf(fp, ",\n  \"final_state\":");
    if (report->has_final_state != 0u) {
        preview_json_write_result(fp, &report->final_state);
    } else {
        (void)fprintf(fp, "null");
    }
    (void)fprintf(fp, ",\n  \"events\":[\n");
    for (i = 0u; i < report->event_count; ++i) {
        const VNPreviewEvent* event;

        event = &report->events[i];
        (void)fprintf(fp, "    {\"kind\":");
        preview_json_write_string(fp, event->name);
        (void)fprintf(fp, ",\"trace_id\":");
        preview_json_write_string(fp, event->trace_id);
        (void)fprintf(fp, ",\"type\":%d,\"value\":%u", event->kind, (unsigned int)event->value);
        if (event->kind == VN_PREVIEW_EVENT_FRAME) {
            (void)fprintf(fp, ",\"host_step_ms\":%.3f,\"result\":", event->host_step_ms);
            preview_json_write_result(fp, &event->result);
        }
        (void)fprintf(fp, "}");
        if ((i + 1u) < report->event_count) {
            (void)fprintf(fp, ",");
        }
        (void)fprintf(fp, "\n");
    }
    (void)fprintf(fp, "  ]\n}\n");

    if (fp != stdout) {
        (void)fclose(fp);
    }
    return 0;
}

void preview_error(VNPreviewReport* report,
                   int error_code,
                   const char* message,
                   int status_code) {
    if (report == (VNPreviewReport*)0) {
        return;
    }
    report->status_code = status_code;
    report->error_code = error_code;
    report->error_name = vn_error_name(error_code);
    report->trace_id = preview_trace_id_for_error(status_code, error_code);
    {
        size_t len;
        if (message == (const char*)0) {
            report->error_message[0] = '\0';
        } else {
            len = strlen(message);
            if (len + 1u > sizeof(report->error_message)) {
                len = sizeof(report->error_message) - 1u;
            }
            if (len > 0u) {
                (void)memcpy(report->error_message, message, len);
            }
            report->error_message[len] = '\0';
        }
    }
    preview_report_add_event(report,
                             VN_PREVIEW_EVENT_ERROR,
                             report->error_name,
                             (vn_u32)(unsigned int)(error_code & 0x7FFFFFFF),
                             0.0,
                             (const VNRunResult*)0);
}

const char* preview_trace_id_for_error(int status_code, int error_code) {
    if (status_code == 2) {
        if (error_code == VN_E_IO) {
            return "preview.request.io";
        }
        if (error_code == VN_E_FORMAT) {
            return "preview.request.format";
        }
        if (error_code == VN_E_UNSUPPORTED) {
            return "preview.request.unsupported";
        }
        return "preview.request.invalid";
    }
    if (error_code == VN_E_RENDER_STATE) {
        return "preview.runtime.render";
    }
    if (error_code == VN_E_SCRIPT_BOUNDS) {
        return "preview.runtime.script";
    }
    return "preview.runtime.failed";
}

const char* preview_trace_id_for_event(int kind) {
    if (kind == VN_PREVIEW_EVENT_COMMAND) {
        return "preview.event.command";
    }
    if (kind == VN_PREVIEW_EVENT_FRAME) {
        return "preview.event.frame";
    }
    if (kind == VN_PREVIEW_EVENT_RELOAD) {
        return "preview.event.reload";
    }
    if (kind == VN_PREVIEW_EVENT_ERROR) {
        return "preview.event.error";
    }
    return "preview.event.unknown";
}

void preview_report_add_event(VNPreviewReport* report,
                              int kind,
                              const char* name,
                              vn_u32 value,
                              double host_step_ms,
                              const VNRunResult* result) {
    VNPreviewEvent* event;
    size_t len;

    if (report == (VNPreviewReport*)0) {
        return;
    }
    if (report->event_count >= VN_PREVIEW_MAX_EVENTS) {
        report->events_truncated = 1u;
        return;
    }
    event = &report->events[report->event_count];
    (void)memset(event, 0, sizeof(VNPreviewEvent));
    event->kind = kind;
    if (name != (const char*)0) {
        len = strlen(name);
        if (len + 1u > sizeof(event->name)) {
            len = sizeof(event->name) - 1u;
        }
        if (len > 0u) {
            (void)memcpy(event->name, name, len);
        }
        event->name[len] = '\0';
    }
    event->trace_id = preview_trace_id_for_event(kind);
    event->value = value;
    event->host_step_ms = host_step_ms;
    if (result != (const VNRunResult*)0) {
        event->has_result = 1u;
        event->result = *result;
    }
    report->event_count += 1u;
}

void preview_report_add_frame(VNPreviewReport* report,
                              const VNRunResult* result,
                              double host_step_ms,
                              vn_u32 trace_enabled) {
    if (report == (VNPreviewReport*)0 || result == (const VNRunResult*)0) {
        return;
    }
    if (report->first_frame.valid == 0u) {
        report->first_frame.valid = 1u;
        report->first_frame.host_step_ms = host_step_ms;
        report->first_frame.result = *result;
    }
    report->last_frame.valid = 1u;
    report->last_frame.host_step_ms = host_step_ms;
    report->last_frame.result = *result;
    report->has_final_state = 1u;
    report->final_state = *result;
    report->frame_samples += 1u;
    report->total_step_ms += host_step_ms;
    if (host_step_ms > report->max_step_ms) {
        report->max_step_ms = host_step_ms;
    }
    if (trace_enabled != 0u) {
        preview_report_add_event(report,
                                 VN_PREVIEW_EVENT_FRAME,
                                 "frame",
                                 result->frames_executed,
                                 host_step_ms,
                                 result);
    }
}

int preview_step_frames(VNRuntimeSession* session,
                        vn_u32 count,
                        VNPreviewReport* report,
                        vn_u32 trace_enabled) {
    vn_u32 i;
    vn_u32 prev_frames;
    int rc;
    double t0;
    double t1;
    VNRunResult result;

    if (session == (VNRuntimeSession*)0 || report == (VNPreviewReport*)0) {
        return VN_E_INVALID_ARG;
    }
    (void)memset(&result, 0, sizeof(result));
    for (i = 0u; i < count; ++i) {
        if (vn_runtime_session_is_done(session) != VN_FALSE) {
            report->session_done = 1u;
            break;
        }
        prev_frames = result.frames_executed;
        t0 = preview_now_ms();
        rc = vn_runtime_session_step(session, &result);
        t1 = preview_now_ms();
        if (rc != VN_OK) {
            return rc;
        }
        if (result.frames_executed > prev_frames) {
            preview_report_add_frame(report, &result, t1 - t0, trace_enabled);
        }
        report->session_done = (vn_u32)vn_runtime_session_is_done(session);
        if (report->session_done != 0u) {
            break;
        }
    }
    return VN_OK;
}

void preview_json_write_string(FILE* fp, const char* text) {
    const unsigned char* p;

    if (fp == (FILE*)0) {
        return;
    }
    (void)fputc('"', fp);
    if (text != (const char*)0) {
        p = (const unsigned char*)text;
        while (*p != '\0') {
            if (*p == '"' || *p == '\\') {
                (void)fputc('\\', fp);
                (void)fputc((int)*p, fp);
            } else if (*p == '\n') {
                (void)fputs("\\n", fp);
            } else if (*p == '\r') {
                (void)fputs("\\r", fp);
            } else if (*p == '\t') {
                (void)fputs("\\t", fp);
            } else if (*p < 32u) {
                (void)fprintf(fp, "\\u%04x", (unsigned int)*p);
            } else {
                (void)fputc((int)*p, fp);
            }
            p += 1;
        }
    }
    (void)fputc('"', fp);
}

void preview_json_write_result(FILE* fp, const VNRunResult* result) {
    if (fp == (FILE*)0 || result == (const VNRunResult*)0) {
        (void)fprintf(fp, "null");
        return;
    }
    (void)fprintf(fp,
                  "{\"frames_executed\":%u,\"text_id\":%u,\"vm_waiting\":%u,\"vm_ended\":%u,\"vm_error\":%u,"
                  "\"fade_alpha\":%u,\"fade_remain_ms\":%u,\"bgm_id\":%u,\"se_id\":%u,\"choice_count\":%u,"
                  "\"choice_selected_index\":%u,\"choice_text_id\":%u,\"op_count\":%u,\"backend_name\":",
                  (unsigned int)result->frames_executed,
                  (unsigned int)result->text_id,
                  (unsigned int)result->vm_waiting,
                  (unsigned int)result->vm_ended,
                  (unsigned int)result->vm_error,
                  (unsigned int)result->fade_alpha,
                  (unsigned int)result->fade_remain_ms,
                  (unsigned int)result->bgm_id,
                  (unsigned int)result->se_id,
                  (unsigned int)result->choice_count,
                  (unsigned int)result->choice_selected_index,
                  (unsigned int)result->choice_text_id,
                  (unsigned int)result->op_count);
    preview_json_write_string(fp, result->backend_name);
    (void)fprintf(fp,
                  ",\"perf_flags_effective\":%u,\"frame_reuse_hits\":%u,\"frame_reuse_misses\":%u,"
                  "\"op_cache_hits\":%u,\"op_cache_misses\":%u,\"dirty_tile_count\":%u,\"dirty_rect_count\":%u,"
                  "\"dirty_full_redraw\":%u,\"dirty_tile_frames\":%u,\"dirty_tile_total\":%u,\"dirty_rect_total\":%u,"
                  "\"dirty_full_redraws\":%u,\"render_width\":%u,\"render_height\":%u,"
                  "\"dynamic_resolution_tier\":%u,\"dynamic_resolution_switches\":%u}",
                  (unsigned int)result->perf_flags_effective,
                  (unsigned int)result->frame_reuse_hits,
                  (unsigned int)result->frame_reuse_misses,
                  (unsigned int)result->op_cache_hits,
                  (unsigned int)result->op_cache_misses,
                  (unsigned int)result->dirty_tile_count,
                  (unsigned int)result->dirty_rect_count,
                  (unsigned int)result->dirty_full_redraw,
                  (unsigned int)result->dirty_tile_frames,
                  (unsigned int)result->dirty_tile_total,
                  (unsigned int)result->dirty_rect_total,
                  (unsigned int)result->dirty_full_redraws,
                  (unsigned int)result->render_width,
                  (unsigned int)result->render_height,
                  (unsigned int)result->dynamic_resolution_tier,
                  (unsigned int)result->dynamic_resolution_switches);
}

void preview_json_write_frame(FILE* fp,
                              const VNPreviewFrameSample* frame) {
    if (fp == (FILE*)0 || frame == (const VNPreviewFrameSample*)0 || frame->valid == 0u) {
        (void)fprintf(fp, "null");
        return;
    }
    (void)fprintf(fp, "{\"host_step_ms\":%.3f,\"result\":", frame->host_step_ms);
    preview_json_write_result(fp, &frame->result);
    (void)fprintf(fp, "}");
}

double preview_now_ms(void) {
    return vn_platform_now_ms();
}
