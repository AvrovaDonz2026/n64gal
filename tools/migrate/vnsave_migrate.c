#include <stdio.h>
#include <string.h>

#include "vn_error.h"
#include "vn_save.h"

static int vnsave_migrate_usage(const char* argv0) {
    (void)fprintf(stderr,
                  "usage: %s --in <legacy_v0.vnsave> --out <v1.vnsave>\n",
                  argv0 != (const char*)0 ? argv0 : "vnsave_migrate");
    return 2;
}

int vnsave_migrate_main(int argc, char** argv) {
    const char* in_path;
    const char* out_path;
    VNSaveProbe probe;
    int i;
    int rc;

    in_path = (const char*)0;
    out_path = (const char*)0;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--in") == 0) {
            if ((i + 1) >= argc) {
                return vnsave_migrate_usage(argv[0]);
            }
            in_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0) {
            if ((i + 1) >= argc) {
                return vnsave_migrate_usage(argv[0]);
            }
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            return vnsave_migrate_usage(argv[0]);
        } else {
            return vnsave_migrate_usage(argv[0]);
        }
    }

    if (in_path == (const char*)0 || out_path == (const char*)0) {
        return vnsave_migrate_usage(argv[0]);
    }

    rc = vnsave_probe_file(in_path, &probe);
    if (rc != VN_E_UNSUPPORTED || probe.status != VNSAVE_STATUS_PRE_1_0) {
        if (rc == VN_OK) {
            rc = VN_E_UNSUPPORTED;
        }
        (void)fprintf(stderr,
                      "trace_id=tool.vnsave_migrate.input.unsupported error_code=%d error_name=%s save_status=%s version=%u header_size=%u payload_size=%u slot=%u scene=%u message=input is not a supported legacy v0 save\n",
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

    rc = vnsave_migrate_v0_to_v1_file(in_path, out_path);
    if (rc != VN_OK) {
        (void)fprintf(stderr,
                      "trace_id=tool.vnsave_migrate.failed error_code=%d error_name=%s save_status=%s version=%u header_size=%u payload_size=%u slot=%u scene=%u message=migration failed\n",
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

    (void)printf("vnsave_migrate ok trace_id=tool.vnsave_migrate.ok in=%s out=%s version=%u slot=%u scene=%u payload=%u\n",
                 in_path,
                 out_path,
                 (unsigned int)VNSAVE_VERSION_1,
                 (unsigned int)probe.slot_id,
                 (unsigned int)probe.scene_id,
                 (unsigned int)probe.payload_size);
    return 0;
}

#ifndef VN_SAVE_MIGRATE_NO_MAIN
int main(int argc, char** argv) {
    return vnsave_migrate_main(argc, argv);
}
#endif
