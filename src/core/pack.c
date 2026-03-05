#include <stdio.h>
#include <stdlib.h>

#include "vn_pack.h"
#include "vn_error.h"

#define VNPAK_MAGIC_0 ((vn_u8)'V')
#define VNPAK_MAGIC_1 ((vn_u8)'N')
#define VNPAK_MAGIC_2 ((vn_u8)'P')
#define VNPAK_MAGIC_3 ((vn_u8)'K')

#define VNPAK_VERSION_1 1u
#define VNPAK_VERSION_2 2u

#define VNPAK_HEADER_SIZE    8u
#define VNPAK_ENTRY_SIZE_V1 14u
#define VNPAK_ENTRY_SIZE_V2 18u
#define VNPAK_MAX_ENTRIES 4096u

static vn_u32 g_crc32_table[256];
static int g_crc32_table_ready = VN_FALSE;

static vn_u16 vnpak_u16_le(const vn_u8* p) {
    return (vn_u16)((vn_u16)p[0] | ((vn_u16)p[1] << 8));
}

static vn_u32 vnpak_u32_le(const vn_u8* p) {
    return (vn_u32)(((vn_u32)p[0]) |
                    ((vn_u32)p[1] << 8) |
                    ((vn_u32)p[2] << 16) |
                    ((vn_u32)p[3] << 24));
}

static int vnpak_add_u32(vn_u32 a, vn_u32 b, vn_u32* out_sum) {
    vn_u32 sum;

    if (out_sum == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }
    sum = a + b;
    if (sum < a) {
        return VN_E_FORMAT;
    }
    *out_sum = sum;
    return VN_OK;
}

static int vnpak_file_size(FILE* fp, vn_u32* out_size) {
    long cur;
    long end;

    if (fp == (FILE*)0 || out_size == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }

    cur = ftell(fp);
    if (cur < 0) {
        return VN_E_IO;
    }
    if (fseek(fp, 0L, SEEK_END) != 0) {
        return VN_E_IO;
    }
    end = ftell(fp);
    if (end < 0) {
        return VN_E_IO;
    }
    if ((unsigned long)end > 0xFFFFFFFFul) {
        return VN_E_FORMAT;
    }
    if (fseek(fp, cur, SEEK_SET) != 0) {
        return VN_E_IO;
    }
    *out_size = (vn_u32)end;
    return VN_OK;
}

static void vnpak_crc32_table_init(void) {
    vn_u32 i;

    if (g_crc32_table_ready != VN_FALSE) {
        return;
    }
    for (i = 0u; i < 256u; ++i) {
        vn_u32 c;
        vn_u32 j;

        c = i;
        for (j = 0u; j < 8u; ++j) {
            if ((c & 1u) != 0u) {
                c = (c >> 1) ^ 0xEDB88320u;
            } else {
                c >>= 1;
            }
        }
        g_crc32_table[i] = c;
    }
    g_crc32_table_ready = VN_TRUE;
}

static vn_u32 vnpak_crc32(const vn_u8* data, vn_u32 size) {
    vn_u32 crc;
    vn_u32 i;

    if (data == (const vn_u8*)0 || size == 0u) {
        return 0u;
    }

    vnpak_crc32_table_init();
    crc = 0xFFFFFFFFu;
    for (i = 0u; i < size; ++i) {
        vn_u32 idx;
        idx = (vn_u32)((crc ^ (vn_u32)data[i]) & 0xFFu);
        crc = g_crc32_table[idx] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

int vnpak_open(VNPak* pak, const char* path) {
    FILE* fp;
    vn_u8 header[VNPAK_HEADER_SIZE];
    vn_u16 version_16;
    vn_u16 entry_count_16;
    vn_u32 version;
    vn_u32 entry_count;
    vn_u32 entry_size;
    vn_u32 table_size;
    vn_u32 data_start;
    vn_u32 file_size;
    ResourceEntry* entries;
    vn_u32 i;
    int ok;
    int rc;

    if (pak == (VNPak*)0 || path == (const char*)0) {
        return VN_E_INVALID_ARG;
    }

    pak->path = (const char*)0;
    pak->version = 0u;
    pak->resource_count = 0u;
    pak->header_size = 0u;
    pak->entry_size = 0u;
    pak->file_size = 0u;
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

    version_16 = vnpak_u16_le(header + 4);
    entry_count_16 = vnpak_u16_le(header + 6);
    version = (vn_u32)version_16;
    entry_count = (vn_u32)entry_count_16;
    if (entry_count > VNPAK_MAX_ENTRIES) {
        (void)fclose(fp);
        return VN_E_FORMAT;
    }
    if (version == VNPAK_VERSION_1) {
        entry_size = VNPAK_ENTRY_SIZE_V1;
    } else if (version == VNPAK_VERSION_2) {
        entry_size = VNPAK_ENTRY_SIZE_V2;
    } else {
        (void)fclose(fp);
        return VN_E_FORMAT;
    }

    if (entry_count > 0u && entry_size > (0xFFFFFFFFu / entry_count)) {
        (void)fclose(fp);
        return VN_E_FORMAT;
    }
    table_size = entry_count * entry_size;
    rc = vnpak_add_u32(VNPAK_HEADER_SIZE, table_size, &data_start);
    if (rc != VN_OK) {
        (void)fclose(fp);
        return rc;
    }

    rc = vnpak_file_size(fp, &file_size);
    if (rc != VN_OK) {
        (void)fclose(fp);
        return rc;
    }
    if (file_size < data_start) {
        (void)fclose(fp);
        return VN_E_FORMAT;
    }

    if (fseek(fp, (long)VNPAK_HEADER_SIZE, SEEK_SET) != 0) {
        (void)fclose(fp);
        return VN_E_IO;
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
        vn_u8 row[VNPAK_ENTRY_SIZE_V2];
        vn_u32 data_end;

        ok = (int)fread(row, 1u, entry_size, fp);
        if (ok != (int)entry_size) {
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
        if (entry_size >= VNPAK_ENTRY_SIZE_V2) {
            entries[i].crc32 = vnpak_u32_le(row + 14);
        } else {
            entries[i].crc32 = 0u;
        }

        if (entries[i].data_off < data_start) {
            free(entries);
            (void)fclose(fp);
            return VN_E_FORMAT;
        }
        rc = vnpak_add_u32(entries[i].data_off, entries[i].data_size, &data_end);
        if (rc != VN_OK) {
            free(entries);
            (void)fclose(fp);
            return rc;
        }
        if (data_end > file_size) {
            free(entries);
            (void)fclose(fp);
            return VN_E_FORMAT;
        }
    }

    for (i = 0u; i < entry_count; ++i) {
        vn_u32 start_i;
        vn_u32 end_i;
        vn_u32 j;

        if (entries[i].data_size == 0u) {
            continue;
        }
        start_i = entries[i].data_off;
        end_i = start_i + entries[i].data_size;
        for (j = i + 1u; j < entry_count; ++j) {
            vn_u32 start_j;
            vn_u32 end_j;

            if (entries[j].data_size == 0u) {
                continue;
            }
            start_j = entries[j].data_off;
            end_j = start_j + entries[j].data_size;
            if (start_i < end_j && start_j < end_i) {
                free(entries);
                (void)fclose(fp);
                return VN_E_FORMAT;
            }
        }
    }

    (void)fclose(fp);

    pak->path = path;
    pak->version = version_16;
    pak->resource_count = entry_count;
    pak->header_size = VNPAK_HEADER_SIZE;
    pak->entry_size = entry_size;
    pak->file_size = file_size;
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

    if (pak->version >= VNPAK_VERSION_2 && entry->data_size > 0u) {
        vn_u32 actual_crc;
        actual_crc = vnpak_crc32(out_buf, entry->data_size);
        if (actual_crc != entry->crc32) {
            return VN_E_FORMAT;
        }
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
    pak->header_size = 0u;
    pak->entry_size = 0u;
    pak->file_size = 0u;
    pak->entries = (ResourceEntry*)0;
}
