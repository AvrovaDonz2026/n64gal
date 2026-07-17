#ifndef VN_SCENE_CATALOG_H
#define VN_SCENE_CATALOG_H

#include "vn_pack.h"

#define VN_SCENE_CATALOG_VERSION 1u
#define VN_SCENE_CATALOG_MAX_SCENES 256u
#define VN_SCENE_NAME_MAX 64u

typedef struct {
    vn_u32 scene_id;
    vn_u16 script_resource_id;
    char name[VN_SCENE_NAME_MAX];
} VNSceneCatalogEntry;

typedef struct {
    VNSceneCatalogEntry entries[VN_SCENE_CATALOG_MAX_SCENES];
    vn_u32 count;
    vn_u32 entry_scene_id;
    int present;
} VNSceneCatalog;

void vn_scene_catalog_init(VNSceneCatalog* catalog);
int vn_scene_name_bytes_are_valid(const vn_u8* name, vn_u32 name_len);
int vn_scene_name_is_valid(const char* name);
int vn_scene_catalog_load(VNSceneCatalog* catalog, const VNPak* pak);
const VNSceneCatalogEntry* vn_scene_catalog_find_name(const VNSceneCatalog* catalog,
                                                      const char* name);
const VNSceneCatalogEntry* vn_scene_catalog_find_id(const VNSceneCatalog* catalog,
                                                    vn_u32 scene_id);

#endif
