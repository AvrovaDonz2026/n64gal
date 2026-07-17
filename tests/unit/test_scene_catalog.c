#include <stdio.h>

#include "vn_error.h"
#include "../../src/core/scene_catalog.h"

static int write_embedded_nul_catalog_pack(const char* path) {
    static const unsigned char blob[] = {
        0x56, 0x4e, 0x50, 0x4b, 0x01, 0x00, 0x02, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x25, 0x00, 0x00, 0x00,
        0x17, 0x00, 0x00, 0x00,
        0x00,
        0x56, 0x4e, 0x53, 0x43, 0x01, 0x00, 0x01, 0x00,
        0x78, 0x56, 0x34, 0x12,
        0x78, 0x56, 0x34, 0x12, 0x00, 0x00, 0x03, 0x00,
        0x41, 0x00, 0x42
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
    const char* embedded_nul_path;
    VNPak pak;
    VNSceneCatalog catalog;
    const VNSceneCatalogEntry* opening;
    const VNSceneCatalogEntry* gallery;
    char long_name[65];
    vn_u32 i;
    int rc;

    embedded_nul_path = "test_scene_catalog_embedded_nul.tmp.vnpak";

    for (i = 0u; i < 64u; ++i) {
        long_name[i] = 'A';
    }
    long_name[64] = '\0';
    if (vn_scene_name_is_valid("Opening") == VN_FALSE ||
        vn_scene_name_is_valid("Scene_1-a") == VN_FALSE ||
        vn_scene_name_is_valid("") != VN_FALSE ||
        vn_scene_name_is_valid("1Scene") != VN_FALSE ||
        vn_scene_name_is_valid("bad/name") != VN_FALSE ||
        vn_scene_name_is_valid(long_name) != VN_FALSE) {
        (void)fprintf(stderr, "scene name validation mismatch\n");
        return 1;
    }

    rc = vnpak_open(&pak, "assets/demo/content-demo.vnpak");
    if (rc != VN_OK) {
        (void)fprintf(stderr, "content pack open failed rc=%d\n", rc);
        return 1;
    }
    vn_scene_catalog_init(&catalog);
    rc = vn_scene_catalog_load(&catalog, &pak);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "scene catalog load failed rc=%d\n", rc);
        vnpak_close(&pak);
        return 1;
    }
    opening = vn_scene_catalog_find_name(&catalog, "Opening");
    gallery = vn_scene_catalog_find_name(&catalog, "Gallery");
    if (catalog.present == VN_FALSE || catalog.count != 2u ||
        opening == (const VNSceneCatalogEntry*)0 ||
        gallery == (const VNSceneCatalogEntry*)0 ||
        opening->scene_id != 0x015825B7u || opening->script_resource_id != 0u ||
        gallery->scene_id != 0xD7B54CDDu || gallery->script_resource_id != 1u ||
        catalog.entry_scene_id != opening->scene_id) {
        (void)fprintf(stderr, "scene catalog contents mismatch\n");
        vnpak_close(&pak);
        return 1;
    }
    vnpak_close(&pak);

    (void)remove(embedded_nul_path);
    if (write_embedded_nul_catalog_pack(embedded_nul_path) != 0) {
        (void)fprintf(stderr, "embedded NUL catalog pack write failed\n");
        return 1;
    }
    rc = vnpak_open(&pak, embedded_nul_path);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "embedded NUL catalog pack open failed rc=%d\n", rc);
        (void)remove(embedded_nul_path);
        return 1;
    }
    vn_scene_catalog_init(&catalog);
    rc = vn_scene_catalog_load(&catalog, &pak);
    vnpak_close(&pak);
    (void)remove(embedded_nul_path);
    if (rc != VN_E_FORMAT || catalog.present != VN_FALSE || catalog.count != 0u) {
        (void)fprintf(stderr, "embedded NUL scene name should be rejected rc=%d\n", rc);
        return 1;
    }

    (void)printf("test_scene_catalog ok\n");
    return 0;
}
