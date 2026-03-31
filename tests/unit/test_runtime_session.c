#include <stdio.h>
#include <string.h>

#include "vn_frontend.h"
#include "vn_error.h"
#include "vn_runtime.h"
#include "vn_save.h"

static int expect_same_resume_result(const VNRunResult* expected,
                                     const VNRunResult* actual) {
    if (expected == (const VNRunResult*)0 || actual == (const VNRunResult*)0) {
        return 1;
    }
    if (expected->frames_executed != actual->frames_executed ||
        expected->text_id != actual->text_id ||
        expected->vm_waiting != actual->vm_waiting ||
        expected->vm_ended != actual->vm_ended ||
        expected->vm_error != actual->vm_error ||
        expected->fade_alpha != actual->fade_alpha ||
        expected->fade_remain_ms != actual->fade_remain_ms ||
        expected->bgm_id != actual->bgm_id ||
        expected->se_id != actual->se_id ||
        expected->choice_count != actual->choice_count ||
        expected->choice_selected_index != actual->choice_selected_index ||
        expected->choice_text_id != actual->choice_text_id ||
        expected->op_count != actual->op_count ||
        expected->perf_flags_effective != actual->perf_flags_effective ||
        expected->render_width != actual->render_width ||
        expected->render_height != actual->render_height ||
        expected->dynamic_resolution_tier != actual->dynamic_resolution_tier ||
        expected->dynamic_resolution_switches != actual->dynamic_resolution_switches) {
        return 1;
    }
    if (expected->backend_name == (const char*)0 ||
        actual->backend_name == (const char*)0 ||
        strcmp(expected->backend_name, actual->backend_name) != 0) {
        return 1;
    }
    return 0;
}

int main(void) {
    VNRunConfig cfg;
    VNRunResult res;
    VNRunResult resumed_res;
    VNRunResult restored_res;
    VNRunResult file_expected_res;
    VNRuntimeSession* session;
    VNRuntimeSession* restored_session;
    VNRuntimeSession* file_session;
    VNRuntimeSessionSnapshot snapshot;
    VNSaveProbe probe;
    int rc;
    vn_u32 guard;
    vn_u32 i;
    const char* save_path;

    session = (VNRuntimeSession*)0;
    restored_session = (VNRuntimeSession*)0;
    file_session = (VNRuntimeSession*)0;
    memset((void*)&res, 0, sizeof(res));
    memset((void*)&resumed_res, 0, sizeof(resumed_res));
    memset((void*)&restored_res, 0, sizeof(restored_res));
    memset((void*)&file_expected_res, 0, sizeof(file_expected_res));
    memset((void*)&snapshot, 0, sizeof(snapshot));
    memset((void*)&probe, 0, sizeof(probe));
    save_path = "test_runtime_session_snapshot.vnsave";
    (void)remove(save_path);
    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 8u;
    cfg.dt_ms = 16u;
    cfg.choice_index = 0u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "vn_runtime_session_create failed rc=%d\n", rc);
        return 1;
    }

    rc = vn_runtime_session_set_choice(session, 1u);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vn_runtime_session_set_choice failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }

    guard = 0u;
    while (vn_runtime_session_is_done(session) == VN_FALSE) {
        rc = vn_runtime_session_step(session, &res);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "vn_runtime_session_step failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
        guard += 1u;
        if (guard > 64u) {
            (void)fprintf(stderr, "runtime session did not finish\n");
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
    }

    if (res.frames_executed == 0u) {
        (void)fprintf(stderr, "no frames executed\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (res.choice_selected_index != 1u) {
        (void)fprintf(stderr, "choice index mismatch got=%u\n", (unsigned int)res.choice_selected_index);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (res.backend_name == (const char*)0) {
        (void)fprintf(stderr, "backend name missing\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }

    rc = vn_runtime_session_destroy(session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vn_runtime_session_destroy failed rc=%d\n", rc);
        return 1;
    }
    session = (VNRuntimeSession*)0;

    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 8u;
    cfg.dt_ms = 16u;
    cfg.choice_index = 1u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "snapshot create failed rc=%d\n", rc);
        return 1;
    }
    for (i = 0u; i < 3u; ++i) {
        rc = vn_runtime_session_step(session, &res);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "snapshot warmup step failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
    }
    rc = vn_runtime_session_capture_snapshot(session, &snapshot);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "capture snapshot failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    rc = vn_runtime_session_create_from_snapshot(&snapshot, &restored_session);
    if (rc != VN_OK || restored_session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "restore snapshot failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    rc = vn_runtime_session_step(session, &resumed_res);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "resume original after snapshot failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    rc = vn_runtime_session_step(restored_session, &restored_res);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "resume restored after snapshot failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (expect_same_resume_result(&resumed_res, &restored_res) != 0) {
        (void)fprintf(stderr,
                      "snapshot resume mismatch orig frames=%u text=%u wait=%u fade=%u bgm=%u choice=%u ops=%u restored frames=%u text=%u wait=%u fade=%u bgm=%u choice=%u ops=%u\n",
                      (unsigned int)resumed_res.frames_executed,
                      (unsigned int)resumed_res.text_id,
                      (unsigned int)resumed_res.vm_waiting,
                      (unsigned int)resumed_res.fade_alpha,
                      (unsigned int)resumed_res.bgm_id,
                      (unsigned int)resumed_res.choice_selected_index,
                      (unsigned int)resumed_res.op_count,
                      (unsigned int)restored_res.frames_executed,
                      (unsigned int)restored_res.text_id,
                      (unsigned int)restored_res.vm_waiting,
                      (unsigned int)restored_res.fade_alpha,
                      (unsigned int)restored_res.bgm_id,
                      (unsigned int)restored_res.choice_selected_index,
                      (unsigned int)restored_res.op_count);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (vn_runtime_session_capture_snapshot((const VNRuntimeSession*)0, &snapshot) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null session snapshot\n");
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (vn_runtime_session_create_from_snapshot((const VNRuntimeSessionSnapshot*)0, &restored_session) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null snapshot create\n");
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    rc = vn_runtime_session_save_to_file(session, save_path, 7u, 123u);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "save snapshot to file failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    rc = vnsave_probe_file(save_path, &probe);
    if (rc != VN_OK ||
        probe.version != VNSAVE_VERSION_1 ||
        probe.slot_id != 7u ||
        probe.scene_id != VN_SCENE_S2 ||
        probe.script_pc != snapshot.vm_pc_offset ||
        probe.timestamp_s != 123u) {
        (void)fprintf(stderr,
                      "saved snapshot probe mismatch rc=%d version=%u slot=%u scene=%u pc=%u timestamp=%u\n",
                      rc,
                      (unsigned int)probe.version,
                      (unsigned int)probe.slot_id,
                      (unsigned int)probe.scene_id,
                      (unsigned int)probe.script_pc,
                      (unsigned int)probe.timestamp_s);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    rc = vn_runtime_session_load_from_file(save_path, &file_session);
    if (rc != VN_OK || file_session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "load snapshot from file failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    rc = vn_runtime_session_step(session, &file_expected_res);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "resume original after file save failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(file_session);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    rc = vn_runtime_session_step(file_session, &restored_res);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "resume file-restored session failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(file_session);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    if (expect_same_resume_result(&file_expected_res, &restored_res) != 0) {
        (void)fprintf(stderr,
                      "file snapshot resume mismatch orig frames=%u text=%u wait=%u fade=%u bgm=%u se=%u choice=%u choice_text=%u ops=%u backend=%s restored frames=%u text=%u wait=%u fade=%u bgm=%u se=%u choice=%u choice_text=%u ops=%u backend=%s\n",
                      (unsigned int)file_expected_res.frames_executed,
                      (unsigned int)file_expected_res.text_id,
                      (unsigned int)file_expected_res.vm_waiting,
                      (unsigned int)file_expected_res.fade_alpha,
                      (unsigned int)file_expected_res.bgm_id,
                      (unsigned int)file_expected_res.se_id,
                      (unsigned int)file_expected_res.choice_selected_index,
                      (unsigned int)file_expected_res.choice_text_id,
                      (unsigned int)file_expected_res.op_count,
                      file_expected_res.backend_name != (const char*)0 ? file_expected_res.backend_name : "null",
                      (unsigned int)restored_res.frames_executed,
                      (unsigned int)restored_res.text_id,
                      (unsigned int)restored_res.vm_waiting,
                      (unsigned int)restored_res.fade_alpha,
                      (unsigned int)restored_res.bgm_id,
                      (unsigned int)restored_res.se_id,
                      (unsigned int)restored_res.choice_selected_index,
                      (unsigned int)restored_res.choice_text_id,
                      (unsigned int)restored_res.op_count,
                      restored_res.backend_name != (const char*)0 ? restored_res.backend_name : "null");
        (void)vn_runtime_session_destroy(file_session);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    if (vn_runtime_session_save_to_file((const VNRuntimeSession*)0, save_path, 0u, 0u) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null save_to_file\n");
        (void)vn_runtime_session_destroy(file_session);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    input.kind = VN_INPUT_KIND_QUIT;
    input.value0 = 0u;
    input.value1 = 0u;
    rc = vn_runtime_session_inject_input(session, &input);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "inject quit before save_to_file failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(file_session);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    if (vn_runtime_session_save_to_file(session, save_path, 0u, 0u) != VN_E_UNSUPPORTED) {
        (void)fprintf(stderr, "expected VN_E_UNSUPPORTED for pending-input save_to_file\n");
        (void)vn_runtime_session_destroy(file_session);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    if (vn_runtime_session_load_from_file((const char*)0, &file_session) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null load_from_file path\n");
        (void)vn_runtime_session_destroy(file_session);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    if (vn_runtime_session_load_from_file("tests/fixtures/vnsave/v1/sample.vnsave", &file_session) != VN_E_UNSUPPORTED) {
        (void)fprintf(stderr, "expected VN_E_UNSUPPORTED for non-runtime vnsave payload\n");
        (void)vn_runtime_session_destroy(file_session);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    rc = vn_runtime_session_destroy(file_session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "file restored destroy failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(restored_session);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    file_session = (VNRuntimeSession*)0;
    (void)remove(save_path);
    rc = vn_runtime_session_destroy(restored_session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "restored destroy failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    restored_session = (VNRuntimeSession*)0;

    rc = vn_runtime_session_destroy(session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vn_runtime_session_destroy failed rc=%d\n", rc);
        return 1;
    }

    rc = vn_runtime_session_create((const VNRunConfig*)0, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "default create failed rc=%d\n", rc);
        return 1;
    }
    rc = vn_runtime_session_destroy(session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "default destroy failed rc=%d\n", rc);
        return 1;
    }

    if (vn_runtime_session_create(&cfg, (VNRuntimeSession**)0) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null out_session\n");
        return 1;
    }
    if (vn_runtime_session_step((VNRuntimeSession*)0, &res) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null session step\n");
        return 1;
    }
    if (vn_runtime_session_set_choice((VNRuntimeSession*)0, 0u) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null session set_choice\n");
        return 1;
    }
    if (vn_runtime_session_is_done((const VNRuntimeSession*)0) != VN_TRUE) {
        (void)fprintf(stderr, "expected null session as done\n");
        return 1;
    }
    if (vn_runtime_session_destroy((VNRuntimeSession*)0) != VN_OK) {
        (void)fprintf(stderr, "null destroy should be ok\n");
        return 1;
    }

    (void)printf("test_runtime_session ok\n");
    return 0;
}
