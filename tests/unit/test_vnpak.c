#include <stdio.h>
#include <string.h>

#include "vn_pack.h"
#include "vn_error.h"

static int write_blob(const char* path, const unsigned char* blob, vn_u32 blob_size) {
    FILE* fp;
    size_t wrote;

    fp = fopen(path, "wb");
    if (fp == (FILE*)0) {
        return 1;
    }
    wrote = fwrite(blob, 1u, (size_t)blob_size, fp);
    (void)fclose(fp);
    return (wrote == (size_t)blob_size) ? 0 : 1;
}

static int write_demo_pack_v1(const char* path) {
    static const unsigned char blob[] = {
        0x56, 0x4e, 0x50, 0x4b, 0x01, 0x00, 0x02, 0x00,
        0x01, 0x00, 0x58, 0x02, 0x20, 0x03, 0x24, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x02, 0x01, 0x80, 0x02, 0xe0, 0x01, 0x28, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x11, 0x22, 0x33,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x10, 0x20
    };
    return write_blob(path, blob, (vn_u32)sizeof(blob));
}

static int write_demo_pack_v2(const char* path) {
    static const unsigned char blob[] = {
        0x56, 0x4e, 0x50, 0x4b, 0x02, 0x00, 0x02, 0x00,
        0x01, 0x00, 0x58, 0x02, 0x20, 0x03, 0x2c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x6d, 0x31, 0xc2, 0x24,
        0x02, 0x01, 0x80, 0x02, 0xe0, 0x01, 0x30, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x2f, 0x6a, 0x06, 0x92,
        0x00, 0x11, 0x22, 0x33,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x10, 0x20
    };
    return write_blob(path, blob, (vn_u32)sizeof(blob));
}

static int write_overlap_pack_v2(const char* path) {
    static const unsigned char blob[] = {
        0x56, 0x4e, 0x50, 0x4b, 0x02, 0x00, 0x02, 0x00,
        0x01, 0x00, 0x58, 0x02, 0x20, 0x03, 0x2c, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x01, 0x80, 0x02, 0xe0, 0x01, 0x30, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x10, 0x20
    };
    return write_blob(path, blob, (vn_u32)sizeof(blob));
}

static int flip_one_byte(const char* path, long offset) {
    FILE* fp;
    int ch;

    fp = fopen(path, "rb+");
    if (fp == (FILE*)0) {
        return 1;
    }
    if (fseek(fp, offset, SEEK_SET) != 0) {
        (void)fclose(fp);
        return 1;
    }
    ch = fgetc(fp);
    if (ch == EOF) {
        (void)fclose(fp);
        return 1;
    }
    if (fseek(fp, offset, SEEK_SET) != 0) {
        (void)fclose(fp);
        return 1;
    }
    if (fputc((ch ^ 0xFF) & 0xFF, fp) == EOF) {
        (void)fclose(fp);
        return 1;
    }
    (void)fclose(fp);
    return 0;
}

static int verify_read(const VNPak* pak, vn_u32 id, vn_u32 expected_size, const vn_u8* expected) {
    vn_u8 buf[16];
    vn_u32 read_count;
    int rc;

    rc = vnpak_read_resource(pak, id, buf, (vn_u32)sizeof(buf), &read_count);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "read failed id=%u rc=%d\n", (unsigned int)id, rc);
        return 1;
    }
    if (read_count != expected_size) {
        (void)fprintf(stderr, "read size mismatch id=%u read=%u expect=%u\n",
                      (unsigned int)id, (unsigned int)read_count, (unsigned int)expected_size);
        return 1;
    }
    if (memcmp(buf, expected, (size_t)expected_size) != 0) {
        (void)fprintf(stderr, "payload mismatch id=%u\n", (unsigned int)id);
        return 1;
    }
    return 0;
}

int main(void) {
    const char* path_v1;
    const char* path_v2;
    const char* path_overlap;
    VNPak pak;
    const ResourceEntry* e0;
    const ResourceEntry* e1;
    vn_u8 expected0[4];
    vn_u8 expected1[8];
    vn_u32 read_count;
    int rc;

    path_v1 = "/tmp/test_demo_v1.vnpak";
    path_v2 = "/tmp/test_demo_v2.vnpak";
    path_overlap = "/tmp/test_demo_overlap.vnpak";
    expected0[0] = 0x00u;
    expected0[1] = 0x11u;
    expected0[2] = 0x22u;
    expected0[3] = 0x33u;
    expected1[0] = 0xaau;
    expected1[1] = 0xbbu;
    expected1[2] = 0xccu;
    expected1[3] = 0xddu;
    expected1[4] = 0xeeu;
    expected1[5] = 0xffu;
    expected1[6] = 0x10u;
    expected1[7] = 0x20u;

    if (write_demo_pack_v1(path_v1) != 0) {
        (void)fprintf(stderr, "failed writing v1 pack\n");
        return 1;
    }
    if (write_demo_pack_v2(path_v2) != 0) {
        (void)fprintf(stderr, "failed writing v2 pack\n");
        return 1;
    }
    if (write_overlap_pack_v2(path_overlap) != 0) {
        (void)fprintf(stderr, "failed writing overlap pack\n");
        return 1;
    }

    pak.path = (const char*)0;
    pak.version = 0u;
    pak.resource_count = 0u;
    pak.header_size = 0u;
    pak.entry_size = 0u;
    pak.file_size = 0u;
    pak.entries = (ResourceEntry*)0;

    rc = vnpak_open(&pak, path_v1);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "v1 open failed rc=%d\n", rc);
        return 1;
    }
    if (pak.version != 1u || pak.resource_count != 2u || pak.entry_size != 14u) {
        (void)fprintf(stderr, "v1 meta mismatch version=%u count=%u entry=%u\n",
                      (unsigned int)pak.version,
                      (unsigned int)pak.resource_count,
                      (unsigned int)pak.entry_size);
        vnpak_close(&pak);
        return 1;
    }
    e0 = vnpak_get(&pak, 0u);
    e1 = vnpak_get(&pak, 1u);
    if (e0 == (const ResourceEntry*)0 || e1 == (const ResourceEntry*)0) {
        (void)fprintf(stderr, "v1 missing entries\n");
        vnpak_close(&pak);
        return 1;
    }
    if (e0->crc32 != 0u || e1->crc32 != 0u) {
        (void)fprintf(stderr, "v1 crc should be zero\n");
        vnpak_close(&pak);
        return 1;
    }
    if (verify_read(&pak, 0u, 4u, expected0) != 0 || verify_read(&pak, 1u, 8u, expected1) != 0) {
        vnpak_close(&pak);
        return 1;
    }
    rc = vnpak_read_resource(&pak, 0u, expected1, 3u, &read_count);
    if (rc != VN_E_NOMEM) {
        (void)fprintf(stderr, "expected VN_E_NOMEM for small buffer rc=%d\n", rc);
        vnpak_close(&pak);
        return 1;
    }
    vnpak_close(&pak);

    rc = vnpak_open(&pak, path_v2);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "v2 open failed rc=%d\n", rc);
        return 1;
    }
    if (pak.version != 2u || pak.resource_count != 2u || pak.entry_size != 18u || pak.header_size != 8u) {
        (void)fprintf(stderr, "v2 meta mismatch version=%u count=%u entry=%u header=%u\n",
                      (unsigned int)pak.version,
                      (unsigned int)pak.resource_count,
                      (unsigned int)pak.entry_size,
                      (unsigned int)pak.header_size);
        vnpak_close(&pak);
        return 1;
    }
    e0 = vnpak_get(&pak, 0u);
    e1 = vnpak_get(&pak, 1u);
    if (e0 == (const ResourceEntry*)0 || e1 == (const ResourceEntry*)0) {
        (void)fprintf(stderr, "v2 missing entries\n");
        vnpak_close(&pak);
        return 1;
    }
    if (e0->crc32 != 0x24c2316du || e1->crc32 != 0x92066a2fu) {
        (void)fprintf(stderr, "v2 crc mismatch crc0=%08x crc1=%08x\n", e0->crc32, e1->crc32);
        vnpak_close(&pak);
        return 1;
    }
    if (verify_read(&pak, 0u, 4u, expected0) != 0 || verify_read(&pak, 1u, 8u, expected1) != 0) {
        vnpak_close(&pak);
        return 1;
    }
    vnpak_close(&pak);

    if (flip_one_byte(path_v2, 49L) != 0) {
        (void)fprintf(stderr, "failed to corrupt v2 payload\n");
        return 1;
    }
    rc = vnpak_open(&pak, path_v2);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "open after corruption failed rc=%d\n", rc);
        return 1;
    }
    rc = vnpak_read_resource(&pak, 1u, expected1, 8u, &read_count);
    if (rc != VN_E_FORMAT) {
        (void)fprintf(stderr, "expected VN_E_FORMAT on crc mismatch rc=%d\n", rc);
        vnpak_close(&pak);
        return 1;
    }
    vnpak_close(&pak);

    rc = vnpak_open(&pak, path_overlap);
    if (rc != VN_E_FORMAT) {
        (void)fprintf(stderr, "expected VN_E_FORMAT for overlap pack rc=%d\n", rc);
        return 1;
    }

    (void)printf("test_vnpak ok\n");
    return 0;
}
