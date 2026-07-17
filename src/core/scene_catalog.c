#include <stdlib.h>
#include <string.h>

#include "scene_catalog.h"
#include "vn_error.h"

#define VN_SCENE_CATALOG_HEADER_SIZE 12u
#define VN_SCENE_CATALOG_ENTRY_HEADER_SIZE 8u

static vn_u16 scene_catalog_u16_le(const vn_u8* p) {
    return (vn_u16)((vn_u16)p[0] | ((vn_u16)p[1] << 8));
}

static vn_u32 scene_catalog_u32_le(const vn_u8* p) {
    return (vn_u32)((vn_u32)p[0] |
                    ((vn_u32)p[1] << 8) |
                    ((vn_u32)p[2] << 16) |
                    ((vn_u32)p[3] << 24));
}

void vn_scene_catalog_init(VNSceneCatalog* catalog) {
    if (catalog == (VNSceneCatalog*)0) {
        return;
    }
    (void)memset(catalog, 0, sizeof(*catalog));
}

int vn_scene_name_bytes_are_valid(const vn_u8* name, vn_u32 name_len) {
    vn_u32 i;
    unsigned char c;

    if (name == (const vn_u8*)0 || name_len == 0u ||
        name_len >= VN_SCENE_NAME_MAX) {
        return VN_FALSE;
    }
    c = (unsigned char)name[0];
    if (!((c >= (unsigned char)'A' && c <= (unsigned char)'Z') ||
          (c >= (unsigned char)'a' && c <= (unsigned char)'z'))) {
        return VN_FALSE;
    }
    for (i = 1u; i < name_len; ++i) {
        c = (unsigned char)name[i];
        if (!((c >= (unsigned char)'A' && c <= (unsigned char)'Z') ||
              (c >= (unsigned char)'a' && c <= (unsigned char)'z') ||
              (c >= (unsigned char)'0' && c <= (unsigned char)'9') ||
              c == (unsigned char)'_' || c == (unsigned char)'-')) {
            return VN_FALSE;
        }
    }
    return VN_TRUE;
}

int vn_scene_name_is_valid(const char* name) {
    vn_u32 name_len;

    if (name == (const char*)0) {
        return VN_FALSE;
    }
    for (name_len = 0u; name_len < VN_SCENE_NAME_MAX; ++name_len) {
        if (name[name_len] == '\0') {
            return vn_scene_name_bytes_are_valid((const vn_u8*)name, name_len);
        }
    }
    return VN_FALSE;
}

static int scene_catalog_validate_entry(const VNSceneCatalog* catalog,
                                        const VNPak* pak,
                                        const VNSceneCatalogEntry* entry) {
    const ResourceEntry* resource;
    vn_u32 i;

    resource = vnpak_get(pak, (vn_u32)entry->script_resource_id);
    if (resource == (const ResourceEntry*)0 || resource->type != VN_RESOURCE_TYPE_SCRIPT) {
        return VN_E_FORMAT;
    }
    for (i = 0u; i < catalog->count; ++i) {
        if (catalog->entries[i].scene_id == entry->scene_id ||
            strcmp(catalog->entries[i].name, entry->name) == 0) {
            return VN_E_FORMAT;
        }
    }
    return VN_OK;
}

int vn_scene_catalog_load(VNSceneCatalog* catalog, const VNPak* pak) {
    const ResourceEntry* resource;
    vn_u8* payload;
    vn_u32 catalog_resource_id;
    vn_u32 catalog_count;
    vn_u32 read_size;
    vn_u32 cursor;
    vn_u32 i;
    int rc;

    if (catalog == (VNSceneCatalog*)0 || pak == (const VNPak*)0) {
        return VN_E_INVALID_ARG;
    }
    vn_scene_catalog_init(catalog);
    resource = (const ResourceEntry*)0;
    catalog_resource_id = 0u;
    catalog_count = 0u;
    for (i = 0u; i < pak->resource_count; ++i) {
        const ResourceEntry* candidate;
        candidate = vnpak_get(pak, i);
        if (candidate != (const ResourceEntry*)0 &&
            candidate->type == VN_RESOURCE_TYPE_SCENE_CATALOG) {
            resource = candidate;
            catalog_resource_id = i;
            catalog_count += 1u;
        }
    }
    if (catalog_count == 0u) {
        return VN_OK;
    }
    if (catalog_count != 1u || resource == (const ResourceEntry*)0 ||
        resource->data_size < VN_SCENE_CATALOG_HEADER_SIZE) {
        return VN_E_FORMAT;
    }

    payload = (vn_u8*)malloc((size_t)resource->data_size);
    if (payload == (vn_u8*)0) {
        return VN_E_NOMEM;
    }
    rc = vnpak_read_resource(pak,
                             catalog_resource_id,
                             payload,
                             resource->data_size,
                             &read_size);
    if (rc != VN_OK || read_size != resource->data_size) {
        free(payload);
        return (rc != VN_OK) ? rc : VN_E_IO;
    }
    if (payload[0] != (vn_u8)'V' || payload[1] != (vn_u8)'N' ||
        payload[2] != (vn_u8)'S' || payload[3] != (vn_u8)'C' ||
        scene_catalog_u16_le(payload + 4) != VN_SCENE_CATALOG_VERSION) {
        free(payload);
        return VN_E_FORMAT;
    }

    catalog_count = (vn_u32)scene_catalog_u16_le(payload + 6);
    catalog->entry_scene_id = scene_catalog_u32_le(payload + 8);
    if (catalog_count == 0u || catalog_count > VN_SCENE_CATALOG_MAX_SCENES) {
        free(payload);
        vn_scene_catalog_init(catalog);
        return VN_E_FORMAT;
    }

    cursor = VN_SCENE_CATALOG_HEADER_SIZE;
    for (i = 0u; i < catalog_count; ++i) {
        VNSceneCatalogEntry entry;
        vn_u32 name_len;

        if (cursor > read_size || read_size - cursor < VN_SCENE_CATALOG_ENTRY_HEADER_SIZE) {
            free(payload);
            vn_scene_catalog_init(catalog);
            return VN_E_FORMAT;
        }
        entry.scene_id = scene_catalog_u32_le(payload + cursor);
        entry.script_resource_id = scene_catalog_u16_le(payload + cursor + 4u);
        name_len = (vn_u32)payload[cursor + 6u];
        if (payload[cursor + 7u] != 0u || name_len == 0u ||
            name_len >= VN_SCENE_NAME_MAX ||
            read_size - cursor - VN_SCENE_CATALOG_ENTRY_HEADER_SIZE < name_len) {
            free(payload);
            vn_scene_catalog_init(catalog);
            return VN_E_FORMAT;
        }
        if (vn_scene_name_bytes_are_valid(
                payload + cursor + VN_SCENE_CATALOG_ENTRY_HEADER_SIZE,
                name_len) == VN_FALSE) {
            free(payload);
            vn_scene_catalog_init(catalog);
            return VN_E_FORMAT;
        }
        (void)memcpy(entry.name,
                     payload + cursor + VN_SCENE_CATALOG_ENTRY_HEADER_SIZE,
                     (size_t)name_len);
        entry.name[name_len] = '\0';
        rc = scene_catalog_validate_entry(catalog, pak, &entry);
        if (rc != VN_OK) {
            free(payload);
            vn_scene_catalog_init(catalog);
            return rc;
        }
        catalog->entries[i] = entry;
        catalog->count += 1u;
        cursor += VN_SCENE_CATALOG_ENTRY_HEADER_SIZE + name_len;
    }
    free(payload);
    if (cursor != read_size ||
        vn_scene_catalog_find_id(catalog, catalog->entry_scene_id) == (const VNSceneCatalogEntry*)0) {
        vn_scene_catalog_init(catalog);
        return VN_E_FORMAT;
    }
    catalog->present = VN_TRUE;
    return VN_OK;
}

const VNSceneCatalogEntry* vn_scene_catalog_find_name(const VNSceneCatalog* catalog,
                                                      const char* name) {
    vn_u32 i;
    if (catalog == (const VNSceneCatalog*)0 || name == (const char*)0) {
        return (const VNSceneCatalogEntry*)0;
    }
    for (i = 0u; i < catalog->count; ++i) {
        if (strcmp(catalog->entries[i].name, name) == 0) {
            return &catalog->entries[i];
        }
    }
    return (const VNSceneCatalogEntry*)0;
}

const VNSceneCatalogEntry* vn_scene_catalog_find_id(const VNSceneCatalog* catalog,
                                                    vn_u32 scene_id) {
    vn_u32 i;
    if (catalog == (const VNSceneCatalog*)0) {
        return (const VNSceneCatalogEntry*)0;
    }
    for (i = 0u; i < catalog->count; ++i) {
        if (catalog->entries[i].scene_id == scene_id) {
            return &catalog->entries[i];
        }
    }
    return (const VNSceneCatalogEntry*)0;
}
