#include <stdio.h>
#include <stdlib.h>

#include "vn_pack.h"
#include "vn_error.h"

#define VNPAK_MAGIC_0 ((vn_u8)'V')
#define VNPAK_MAGIC_1 ((vn_u8)'N')
#define VNPAK_MAGIC_2 ((vn_u8)'P')
#define VNPAK_MAGIC_3 ((vn_u8)'K')

#define VNPAK_HEADER_SIZE 8u
#define VNPAK_ENTRY_SIZE  14u
#define VNPAK_MAX_ENTRIES 4096u

static vn_u16 vnpak_u16_le(const vn_u8* p) {
    return (vn_u16)((vn_u16)p[0] | ((vn_u16)p[1] << 8));
}

static vn_u32 vnpak_u32_le(const vn_u8* p) {
    return (vn_u32)(((vn_u32)p[0]) |
                    ((vn_u32)p[1] << 8) |
                    ((vn_u32)p[2] << 16) |
                    ((vn_u32)p[3] << 24));
}

int vnpak_open(VNPak* pak, const char* path) {
    FILE* fp;
    vn_u8 header[VNPAK_HEADER_SIZE];
    vn_u16 version;
    vn_u16 entry_count_16;
    vn_u32 entry_count;
    ResourceEntry* entries;
    vn_u32 i;
    int ok;

    if (pak == (VNPak*)0 || path == (const char*)0) {
        return VN_E_INVALID_ARG;
    }

    pak->path = (const char*)0;
    pak->version = 0u;
    pak->resource_count = 0u;
    pak->entries = (ResourceEntry*)0;

    fp = fopen(path, "rb");
    if (fp == (FILE*)0) {
        return VN_E_IO;
    }

    ok = (int)fread(header, 1u, VNPAK_HEADER_SIZE, fp);
    if (ok != (int)VNPAK_HEADER_SIZE) {
        (void)fclose(fp);
        return VN_E_IO;
    }

    if (header[0] != VNPAK_MAGIC_0 ||
        header[1] != VNPAK_MAGIC_1 ||
        header[2] != VNPAK_MAGIC_2 ||
        header[3] != VNPAK_MAGIC_3) {
        (void)fclose(fp);
        return VN_E_FORMAT;
    }

    version = vnpak_u16_le(header + 4);
    entry_count_16 = vnpak_u16_le(header + 6);
    entry_count = (vn_u32)entry_count_16;
    if (entry_count > VNPAK_MAX_ENTRIES) {
        (void)fclose(fp);
        return VN_E_FORMAT;
    }

    entries = (ResourceEntry*)0;
    if (entry_count > 0u) {
        entries = (ResourceEntry*)malloc(sizeof(ResourceEntry) * entry_count);
        if (entries == (ResourceEntry*)0) {
            (void)fclose(fp);
            return VN_E_NOMEM;
        }
    }

    for (i = 0u; i < entry_count; ++i) {
        vn_u8 row[VNPAK_ENTRY_SIZE];
        vn_u32 data_end;

        ok = (int)fread(row, 1u, VNPAK_ENTRY_SIZE, fp);
        if (ok != (int)VNPAK_ENTRY_SIZE) {
            free(entries);
            (void)fclose(fp);
            return VN_E_IO;
        }

        entries[i].type = row[0];
        entries[i].flags = row[1];
        entries[i].width = vnpak_u16_le(row + 2);
        entries[i].height = vnpak_u16_le(row + 4);
        entries[i].data_off = vnpak_u32_le(row + 6);
        entries[i].data_size = vnpak_u32_le(row + 10);

        data_end = entries[i].data_off + entries[i].data_size;
        if (data_end < entries[i].data_off) {
            free(entries);
            (void)fclose(fp);
            return VN_E_FORMAT;
        }
    }

    (void)fclose(fp);

    pak->path = path;
    pak->version = version;
    pak->resource_count = entry_count;
    pak->entries = entries;

    return VN_OK;
}

const ResourceEntry* vnpak_get(const VNPak* pak, vn_u32 id) {
    if (pak == (const VNPak*)0 || pak->entries == (ResourceEntry*)0) {
        return (const ResourceEntry*)0;
    }
    if (id >= pak->resource_count) {
        return (const ResourceEntry*)0;
    }
    return &pak->entries[id];
}

int vnpak_read_resource(const VNPak* pak, vn_u32 id, vn_u8* out_buf, vn_u32 out_size, vn_u32* out_read) {
    const ResourceEntry* entry;
    FILE* fp;
    long seek_rc;
    size_t got;

    if (out_read != (vn_u32*)0) {
        *out_read = 0u;
    }
    if (pak == (const VNPak*)0 || out_buf == (vn_u8*)0) {
        return VN_E_INVALID_ARG;
    }

    entry = vnpak_get(pak, id);
    if (entry == (const ResourceEntry*)0) {
        return VN_E_INVALID_ARG;
    }
    if (entry->data_size > out_size) {
        return VN_E_NOMEM;
    }
    if (pak->path == (const char*)0) {
        return VN_E_FORMAT;
    }

    fp = fopen(pak->path, "rb");
    if (fp == (FILE*)0) {
        return VN_E_IO;
    }

    seek_rc = fseek(fp, (long)entry->data_off, SEEK_SET);
    if (seek_rc != 0) {
        (void)fclose(fp);
        return VN_E_IO;
    }

    got = fread(out_buf, 1u, (size_t)entry->data_size, fp);
    (void)fclose(fp);
    if (got != (size_t)entry->data_size) {
        return VN_E_IO;
    }

    if (out_read != (vn_u32*)0) {
        *out_read = (vn_u32)got;
    }
    return VN_OK;
}

void vnpak_close(VNPak* pak) {
    if (pak == (VNPak*)0) {
        return;
    }
    if (pak->entries != (ResourceEntry*)0) {
        free(pak->entries);
    }
    pak->path = (const char*)0;
    pak->version = 0u;
    pak->resource_count = 0u;
    pak->entries = (ResourceEntry*)0;
}
