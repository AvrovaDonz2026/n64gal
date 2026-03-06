#include <stdio.h>
#include <string.h>

#include "vn_preview.h"

static int file_contains(const char* path, const char* needle) {
    FILE* fp;
    char buffer[32768];
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

int main(void) {
    const char* request_path;
    const char* response_path;
    FILE* fp;
    char* argv_req[5];
    char* argv_cli[10];
    int rc;

    request_path = "tests/integration/preview_protocol_request.tmp";
    response_path = "tests/integration/preview_protocol_response.tmp.json";
    (void)remove(request_path);
    (void)remove(response_path);

    fp = fopen(request_path, "w");
    if (fp == (FILE*)0) {
        (void)fprintf(stderr, "open request file failed\n");
        return 1;
    }
    (void)fprintf(fp, "preview_protocol=v1\n");
    (void)fprintf(fp, "project_dir=.\n");
    (void)fprintf(fp, "scene_name=S2\n");
    (void)fprintf(fp, "frames=8\n");
    (void)fprintf(fp, "trace=1\n");
    (void)fprintf(fp, "command=set_choice:1\n");
    (void)fprintf(fp, "command=inject_input:choice:1\n");
    (void)fprintf(fp, "command=step_frame:8\n");
    (void)fclose(fp);

    argv_req[0] = (char*)"vn_previewd";
    argv_req[1] = (char*)"--request";
    argv_req[2] = (char*)request_path;
    argv_req[3] = (char*)"--response";
    argv_req[4] = (char*)response_path;

    rc = vn_preview_run_cli(5, argv_req);
    if (rc != 0) {
        (void)fprintf(stderr, "request mode failed rc=%d\n", rc);
        (void)remove(request_path);
        (void)remove(response_path);
        return 1;
    }
    if (!file_contains(response_path, "\"preview_protocol\":\"v1\"")) {
        (void)fprintf(stderr, "missing protocol version\n");
        (void)remove(request_path);
        (void)remove(response_path);
        return 1;
    }
    if (!file_contains(response_path, "\"status\":\"ok\"")) {
        (void)fprintf(stderr, "missing ok status\n");
        (void)remove(request_path);
        (void)remove(response_path);
        return 1;
    }
    if (!file_contains(response_path, "\"choice_selected_index\":1")) {
        (void)fprintf(stderr, "missing selected choice\n");
        (void)remove(request_path);
        (void)remove(response_path);
        return 1;
    }
    if (!file_contains(response_path, "\"events\"")) {
        (void)fprintf(stderr, "missing events\n");
        (void)remove(request_path);
        (void)remove(response_path);
        return 1;
    }

    argv_cli[0] = (char*)"vn_previewd";
    argv_cli[1] = (char*)"--scene=S1";
    argv_cli[2] = (char*)"--frames=2";
    argv_cli[3] = (char*)"--trace";
    argv_cli[4] = (char*)"--command=step_frame:2";
    argv_cli[5] = (char*)"--command=reload_scene";
    argv_cli[6] = (char*)"--command=inject_input:key:q";
    argv_cli[7] = (char*)"--command=step_frame:1";
    argv_cli[8] = (char*)"--response";
    argv_cli[9] = (char*)response_path;

    rc = vn_preview_run_cli(10, argv_cli);
    if (rc != 0) {
        (void)fprintf(stderr, "cli mode failed rc=%d\n", rc);
        (void)remove(request_path);
        (void)remove(response_path);
        return 1;
    }
    if (!file_contains(response_path, "\"scene_name\":\"S1\"")) {
        (void)fprintf(stderr, "missing scene name\n");
        (void)remove(request_path);
        (void)remove(response_path);
        return 1;
    }
    if (!file_contains(response_path, "\"reload_count\":1")) {
        (void)fprintf(stderr, "missing reload count\n");
        (void)remove(request_path);
        (void)remove(response_path);
        return 1;
    }
    if (!file_contains(response_path, "inject_input.key")) {
        (void)fprintf(stderr, "missing injected key event\n");
        (void)remove(request_path);
        (void)remove(response_path);
        return 1;
    }
    if (!file_contains(response_path, "\"session_done\":1")) {
        (void)fprintf(stderr, "missing done state\n");
        (void)remove(request_path);
        (void)remove(response_path);
        return 1;
    }

    (void)remove(request_path);
    (void)remove(response_path);
    (void)printf("test_preview_protocol ok\n");
    return 0;
}
