#include <stdio.h>
#include <string.h>

#include "vn_runtime.h"

static int file_contains(const char* path, const char* needle) {
    FILE* fp;
    char buffer[4096];
    size_t count;

    fp = fopen(path, "r");
    if (fp == (FILE*)0) {
        return 0;
    }
    count = fread(buffer, 1u, sizeof(buffer) - 1u, fp);
    (void)fclose(fp);
    buffer[count] = '\0';
    return (strstr(buffer, needle) != (char*)0) ? 1 : 0;
}

static int redirect_stderr_to(const char* path) {
    return (freopen(path, "w", stderr) == (FILE*)0) ? 1 : 0;
}

int main(void) {
    const char* err_path;
    const char* save_path;
    VNRunConfig cfg;
    VNRuntimeSession* session;
    char* argv_missing[2];
    char* argv_invalid[2];
    char* argv_scene[2];
    char* argv_load_missing[2];
    char* argv_load_conflict[3];
    char* argv_load_ok[3];
    int rc;
    vn_u32 i;

    err_path = "test_runtime_cli_errors.stderr";
    save_path = "test_runtime_cli_loadsave.vnsave";
    (void)remove(err_path);
    (void)remove(save_path);

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
    if (rc != 0 || session == (VNRuntimeSession*)0) {
        (void)printf("failed create save fixture rc=%d\n", rc);
        return 1;
    }
    for (i = 0u; i < 3u; ++i) {
        rc = vn_runtime_session_step(session, (VNRunResult*)0);
        if (rc != 0) {
            (void)printf("failed warmup save fixture rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
    }
    rc = vn_runtime_session_save_to_file(session, save_path, 3u, 11u);
    if (rc != 0) {
        (void)printf("failed save fixture rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    (void)vn_runtime_session_destroy(session);

    argv_missing[0] = (char*)"vn_player";
    argv_missing[1] = (char*)"--backend";
    if (redirect_stderr_to(err_path) != 0) {
        (void)printf("failed redirect stderr\n");
        return 1;
    }
    rc = vn_runtime_run_cli(2, argv_missing);
    (void)fflush(stderr);
    if (rc != 2) {
        (void)printf("missing arg case rc=%d\n", rc);
        return 1;
    }
    if (!file_contains(err_path, "trace_id=runtime.cli.arg.missing") ||
        !file_contains(err_path, "error_name=VN_E_INVALID_ARG")) {
        (void)printf("missing structured missing-arg output\n");
        return 1;
    }

    argv_invalid[0] = (char*)"vn_player";
    argv_invalid[1] = (char*)"--frames=abc";
    if (redirect_stderr_to(err_path) != 0) {
        return 1;
    }
    rc = vn_runtime_run_cli(2, argv_invalid);
    (void)fflush(stderr);
    if (rc != 2) {
        (void)printf("invalid arg case rc=%d\n", rc);
        return 1;
    }
    if (!file_contains(err_path, "trace_id=runtime.cli.arg.invalid") ||
        !file_contains(err_path, "arg=--frames")) {
        (void)printf("missing structured invalid-arg output\n");
        return 1;
    }

    argv_scene[0] = (char*)"vn_player";
    argv_scene[1] = (char*)"--scene=NOPE";
    if (redirect_stderr_to(err_path) != 0) {
        return 1;
    }
    rc = vn_runtime_run_cli(2, argv_scene);
    (void)fflush(stderr);
    if (rc != 2) {
        (void)printf("invalid scene case rc=%d\n", rc);
        return 1;
    }
    if (!file_contains(err_path, "trace_id=runtime.cli.scene.invalid") ||
        !file_contains(err_path, "arg=scene")) {
        (void)printf("missing structured invalid-scene output\n");
        return 1;
    }

    argv_load_missing[0] = (char*)"vn_player";
    argv_load_missing[1] = (char*)"--load-save";
    if (redirect_stderr_to(err_path) != 0) {
        return 1;
    }
    rc = vn_runtime_run_cli(2, argv_load_missing);
    (void)fflush(stderr);
    if (rc != 2) {
        (void)printf("missing load-save case rc=%d\n", rc);
        return 1;
    }
    if (!file_contains(err_path, "trace_id=runtime.cli.arg.missing") ||
        !file_contains(err_path, "arg=--load-save")) {
        (void)printf("missing structured load-save missing output\n");
        return 1;
    }

    argv_load_conflict[0] = (char*)"vn_player";
    argv_load_conflict[1] = (char*)"--load-save=test_runtime_cli_loadsave.vnsave";
    argv_load_conflict[2] = (char*)"--scene=S0";
    if (redirect_stderr_to(err_path) != 0) {
        return 1;
    }
    rc = vn_runtime_run_cli(3, argv_load_conflict);
    (void)fflush(stderr);
    if (rc != 2) {
        (void)printf("load-save conflict case rc=%d\n", rc);
        return 1;
    }
    if (!file_contains(err_path, "trace_id=runtime.cli.arg.invalid") ||
        !file_contains(err_path, "arg=--load-save") ||
        !file_contains(err_path, "value=--scene")) {
        (void)printf("missing structured load-save conflict output\n");
        return 1;
    }

    argv_load_ok[0] = (char*)"vn_player";
    argv_load_ok[1] = (char*)"--load-save=test_runtime_cli_loadsave.vnsave";
    argv_load_ok[2] = (char*)"--quiet";
    rc = vn_runtime_run_cli(3, argv_load_ok);
    if (rc != 0) {
        (void)printf("load-save success case rc=%d\n", rc);
        (void)remove(save_path);
        return 1;
    }

    (void)remove(err_path);
    (void)remove(save_path);
    (void)printf("test_runtime_cli_errors ok\n");
    return 0;
}
