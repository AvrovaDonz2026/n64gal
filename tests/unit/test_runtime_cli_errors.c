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
    char* argv_missing[2];
    char* argv_invalid[2];
    char* argv_scene[2];
    int rc;

    err_path = "test_runtime_cli_errors.stderr";
    (void)remove(err_path);

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

    (void)remove(err_path);
    (void)printf("test_runtime_cli_errors ok\n");
    return 0;
}
