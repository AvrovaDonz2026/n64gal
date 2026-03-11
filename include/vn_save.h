#ifndef VN_SAVE_H
#define VN_SAVE_H

#include "vn_types.h"

#define VNSAVE_VERSION_1 0x00010000u
#define VNSAVE_HEADER_SIZE_V0 16u
#define VNSAVE_HEADER_SIZE_V1 32u

#define VNSAVE_STATUS_OK 0u
#define VNSAVE_STATUS_BAD_MAGIC 1u
#define VNSAVE_STATUS_TRUNCATED 2u
#define VNSAVE_STATUS_PRE_1_0 3u
#define VNSAVE_STATUS_NEWER_VERSION 4u
#define VNSAVE_STATUS_INVALID_HEADER 5u

typedef struct {
    const char* path;
    vn_u32 version;
    vn_u32 header_size;
    vn_u32 payload_size;
    vn_u32 slot_id;
    vn_u32 script_pc;
    vn_u32 scene_id;
    vn_u32 timestamp_s;
    vn_u32 payload_crc32;
    vn_u32 status;
    int error_code;
} VNSaveProbe;

int vnsave_probe_file(const char* path, VNSaveProbe* out_probe);
int vnsave_migrate_v0_to_v1_file(const char* in_path, const char* out_path);
const char* vnsave_status_name(vn_u32 status);

#endif
