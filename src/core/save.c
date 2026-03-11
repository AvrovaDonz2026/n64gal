#include <stdio.h>
#include <stdlib.h>

#include "vn_save.h"
#include "vn_error.h"
#include "platform.h"

#define VNSAVE_MAGIC_0 ((vn_u8)'V')
#define VNSAVE_MAGIC_1 ((vn_u8)'N')
#define VNSAVE_MAGIC_2 ((vn_u8)'S')
#define VNSAVE_MAGIC_V0 ((vn_u8)'0')
#define VNSAVE_MAGIC_V1 ((vn_u8)'V')

static vn_u32 g_vnsave_crc32_table[256];
static int g_vnsave_crc32_ready = VN_FALSE;

static void vnsave_u32_le_write(vn_u8* p, vn_u32 value) {
    p[0] = (vn_u8)(value & 0xFFu);
    p[1] = (vn_u8)((value >> 8) & 0xFFu);
    p[2] = (vn_u8)((value >> 16) & 0xFFu);
    p[3] = (vn_u8)((value >> 24) & 0xFFu);
}

static vn_u32 vnsave_u32_le(const vn_u8* p) {
    return (vn_u32)(((vn_u32)p[0]) |
                    ((vn_u32)p[1] << 8) |
                    ((vn_u32)p[2] << 16) |
                    ((vn_u32)p[3] << 24));
}

static int vnsave_file_size(FILE* fp, vn_u32* out_size) {
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

static void vnsave_crc32_table_init(void) {
    vn_u32 i;

    if (g_vnsave_crc32_ready != VN_FALSE) {
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
        g_vnsave_crc32_table[i] = c;
    }
    g_vnsave_crc32_ready = VN_TRUE;
}

static vn_u32 vnsave_crc32(const vn_u8* data, vn_u32 size) {
    vn_u32 crc;
    vn_u32 i;

    if (data == (const vn_u8*)0 || size == 0u) {
        return 0u;
    }

    vnsave_crc32_table_init();
    crc = 0xFFFFFFFFu;
    for (i = 0u; i < size; ++i) {
        vn_u32 idx;
        idx = (vn_u32)((crc ^ (vn_u32)data[i]) & 0xFFu);
        crc = g_vnsave_crc32_table[idx] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

static void vnsave_probe_init(VNSaveProbe* probe, const char* path) {
    if (probe == (VNSaveProbe*)0) {
        return;
    }
    probe->path = path;
    probe->version = 0u;
    probe->header_size = 0u;
    probe->payload_size = 0u;
    probe->slot_id = 0u;
    probe->script_pc = 0u;
    probe->scene_id = 0u;
    probe->timestamp_s = 0u;
    probe->payload_crc32 = 0u;
    probe->status = VNSAVE_STATUS_INVALID_HEADER;
    probe->error_code = VN_E_FORMAT;
}

int vnsave_probe_file(const char* path, VNSaveProbe* out_probe) {
    FILE* fp;
    vn_u8 header[VNSAVE_HEADER_SIZE_V1];
    vn_u32 file_size;
    int got;
    int rc;

    if (path == (const char*)0 || out_probe == (VNSaveProbe*)0) {
        return VN_E_INVALID_ARG;
    }

    vnsave_probe_init(out_probe, path);

    fp = vn_platform_fopen_read_binary(path);
    if (fp == (FILE*)0) {
        out_probe->error_code = VN_E_IO;
        return VN_E_IO;
    }

    rc = vnsave_file_size(fp, &file_size);
    if (rc != VN_OK) {
        out_probe->error_code = rc;
        (void)fclose(fp);
        return rc;
    }
    if (file_size < VNSAVE_HEADER_SIZE_V0) {
        out_probe->status = VNSAVE_STATUS_TRUNCATED;
        out_probe->error_code = VN_E_FORMAT;
        (void)fclose(fp);
        return VN_E_FORMAT;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        out_probe->error_code = VN_E_IO;
        (void)fclose(fp);
        return VN_E_IO;
    }
    got = (int)fread(header, 1u, VNSAVE_HEADER_SIZE_V0, fp);
    if (got != (int)VNSAVE_HEADER_SIZE_V0) {
        out_probe->status = VNSAVE_STATUS_TRUNCATED;
        out_probe->error_code = VN_E_FORMAT;
        (void)fclose(fp);
        return VN_E_FORMAT;
    }

    if (header[0] != VNSAVE_MAGIC_0 ||
        header[1] != VNSAVE_MAGIC_1 ||
        header[2] != VNSAVE_MAGIC_2) {
        out_probe->status = VNSAVE_STATUS_BAD_MAGIC;
        out_probe->error_code = VN_E_FORMAT;
        (void)fclose(fp);
        return VN_E_FORMAT;
    }

    if (header[3] == VNSAVE_MAGIC_V0) {
        out_probe->version = 0u;
        out_probe->header_size = VNSAVE_HEADER_SIZE_V0;
        out_probe->slot_id = vnsave_u32_le(header + 4);
        out_probe->script_pc = vnsave_u32_le(header + 8);
        out_probe->scene_id = vnsave_u32_le(header + 12);
        out_probe->payload_size = file_size - VNSAVE_HEADER_SIZE_V0;
        out_probe->status = VNSAVE_STATUS_PRE_1_0;
        out_probe->error_code = VN_E_UNSUPPORTED;
        (void)fclose(fp);
        return VN_E_UNSUPPORTED;
    }

    if (header[3] != VNSAVE_MAGIC_V1) {
        out_probe->status = VNSAVE_STATUS_BAD_MAGIC;
        out_probe->error_code = VN_E_FORMAT;
        (void)fclose(fp);
        return VN_E_FORMAT;
    }

    if (file_size < VNSAVE_HEADER_SIZE_V1) {
        out_probe->status = VNSAVE_STATUS_TRUNCATED;
        out_probe->error_code = VN_E_FORMAT;
        (void)fclose(fp);
        return VN_E_FORMAT;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        out_probe->error_code = VN_E_IO;
        (void)fclose(fp);
        return VN_E_IO;
    }
    got = (int)fread(header, 1u, VNSAVE_HEADER_SIZE_V1, fp);
    if (got != (int)VNSAVE_HEADER_SIZE_V1) {
        out_probe->status = VNSAVE_STATUS_TRUNCATED;
        out_probe->error_code = VN_E_FORMAT;
        (void)fclose(fp);
        return VN_E_FORMAT;
    }

    out_probe->version = vnsave_u32_le(header + 4);
    out_probe->header_size = VNSAVE_HEADER_SIZE_V1;
    out_probe->slot_id = vnsave_u32_le(header + 8);
    out_probe->script_pc = vnsave_u32_le(header + 12);
    out_probe->scene_id = vnsave_u32_le(header + 16);
    out_probe->timestamp_s = vnsave_u32_le(header + 20);
    out_probe->payload_crc32 = vnsave_u32_le(header + 24);
    out_probe->payload_size = file_size - VNSAVE_HEADER_SIZE_V1;

    if (out_probe->version < VNSAVE_VERSION_1) {
        out_probe->status = VNSAVE_STATUS_PRE_1_0;
        out_probe->error_code = VN_E_UNSUPPORTED;
        (void)fclose(fp);
        return VN_E_UNSUPPORTED;
    }
    if (out_probe->version > VNSAVE_VERSION_1) {
        out_probe->status = VNSAVE_STATUS_NEWER_VERSION;
        out_probe->error_code = VN_E_UNSUPPORTED;
        (void)fclose(fp);
        return VN_E_UNSUPPORTED;
    }
    if (vnsave_u32_le(header + 28) != 0u) {
        out_probe->status = VNSAVE_STATUS_INVALID_HEADER;
        out_probe->error_code = VN_E_FORMAT;
        (void)fclose(fp);
        return VN_E_FORMAT;
    }
    if (out_probe->payload_size > 0u) {
        vn_u8* payload;
        vn_u32 actual_crc;

        payload = (vn_u8*)malloc((size_t)out_probe->payload_size);
        if (payload == (vn_u8*)0) {
            out_probe->error_code = VN_E_NOMEM;
            (void)fclose(fp);
            return VN_E_NOMEM;
        }
        if ((int)fread(payload, 1u, out_probe->payload_size, fp) != (int)out_probe->payload_size) {
            free(payload);
            out_probe->status = VNSAVE_STATUS_INVALID_HEADER;
            out_probe->error_code = VN_E_IO;
            (void)fclose(fp);
            return VN_E_IO;
        }
        actual_crc = vnsave_crc32(payload, out_probe->payload_size);
        free(payload);
        if (actual_crc != out_probe->payload_crc32) {
            out_probe->status = VNSAVE_STATUS_INVALID_HEADER;
            out_probe->error_code = VN_E_FORMAT;
            (void)fclose(fp);
            return VN_E_FORMAT;
        }
    } else if (out_probe->payload_crc32 != 0u) {
        out_probe->status = VNSAVE_STATUS_INVALID_HEADER;
        out_probe->error_code = VN_E_FORMAT;
        (void)fclose(fp);
        return VN_E_FORMAT;
    }

    out_probe->status = VNSAVE_STATUS_OK;
    out_probe->error_code = VN_OK;
    (void)fclose(fp);
    return VN_OK;
}

int vnsave_migrate_v0_to_v1_file(const char* in_path, const char* out_path) {
    FILE* in_fp;
    FILE* out_fp;
    VNSaveProbe probe;
    vn_u8 header[VNSAVE_HEADER_SIZE_V1];
    vn_u8* payload;
    vn_u32 crc;
    int rc;

    if (in_path == (const char*)0 || out_path == (const char*)0) {
        return VN_E_INVALID_ARG;
    }

    rc = vnsave_probe_file(in_path, &probe);
    if (rc == VN_E_UNSUPPORTED && probe.status == VNSAVE_STATUS_PRE_1_0) {
        /* Expected legacy input, continue. */
    } else if (rc != VN_OK) {
        return rc;
    } else {
        return VN_E_UNSUPPORTED;
    }

    in_fp = vn_platform_fopen_read_binary(in_path);
    if (in_fp == (FILE*)0) {
        return VN_E_IO;
    }
    if (fseek(in_fp, (long)VNSAVE_HEADER_SIZE_V0, SEEK_SET) != 0) {
        (void)fclose(in_fp);
        return VN_E_IO;
    }

    payload = (vn_u8*)0;
    if (probe.payload_size > 0u) {
        payload = (vn_u8*)malloc((size_t)probe.payload_size);
        if (payload == (vn_u8*)0) {
            (void)fclose(in_fp);
            return VN_E_NOMEM;
        }
        if ((int)fread(payload, 1u, probe.payload_size, in_fp) != (int)probe.payload_size) {
            free(payload);
            (void)fclose(in_fp);
            return VN_E_IO;
        }
    }
    (void)fclose(in_fp);

    crc = vnsave_crc32(payload, probe.payload_size);
    header[0] = VNSAVE_MAGIC_0;
    header[1] = VNSAVE_MAGIC_1;
    header[2] = VNSAVE_MAGIC_2;
    header[3] = VNSAVE_MAGIC_V1;
    vnsave_u32_le_write(header + 4, VNSAVE_VERSION_1);
    vnsave_u32_le_write(header + 8, probe.slot_id);
    vnsave_u32_le_write(header + 12, probe.script_pc);
    vnsave_u32_le_write(header + 16, probe.scene_id);
    vnsave_u32_le_write(header + 20, 0u);
    vnsave_u32_le_write(header + 24, crc);
    vnsave_u32_le_write(header + 28, 0u);

    out_fp = fopen(out_path, "wb");
    if (out_fp == (FILE*)0) {
        free(payload);
        return VN_E_IO;
    }
    if (fwrite(header, 1u, VNSAVE_HEADER_SIZE_V1, out_fp) != (size_t)VNSAVE_HEADER_SIZE_V1) {
        free(payload);
        (void)fclose(out_fp);
        return VN_E_IO;
    }
    if (probe.payload_size > 0u) {
        if (fwrite(payload, 1u, probe.payload_size, out_fp) != (size_t)probe.payload_size) {
            free(payload);
            (void)fclose(out_fp);
            return VN_E_IO;
        }
    }
    free(payload);
    if (fclose(out_fp) != 0) {
        return VN_E_IO;
    }
    return VN_OK;
}

const char* vnsave_status_name(vn_u32 status) {
    if (status == VNSAVE_STATUS_OK) {
        return "VNSAVE_STATUS_OK";
    }
    if (status == VNSAVE_STATUS_BAD_MAGIC) {
        return "VNSAVE_STATUS_BAD_MAGIC";
    }
    if (status == VNSAVE_STATUS_TRUNCATED) {
        return "VNSAVE_STATUS_TRUNCATED";
    }
    if (status == VNSAVE_STATUS_PRE_1_0) {
        return "VNSAVE_STATUS_PRE_1_0";
    }
    if (status == VNSAVE_STATUS_NEWER_VERSION) {
        return "VNSAVE_STATUS_NEWER_VERSION";
    }
    if (status == VNSAVE_STATUS_INVALID_HEADER) {
        return "VNSAVE_STATUS_INVALID_HEADER";
    }
    return "VNSAVE_STATUS_UNKNOWN";
}
