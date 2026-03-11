#include <stdio.h>
#include <string.h>

#include "vn_error.h"
#include "vn_save.h"

static int vnsave_probe_usage(const char* argv0) {
    (void)fprintf(stderr,
                  "usage: %s --in <save.vnsave>\n",
                  argv0 != (const char*)0 ? argv0 : "vnsave_probe");
    return 2;
}

int vnsave_probe_main(int argc, char** argv) {
    const char* in_path;
    VNSaveProbe probe;
    int i;
    int rc;

    in_path = (const char*)0;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--in") == 0) {
            if ((i + 1) >= argc) {
                return vnsave_probe_usage(argv[0]);
            }
            in_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            return vnsave_probe_usage(argv[0]);
        } else {
            return vnsave_probe_usage(argv[0]);
        }
    }

    if (in_path == (const char*)0) {
        return vnsave_probe_usage(argv[0]);
    }

    rc = vnsave_probe_file(in_path, &probe);
    if (rc != VN_OK) {
        (void)fprintf(stderr,
                      "trace_id=tool.probe.vnsave.failed error_code=%d error_name=%s save_status=%s version=%u header_size=%u payload_size=%u slot=%u scene=%u message=save probe failed\n",
                      rc,
                      vn_error_name(rc),
                      vnsave_status_name(probe.status),
                      (unsigned int)probe.version,
                      (unsigned int)probe.header_size,
                      (unsigned int)probe.payload_size,
                      (unsigned int)probe.slot_id,
                      (unsigned int)probe.scene_id);
        return 1;
    }

    (void)printf(
        "trace_id=tool.probe.vnsave.ok error_code=%d error_name=%s save_status=%s version=%u header_size=%u payload_size=%u slot=%u scene=%u timestamp=%u payload_crc32=0x%08X path=%s\n",
        rc,
        vn_error_name(rc),
        vnsave_status_name(probe.status),
        (unsigned int)probe.version,
        (unsigned int)probe.header_size,
        (unsigned int)probe.payload_size,
        (unsigned int)probe.slot_id,
        (unsigned int)probe.scene_id,
        (unsigned int)probe.timestamp_s,
        (unsigned int)probe.payload_crc32,
        in_path);
    return 0;
}

#ifndef VN_SAVE_PROBE_NO_MAIN
int main(int argc, char** argv) {
    return vnsave_probe_main(argc, argv);
}
#endif
