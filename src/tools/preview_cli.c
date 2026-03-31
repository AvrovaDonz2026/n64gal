#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vn_error.h"
#include "preview_internal.h"
#include "../core/platform.h"

static void preview_request_init(VNPreviewRequest* req);
static int preview_run_request(const VNPreviewRequest* req,
                               VNPreviewReport* report);
int vn_preview_run_cli(int argc, char** argv) {
    VNPreviewRequest req;
    VNPreviewReport report;
    int rc;

    preview_request_init(&req);
    preview_report_init(&report);

    rc = preview_parse_cli(&req, &report, argc, argv);
    if (rc == VN_PREVIEW_EXIT_HELP) {
        return 0;
    }
    if (rc == 0) {
        rc = preview_run_request(&req, &report);
    }

    if (rc != 0 && report.error_name == (const char*)0) {
        preview_error(&report, rc, "preview execution failed", 1);
    }

    if (report.error_name == (const char*)0) {
        report.error_name = vn_error_name(report.error_code);
    }
    if (report.trace_id == (const char*)0) {
        report.trace_id = "preview.ok";
    }

    if (preview_write_response(&req, &report) != 0 && rc == 0) {
        rc = 1;
    }
    return rc;
}

static void preview_request_init(VNPreviewRequest* req) {
    if (req == (VNPreviewRequest*)0) {
        return;
    }
    (void)memset(req, 0, sizeof(VNPreviewRequest));
    vn_run_config_init(&req->cfg);
    req->cfg.trace = 0u;
    req->cfg.emit_logs = 0u;
    req->trace_enabled = 0u;
}

static int preview_run_request(const VNPreviewRequest* req,
                               VNPreviewReport* report) {
    VNRuntimeSession* session;
    VNPreviewCommand command;
    vn_u32 i;
    int rc;

    if (req == (const VNPreviewRequest*)0 || report == (VNPreviewReport*)0) {
        preview_error(report, VN_E_INVALID_ARG, "null preview request", 1);
        return 1;
    }

    session = (VNRuntimeSession*)0;
    rc = vn_runtime_session_create(&req->cfg, &session);
    if (rc != VN_OK) {
        preview_error(report, rc, "vn_runtime_session_create failed", 1);
        return 1;
    }

    if (req->command_count == 0u) {
        preview_report_add_event(report, VN_PREVIEW_EVENT_COMMAND, "run_to_end", 0u, 0.0, (const VNRunResult*)0);
        rc = preview_step_frames(session, req->cfg.frames, report, req->trace_enabled);
        if (rc != VN_OK) {
            preview_error(report, rc, "preview run_to_end failed", 1);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
    } else {
        for (i = 0u; i < req->command_count; ++i) {
            command = req->commands[i];
            if (command.kind == VN_PREVIEW_CMD_RUN_TO_END) {
                preview_report_add_event(report, VN_PREVIEW_EVENT_COMMAND, "run_to_end", 0u, 0.0, (const VNRunResult*)0);
                rc = preview_step_frames(session, req->cfg.frames, report, req->trace_enabled);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview run_to_end failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_STEP_FRAME) {
                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "step_frame",
                                         command.value,
                                         0.0,
                                         (const VNRunResult*)0);
                rc = preview_step_frames(session,
                                         (command.value == 0u) ? 1u : command.value,
                                         report,
                                         req->trace_enabled);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview step_frame failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_RELOAD_SCENE) {
                preview_report_add_event(report, VN_PREVIEW_EVENT_RELOAD, "reload_scene", 0u, 0.0, (const VNRunResult*)0);
                report->reload_count += 1u;
                report->session_done = 0u;
                report->has_final_state = 0u;
                (void)memset(&report->final_state, 0, sizeof(VNRunResult));
                (void)vn_runtime_session_destroy(session);
                session = (VNRuntimeSession*)0;
                rc = vn_runtime_session_create(&req->cfg, &session);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview reload_scene failed", 1);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_SET_CHOICE) {
                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "set_choice",
                                         command.value,
                                         0.0,
                                         (const VNRunResult*)0);
                rc = vn_runtime_session_set_choice(session, (vn_u8)(command.value & 0xFFu));
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview set_choice failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_INJECT_CHOICE) {
                VNInputEvent input_event;

                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "inject_input",
                                         command.value,
                                         0.0,
                                         (const VNRunResult*)0);
                input_event.kind = VN_INPUT_KIND_CHOICE;
                input_event.value0 = command.value;
                input_event.value1 = 0u;
                rc = vn_runtime_session_inject_input(session, &input_event);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview inject_input failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_INJECT_KEY) {
                VNInputEvent input_event;

                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "inject_input.key",
                                         command.value,
                                         0.0,
                                         (const VNRunResult*)0);
                input_event.kind = VN_INPUT_KIND_KEY;
                input_event.value0 = command.value;
                input_event.value1 = 0u;
                rc = vn_runtime_session_inject_input(session, &input_event);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview inject_input key failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_INJECT_TRACE_TOGGLE) {
                VNInputEvent input_event;

                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "inject_input.trace_toggle",
                                         0u,
                                         0.0,
                                         (const VNRunResult*)0);
                input_event.kind = VN_INPUT_KIND_TRACE_TOGGLE;
                input_event.value0 = 0u;
                input_event.value1 = 0u;
                rc = vn_runtime_session_inject_input(session, &input_event);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview inject_input trace_toggle failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            } else if (command.kind == VN_PREVIEW_CMD_INJECT_QUIT) {
                VNInputEvent input_event;

                preview_report_add_event(report,
                                         VN_PREVIEW_EVENT_COMMAND,
                                         "inject_input.quit",
                                         0u,
                                         0.0,
                                         (const VNRunResult*)0);
                input_event.kind = VN_INPUT_KIND_QUIT;
                input_event.value0 = 0u;
                input_event.value1 = 0u;
                rc = vn_runtime_session_inject_input(session, &input_event);
                if (rc != VN_OK) {
                    preview_error(report, rc, "preview inject_input quit failed", 1);
                    (void)vn_runtime_session_destroy(session);
                    return 1;
                }
            }
        }
    }

    if (session != (VNRuntimeSession*)0) {
        report->session_done = (vn_u32)vn_runtime_session_is_done(session);
        (void)vn_runtime_session_destroy(session);
    }
    report->status_code = 0;
    report->error_code = VN_OK;
    report->error_name = vn_error_name(VN_OK);
    return 0;
}
