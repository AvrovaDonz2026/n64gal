#ifndef VN_PACK_H
#define VN_PACK_H

#include "vn_types.h"

#define VNPAK_VERSION_1 1u
#define VNPAK_VERSION_2 2u
#define VNPAK_READ_MIN_VERSION VNPAK_VERSION_1
#define VNPAK_READ_MAX_VERSION VNPAK_VERSION_2
#define VNPAK_WRITE_DEFAULT_VERSION VNPAK_VERSION_2

typedef struct {
    vn_u8 type;
    vn_u8 flags;
    vn_u16 width;
    vn_u16 height;
    vn_u32 data_off;
    vn_u32 data_size;
    vn_u32 crc32;
} ResourceEntry;

typedef struct {
    const char* path;
    vn_u16 version;
    vn_u32 resource_count;
    vn_u32 header_size;
    vn_u32 entry_size;
    vn_u32 file_size;
    ResourceEntry* entries;
} VNPak;

int vnpak_open(VNPak* pak, const char* path);
const ResourceEntry* vnpak_get(const VNPak* pak, vn_u32 id);
int vnpak_read_resource(const VNPak* pak, vn_u32 id, vn_u8* out_buf, vn_u32 out_size, vn_u32* out_read);
void vnpak_close(VNPak* pak);

#endif
