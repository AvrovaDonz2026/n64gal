#include <stdio.h>
#include <string.h>

#include "../../src/core/platform.h"

static int expect_string(const char* label, const char* actual, const char* expected) {
    if (strcmp(actual, expected) != 0) {
        (void)fprintf(stderr,
                      "%s mismatch actual=%s expected=%s\n",
                      label,
                      actual,
                      expected);
        return 1;
    }
    return 0;
}

static int expect_known_name(const char* label, const char* value) {
    if (value == (const char*)0 || value[0] == '\0' || strcmp(value, "unknown") == 0) {
        (void)fprintf(stderr, "%s should be known got=%s\n", label, value == (const char*)0 ? "<null>" : value);
        return 1;
    }
    return 0;
}

int main(void) {
    char path[128];
    char dirbuf[128];
    double t0;
    double t1;
    double t2;

    if (expect_known_name("host os", vn_platform_host_os_name()) != 0) {
        return 1;
    }
    if (expect_known_name("host arch", vn_platform_host_arch_name()) != 0) {
        return 1;
    }
    if (expect_known_name("host compiler", vn_platform_host_compiler_name()) != 0) {
        return 1;
    }

    t0 = vn_platform_now_ms();
    t1 = vn_platform_now_ms();
    if (t0 <= 0.0 || t1 <= 0.0 || t1 + 1.0 < t0) {
        (void)fprintf(stderr, "platform now_ms should be monotonic and non-zero\n");
        return 1;
    }
    vn_platform_sleep_ms(1u);
    t2 = vn_platform_now_ms();
    if (t2 + 1.0 < t1) {
        (void)fprintf(stderr, "platform sleep should not move time backwards\n");
        return 1;
    }

    if (vn_platform_path_is_absolute("/tmp/demo.vnpak") == 0) {
        (void)fprintf(stderr, "expected unix absolute path\n");
        return 1;
    }
    if (vn_platform_path_is_absolute("C:\\demo\\demo.vnpak") == 0) {
        (void)fprintf(stderr, "expected drive absolute path\n");
        return 1;
    }
    if (vn_platform_path_is_absolute("C:demo\\demo.vnpak") != 0) {
        (void)fprintf(stderr, "drive-relative path should not be absolute\n");
        return 1;
    }
    if (vn_platform_path_is_absolute("assets/demo.vnpak") != 0) {
        (void)fprintf(stderr, "relative path should not be absolute\n");
        return 1;
    }

    vn_platform_path_dirname("assets/demo/demo.vnpak", dirbuf, sizeof(dirbuf));
    if (expect_string("dirname assets", dirbuf, "assets/demo") != 0) {
        return 1;
    }

    vn_platform_path_dirname("demo.vnpak", dirbuf, sizeof(dirbuf));
    if (expect_string("dirname leaf", dirbuf, ".") != 0) {
        return 1;
    }

    vn_platform_path_dirname("/demo.vnpak", dirbuf, sizeof(dirbuf));
    if (expect_string("dirname root", dirbuf, "/") != 0) {
        return 1;
    }

    vn_platform_path_dirname("C:\\demo.vnpak", dirbuf, sizeof(dirbuf));
    if (expect_string("dirname drive root", dirbuf, "C:\\") != 0) {
        return 1;
    }

    vn_platform_path_join(path, sizeof(path), "assets", "demo.vnpak");
#if defined(_WIN32)
    if (expect_string("join relative", path, "assets\\demo.vnpak") != 0) {
        return 1;
    }
#else
    if (expect_string("join relative", path, "assets/demo.vnpak") != 0) {
        return 1;
    }
#endif

    vn_platform_path_join(path, sizeof(path), "", "demo.vnpak");
    if (expect_string("join empty base", path, "demo.vnpak") != 0) {
        return 1;
    }

    vn_platform_path_join(path, sizeof(path), "assets", "C:\\demo\\demo.vnpak");
    if (expect_string("join absolute leaf", path, "C:\\demo\\demo.vnpak") != 0) {
        return 1;
    }

    (void)printf("test_platform_paths ok\n");
    return 0;
}
