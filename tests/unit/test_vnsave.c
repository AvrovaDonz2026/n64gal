#include <stdio.h>
#include <string.h>

#include "vn_error.h"
#include "vn_save.h"

static int expect_probe(const char* path,
                        int expected_rc,
                        vn_u32 expected_status,
                        vn_u32 expected_version,
                        vn_u32 expected_header_size,
                        vn_u32 expected_payload_size) {
    VNSaveProbe probe;
    int rc;

    rc = vnsave_probe_file(path, &probe);
    if (rc != expected_rc) {
        (void)fprintf(stderr, "unexpected rc path=%s rc=%d expected=%d\n", path, rc, expected_rc);
        return 1;
    }
    if (probe.status != expected_status) {
        (void)fprintf(stderr, "unexpected status path=%s status=%u expected=%u\n",
                      path,
                      (unsigned int)probe.status,
                      (unsigned int)expected_status);
        return 1;
    }
    if (probe.version != expected_version) {
        (void)fprintf(stderr, "unexpected version path=%s version=%u expected=%u\n",
                      path,
                      (unsigned int)probe.version,
                      (unsigned int)expected_version);
        return 1;
    }
    if (probe.header_size != expected_header_size) {
        (void)fprintf(stderr, "unexpected header_size path=%s header=%u expected=%u\n",
                      path,
                      (unsigned int)probe.header_size,
                      (unsigned int)expected_header_size);
        return 1;
    }
    if (probe.payload_size != expected_payload_size) {
        (void)fprintf(stderr, "unexpected payload_size path=%s payload=%u expected=%u\n",
                      path,
                      (unsigned int)probe.payload_size,
                      (unsigned int)expected_payload_size);
        return 1;
    }
    return 0;
}

int main(void) {
    if (expect_probe("tests/fixtures/vnsave/v1/sample.vnsave",
                     VN_OK,
                     VNSAVE_STATUS_OK,
                     VNSAVE_VERSION_1,
                     VNSAVE_HEADER_SIZE_V1,
                     4u) != 0) {
        return 1;
    }
    if (expect_probe("tests/fixtures/vnsave/v0/sample.vnsave",
                     VN_E_UNSUPPORTED,
                     VNSAVE_STATUS_PRE_1_0,
                     0u,
                     VNSAVE_HEADER_SIZE_V0,
                     4u) != 0) {
        return 1;
    }
    if (expect_probe("tests/fixtures/vnsave/invalid/pre_1_0.vnsave",
                     VN_E_UNSUPPORTED,
                     VNSAVE_STATUS_PRE_1_0,
                     0u,
                     VNSAVE_HEADER_SIZE_V0,
                     4u) != 0) {
        return 1;
    }
    if (expect_probe("tests/fixtures/vnsave/invalid/newer_version.vnsave",
                     VN_E_UNSUPPORTED,
                     VNSAVE_STATUS_NEWER_VERSION,
                     0x00020000u,
                     VNSAVE_HEADER_SIZE_V1,
                     4u) != 0) {
        return 1;
    }
    if (expect_probe("tests/fixtures/vnsave/invalid/bad_magic.vnsave",
                     VN_E_FORMAT,
                     VNSAVE_STATUS_BAD_MAGIC,
                     0u,
                     0u,
                     0u) != 0) {
        return 1;
    }
    if (expect_probe("tests/fixtures/vnsave/invalid/truncated.vnsave",
                     VN_E_FORMAT,
                     VNSAVE_STATUS_TRUNCATED,
                     0u,
                     0u,
                     0u) != 0) {
        return 1;
    }
    if (expect_probe("tests/fixtures/vnsave/invalid/invalid_flags.vnsave",
                     VN_E_FORMAT,
                     VNSAVE_STATUS_INVALID_HEADER,
                     VNSAVE_VERSION_1,
                     VNSAVE_HEADER_SIZE_V1,
                     4u) != 0) {
        return 1;
    }
    if (strcmp(vnsave_status_name(VNSAVE_STATUS_OK), "VNSAVE_STATUS_OK") != 0) {
        (void)fprintf(stderr, "unexpected status name for ok\n");
        return 1;
    }
    if (strcmp(vnsave_status_name(999u), "VNSAVE_STATUS_UNKNOWN") != 0) {
        (void)fprintf(stderr, "unexpected status name for unknown\n");
        return 1;
    }
    (void)printf("test_vnsave ok\n");
    return 0;
}
