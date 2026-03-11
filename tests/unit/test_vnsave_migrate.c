#include <stdio.h>
#include <string.h>

#include "vn_error.h"
#include "vn_save.h"

int vnsave_migrate_main(int argc, char** argv);

static int compare_files(const char* lhs_path, const char* rhs_path) {
    FILE* lhs;
    FILE* rhs;
    int lhs_ch;
    int rhs_ch;

    lhs = fopen(lhs_path, "rb");
    rhs = fopen(rhs_path, "rb");
    if (lhs == (FILE*)0 || rhs == (FILE*)0) {
        if (lhs != (FILE*)0) {
            (void)fclose(lhs);
        }
        if (rhs != (FILE*)0) {
            (void)fclose(rhs);
        }
        return 1;
    }
    do {
        lhs_ch = fgetc(lhs);
        rhs_ch = fgetc(rhs);
        if (lhs_ch != rhs_ch) {
            (void)fclose(lhs);
            (void)fclose(rhs);
            return 1;
        }
    } while (lhs_ch != EOF && rhs_ch != EOF);
    (void)fclose(lhs);
    (void)fclose(rhs);
    return 0;
}

int main(void) {
    char* argv_ok[5];
    char* argv_bad[5];
    VNSaveProbe probe;
    int rc;

    argv_ok[0] = "vnsave_migrate";
    argv_ok[1] = "--in";
    argv_ok[2] = "tests/fixtures/vnsave/v0/sample.vnsave";
    argv_ok[3] = "--out";
    argv_ok[4] = "test_vnsave_migrate_output.vnsave";

    rc = vnsave_migrate_main(5, argv_ok);
    if (rc != 0) {
        (void)fprintf(stderr, "expected migration success rc=%d\n", rc);
        return 1;
    }
    if (compare_files("test_vnsave_migrate_output.vnsave",
                      "tests/fixtures/vnsave/v1/from_v0_sample.vnsave") != 0) {
        (void)fprintf(stderr, "migrated file did not match golden output\n");
        return 1;
    }
    rc = vnsave_probe_file("test_vnsave_migrate_output.vnsave", &probe);
    if (rc != VN_OK || probe.version != VNSAVE_VERSION_1) {
        (void)fprintf(stderr, "migrated output probe failed rc=%d version=%u\n",
                      rc,
                      (unsigned int)probe.version);
        return 1;
    }

    argv_bad[0] = "vnsave_migrate";
    argv_bad[1] = "--in";
    argv_bad[2] = "tests/fixtures/vnsave/v1/sample.vnsave";
    argv_bad[3] = "--out";
    argv_bad[4] = "test_vnsave_migrate_should_fail.vnsave";

    rc = vnsave_migrate_main(5, argv_bad);
    if (rc == 0) {
        (void)fprintf(stderr, "expected migration failure for v1 input\n");
        return 1;
    }

    (void)remove("test_vnsave_migrate_output.vnsave");
    (void)remove("test_vnsave_migrate_should_fail.vnsave");
    (void)printf("test_vnsave_migrate ok\n");
    return 0;
}
