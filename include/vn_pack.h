#ifndef VN_PACK_H
#define VN_PACK_H

#include "vn_types.h"

typedef struct {
    vn_u8 type;
    vn_u8 flags;
    vn_u16 width;
    vn_u16 height;
    vn_u32 data_off;
    vn_u32 data_size;
} ResourceEntry;

typedef struct {
    const char* path;
    vn_u16 version;
    vn_u32 resource_count;
    ResourceEntry* entries;
} VNPak;

int vnpak_open(VNPak* pak, const char* path);
const ResourceEntry* vnpak_get(const VNPak* pak, vn_u32 id);
void vnpak_close(VNPak* pak);

#endif
