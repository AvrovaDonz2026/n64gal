#include <ctype.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#endif

#include "build_config.h"
#include "platform.h"

#if defined(_WIN32)
static double vn_platform_large_integer_to_double(const LARGE_INTEGER* value) {
    double high;
    double low;

    if (value == (const LARGE_INTEGER*)0) {
        return 0.0;
    }
    high = (double)(unsigned long)value->HighPart;
    low = (double)(unsigned long)value->LowPart;
    return (high * 4294967296.0) + low;
}
#endif

static void vn_platform_copy_string(char* out_text, size_t out_size, const char* text) {
    size_t len;

    if (out_text == (char*)0 || out_size == 0u) {
        return;
    }
    out_text[0] = '\0';
    if (text == (const char*)0) {
        return;
    }
    len = strlen(text);
    if (len + 1u > out_size) {
        len = out_size - 1u;
    }
    if (len > 0u) {
        (void)memcpy(out_text, text, len);
    }
    out_text[len] = '\0';
}

static void vn_platform_append_string(char* out_text, size_t out_size, const char* text) {
    size_t used;
    size_t len;

    if (out_text == (char*)0 || out_size == 0u || text == (const char*)0) {
        return;
    }
    used = strlen(out_text);
    if (used + 1u >= out_size) {
        return;
    }
    len = strlen(text);
    if (used + len + 1u > out_size) {
        len = out_size - used - 1u;
    }
    if (len > 0u) {
        (void)memcpy(out_text + used, text, len);
        out_text[used + len] = '\0';
    }
}

const char* vn_platform_host_os_name(void) {
    return VN_HOST_OS_NAME;
}

const char* vn_platform_host_arch_name(void) {
    return VN_HOST_ARCH_NAME;
}

const char* vn_platform_host_compiler_name(void) {
    return VN_HOST_COMPILER_NAME;
}

double vn_platform_now_ms(void) {
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    double counter_value;
    double frequency_value;

    if (QueryPerformanceFrequency(&frequency) != 0 &&
        QueryPerformanceCounter(&counter) != 0) {
        counter_value = vn_platform_large_integer_to_double(&counter);
        frequency_value = vn_platform_large_integer_to_double(&frequency);
        if (frequency_value > 0.0) {
            return (counter_value * 1000.0) / frequency_value;
        }
    }
    return (double)GetTickCount();
#else
    struct timeval tv;
    clock_t now_ticks;

    if (gettimeofday(&tv, (struct timezone*)0) == 0) {
        return ((double)tv.tv_sec * 1000.0) + ((double)tv.tv_usec / 1000.0);
    }
    now_ticks = clock();
    if (now_ticks == (clock_t)-1) {
        return 0.0;
    }
    return ((double)now_ticks * 1000.0) / (double)CLOCKS_PER_SEC;
#endif
}

void vn_platform_sleep_ms(unsigned int ms) {
#if defined(_WIN32)
    Sleep((DWORD)ms);
#else
    struct timeval tv;

    if (ms == 0u) {
        return;
    }
    tv.tv_sec = (long)(ms / 1000u);
    tv.tv_usec = (long)((ms % 1000u) * 1000u);
    (void)select(0, (fd_set*)0, (fd_set*)0, (fd_set*)0, &tv);
#endif
}

FILE* vn_platform_fopen_read_binary(const char* path) {
    if (path == (const char*)0) {
        return (FILE*)0;
    }
    return fopen(path, "rb");
}

int vn_platform_path_is_absolute(const char* path) {
    if (path == (const char*)0 || path[0] == '\0') {
        return 0;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return 1;
    }
    if (isalpha((unsigned char)path[0]) &&
        path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\')) {
        return 1;
    }
    return 0;
}

char vn_platform_path_separator(void) {
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

void vn_platform_path_dirname(const char* path, char* out_dir, size_t out_size) {
    const char* slash;
    const char* backslash;
    const char* cut;
    size_t len;
    char root_sep;

    if (out_dir == (char*)0 || out_size == 0u) {
        return;
    }
    if (path == (const char*)0 || path[0] == '\0') {
        out_dir[0] = '.';
        out_dir[1] = '\0';
        return;
    }

    slash = strrchr(path, '/');
    backslash = strrchr(path, '\\');
    cut = slash;
    if (backslash != (const char*)0 && (cut == (const char*)0 || backslash > cut)) {
        cut = backslash;
    }
    if (cut == (const char*)0) {
        out_dir[0] = '.';
        out_dir[1] = '\0';
        return;
    }

    len = (size_t)(cut - path);
    if (len == 0u) {
        out_dir[0] = *cut;
        out_dir[1] = '\0';
        return;
    }
    if (len == 2u &&
        path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\')) {
        if (out_size < 4u) {
            out_dir[0] = '\0';
            return;
        }
        root_sep = path[2];
        out_dir[0] = path[0];
        out_dir[1] = ':';
        out_dir[2] = root_sep;
        out_dir[3] = '\0';
        return;
    }
    if (len + 1u > out_size) {
        len = out_size - 1u;
    }
    (void)memcpy(out_dir, path, len);
    out_dir[len] = '\0';
}

void vn_platform_path_join(char* out_path,
                           size_t out_size,
                           const char* base_dir,
                           const char* leaf) {
    size_t base_len;
    char sep;
    char sep_text[2];

    if (out_path == (char*)0 || out_size == 0u) {
        return;
    }
    out_path[0] = '\0';
    if (leaf == (const char*)0 || leaf[0] == '\0') {
        return;
    }
    if (base_dir == (const char*)0 ||
        base_dir[0] == '\0' ||
        vn_platform_path_is_absolute(leaf) != 0) {
        vn_platform_copy_string(out_path, out_size, leaf);
        return;
    }

    vn_platform_copy_string(out_path, out_size, base_dir);
    base_len = strlen(out_path);
    sep = vn_platform_path_separator();
    sep_text[0] = sep;
    sep_text[1] = '\0';
    if (base_len > 0u &&
        out_path[base_len - 1u] != '/' &&
        out_path[base_len - 1u] != '\\') {
        vn_platform_append_string(out_path, out_size, sep_text);
    }
    vn_platform_append_string(out_path, out_size, leaf);
}
