#include <stdio.h>
#include <string.h>

#include "vn_error.h"

static int expect_name(int code, const char* expected) {
    const char* actual;

    actual = vn_error_name(code);
    if (actual == (const char*)0) {
        (void)fprintf(stderr, "vn_error_name returned null for code=%d\n", code);
        return 1;
    }
    if (strcmp(actual, expected) != 0) {
        (void)fprintf(stderr, "expected %s got %s for code=%d\n", expected, actual, code);
        return 1;
    }
    return 0;
}

int main(void) {
    if (expect_name(VN_OK, "VN_OK") != 0) {
        return 1;
    }
    if (expect_name(VN_E_INVALID_ARG, "VN_E_INVALID_ARG") != 0) {
        return 1;
    }
    if (expect_name(VN_E_IO, "VN_E_IO") != 0) {
        return 1;
    }
    if (expect_name(VN_E_FORMAT, "VN_E_FORMAT") != 0) {
        return 1;
    }
    if (expect_name(VN_E_UNSUPPORTED, "VN_E_UNSUPPORTED") != 0) {
        return 1;
    }
    if (expect_name(VN_E_NOMEM, "VN_E_NOMEM") != 0) {
        return 1;
    }
    if (expect_name(VN_E_SCRIPT_BOUNDS, "VN_E_SCRIPT_BOUNDS") != 0) {
        return 1;
    }
    if (expect_name(VN_E_RENDER_STATE, "VN_E_RENDER_STATE") != 0) {
        return 1;
    }
    if (expect_name(VN_E_AUDIO_DEVICE, "VN_E_AUDIO_DEVICE") != 0) {
        return 1;
    }
    if (expect_name(-999, "VN_E_UNKNOWN") != 0) {
        return 1;
    }
    (void)printf("test_error_codes ok\n");
    return 0;
}
