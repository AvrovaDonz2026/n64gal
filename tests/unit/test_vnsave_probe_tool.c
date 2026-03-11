#include <stdio.h>

int vnsave_probe_main(int argc, char** argv);

int main(void) {
    char* argv_ok[3];
    char* argv_bad[3];
    int rc;

    argv_ok[0] = "vnsave_probe";
    argv_ok[1] = "--in";
    argv_ok[2] = "tests/fixtures/vnsave/v1/sample.vnsave";

    rc = vnsave_probe_main(3, argv_ok);
    if (rc != 0) {
        (void)fprintf(stderr, "expected probe success rc=%d\n", rc);
        return 1;
    }

    argv_bad[0] = "vnsave_probe";
    argv_bad[1] = "--in";
    argv_bad[2] = "tests/fixtures/vnsave/invalid/newer_version.vnsave";

    rc = vnsave_probe_main(3, argv_bad);
    if (rc == 0) {
        (void)fprintf(stderr, "expected probe failure for newer version\n");
        return 1;
    }

    (void)printf("test_vnsave_probe_tool ok\n");
    return 0;
}
