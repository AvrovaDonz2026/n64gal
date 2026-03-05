#include <stdio.h>
#include <string.h>

#include "vn_pack.h"
#include "vn_error.h"

static int write_demo_pack(const char* path) {
    unsigned char blob[] = {
        0x56, 0x4e, 0x50, 0x4b, 0x01, 0x00, 0x02, 0x00,
        0x01, 0x00, 0x58, 0x02, 0x20, 0x03, 0x24, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x02, 0x01, 0x80, 0x02, 0xe0, 0x01, 0x28, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x11, 0x22, 0x33,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x10, 0x20
    };
    FILE* fp;
    size_t wrote;

    fp = fopen(path, "wb");
    if (fp == (FILE*)0) {
        return 1;
    }
    wrote = fwrite(blob, 1u, sizeof(blob), fp);
    (void)fclose(fp);

    return (wrote == sizeof(blob)) ? 0 : 1;
}

int main(void) {
    const char* path;
    VNPak pak;
    const ResourceEntry* e0;
    const ResourceEntry* e1;
    vn_u8 buf[8];
    vn_u32 read_count;
    int rc;

    path = "/tmp/test_demo.vnpak";
    if (write_demo_pack(path) != 0) {
        (void)fprintf(stderr, "failed writing demo pack\n");
        return 1;
    }

    pak.path = (const char*)0;
    pak.version = 0u;
    pak.resource_count = 0u;
    pak.entries = (ResourceEntry*)0;

    rc = vnpak_open(&pak, path);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vnpak_open failed rc=%d\n", rc);
        return 1;
    }

    if (pak.version != 1u || pak.resource_count != 2u) {
        (void)fprintf(stderr, "header mismatch version=%u count=%u\n",
                      (unsigned int)pak.version,
                      (unsigned int)pak.resource_count);
        vnpak_close(&pak);
        return 1;
    }

    e0 = vnpak_get(&pak, 0u);
    e1 = vnpak_get(&pak, 1u);
    if (e0 == (const ResourceEntry*)0 || e1 == (const ResourceEntry*)0) {
        (void)fprintf(stderr, "entries missing\n");
        vnpak_close(&pak);
        return 1;
    }

    if (e0->width != 600u || e0->height != 800u || e0->data_off != 36u || e0->data_size != 4u) {
        (void)fprintf(stderr, "entry0 mismatch\n");
        vnpak_close(&pak);
        return 1;
    }
    if (e1->flags != 1u || e1->width != 640u || e1->height != 480u || e1->data_off != 40u || e1->data_size != 8u) {
        (void)fprintf(stderr, "entry1 mismatch\n");
        vnpak_close(&pak);
        return 1;
    }

    if (vnpak_get(&pak, 2u) != (const ResourceEntry*)0) {
        (void)fprintf(stderr, "expected null for out-of-range id\n");
        vnpak_close(&pak);
        return 1;
    }

    rc = vnpak_read_resource(&pak, 0u, buf, 3u, &read_count);
    if (rc != VN_E_NOMEM) {
        (void)fprintf(stderr, "expected VN_E_NOMEM for small buffer rc=%d\n", rc);
        vnpak_close(&pak);
        return 1;
    }

    rc = vnpak_read_resource(&pak, 0u, buf, 8u, &read_count);
    if (rc != VN_OK || read_count != 4u) {
        (void)fprintf(stderr, "read entry0 failed rc=%d read=%u\n", rc, (unsigned int)read_count);
        vnpak_close(&pak);
        return 1;
    }
    if (buf[0] != 0x00u || buf[1] != 0x11u || buf[2] != 0x22u || buf[3] != 0x33u) {
        (void)fprintf(stderr, "entry0 payload mismatch\n");
        vnpak_close(&pak);
        return 1;
    }

    rc = vnpak_read_resource(&pak, 1u, buf, 8u, &read_count);
    if (rc != VN_OK || read_count != 8u) {
        (void)fprintf(stderr, "read entry1 failed rc=%d read=%u\n", rc, (unsigned int)read_count);
        vnpak_close(&pak);
        return 1;
    }
    if (buf[0] != 0xaau || buf[1] != 0xbbu || buf[7] != 0x20u) {
        (void)fprintf(stderr, "entry1 payload mismatch\n");
        vnpak_close(&pak);
        return 1;
    }

    vnpak_close(&pak);

    (void)printf("test_vnpak ok\n");
    return 0;
}
