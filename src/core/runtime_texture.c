#include <stdlib.h>
#include <string.h>

#include "runtime_texture.h"
#include "vn_error.h"

static int runtime_texture_expected_size(const ResourceEntry* resource,
                                         vn_u32* out_size,
                                         vn_u8* out_format) {
    vn_u32 pixels;
    vn_u32 expected;
    vn_u8 format;

    if (resource == (const ResourceEntry*)0 || out_size == (vn_u32*)0 ||
        out_format == (vn_u8*)0 || resource->type != VN_RESOURCE_TYPE_IMAGE ||
        resource->width == 0u || resource->height == 0u) {
        return VN_E_FORMAT;
    }
    pixels = (vn_u32)resource->width * (vn_u32)resource->height;
    if (resource->height != 0u && pixels / (vn_u32)resource->height != (vn_u32)resource->width) {
        return VN_E_FORMAT;
    }
    format = (vn_u8)(resource->flags & 0x0Fu);
    if (format == VN_IMAGE_FORMAT_RGBA16) {
        if (pixels > 0x7FFFFFFFu) {
            return VN_E_FORMAT;
        }
        expected = pixels * 2u;
    } else if (format == VN_IMAGE_FORMAT_CI8) {
        if (pixels > 0xFFFFFFFFu - 512u) {
            return VN_E_FORMAT;
        }
        expected = pixels + 512u;
    } else if (format == VN_IMAGE_FORMAT_IA8) {
        expected = pixels;
    } else {
        return VN_E_UNSUPPORTED;
    }
    if (resource->data_size != expected || expected == 0u ||
        expected > VN_RUNTIME_TEXTURE_CACHE_MAX_BYTES) {
        return (expected > VN_RUNTIME_TEXTURE_CACHE_MAX_BYTES) ? VN_E_NOMEM : VN_E_FORMAT;
    }
    *out_size = expected;
    *out_format = format;
    return VN_OK;
}

int runtime_texture_store_init(VNRuntimeTextureStore* store, const VNPak* pak) {
    if (store == (VNRuntimeTextureStore*)0 || pak == (const VNPak*)0) {
        return VN_E_INVALID_ARG;
    }
    (void)memset(store, 0, sizeof(*store));
    store->pak = pak;
    store->entry_count = pak->resource_count;
    store->byte_limit = VN_RUNTIME_TEXTURE_CACHE_MAX_BYTES;
    if (store->entry_count > 0u) {
        store->entries = (VNRuntimeTextureCacheEntry*)calloc((size_t)store->entry_count,
                                                             sizeof(VNRuntimeTextureCacheEntry));
        if (store->entries == (VNRuntimeTextureCacheEntry*)0) {
            (void)memset(store, 0, sizeof(*store));
            return VN_E_NOMEM;
        }
    }
    return VN_OK;
}

void runtime_texture_store_destroy(VNRuntimeTextureStore* store) {
    vn_u32 i;
    if (store == (VNRuntimeTextureStore*)0) {
        return;
    }
    for (i = 0u; i < store->entry_count; ++i) {
        if (store->entries[i].data != (vn_u8*)0) {
            free(store->entries[i].data);
        }
    }
    if (store->entries != (VNRuntimeTextureCacheEntry*)0) {
        free(store->entries);
    }
    (void)memset(store, 0, sizeof(*store));
}

static int runtime_texture_store_evict_one(VNRuntimeTextureStore* store) {
    vn_u32 victim;
    vn_u32 oldest;
    vn_u32 i;
    int found;

    victim = 0u;
    oldest = 0u;
    found = VN_FALSE;
    for (i = 0u; i < store->entry_count; ++i) {
        const VNRuntimeTextureCacheEntry* entry;
        entry = &store->entries[i];
        if (entry->loaded == 0u || entry->pinned != 0u) {
            continue;
        }
        if (found == VN_FALSE || entry->stamp < oldest) {
            victim = i;
            oldest = entry->stamp;
            found = VN_TRUE;
        }
    }
    if (found == VN_FALSE) {
        return VN_E_NOMEM;
    }
    store->bytes_used -= store->entries[victim].data_size;
    free(store->entries[victim].data);
    (void)memset(&store->entries[victim], 0, sizeof(store->entries[victim]));
    return VN_OK;
}

static int runtime_texture_store_load(VNRuntimeTextureStore* store, vn_u16 texture_id) {
    VNRuntimeTextureCacheEntry* cache;
    const ResourceEntry* resource;
    vn_u8 format;
    vn_u32 expected;
    vn_u32 read_size;
    vn_u8* data;
    int rc;

    if ((vn_u32)texture_id >= store->entry_count) {
        return VN_E_FORMAT;
    }
    cache = &store->entries[texture_id];
    if (cache->loaded != 0u) {
        return VN_OK;
    }
    resource = vnpak_get(store->pak, (vn_u32)texture_id);
    rc = runtime_texture_expected_size(resource, &expected, &format);
    if (rc != VN_OK) {
        return rc;
    }
    if (store->byte_limit == 0u || expected > store->byte_limit) {
        return VN_E_NOMEM;
    }
    while (store->bytes_used > store->byte_limit - expected) {
        rc = runtime_texture_store_evict_one(store);
        if (rc != VN_OK) {
            return rc;
        }
    }
    data = (vn_u8*)malloc((size_t)expected);
    if (data == (vn_u8*)0) {
        return VN_E_NOMEM;
    }
    rc = vnpak_read_resource(store->pak,
                             (vn_u32)texture_id,
                             data,
                             expected,
                             &read_size);
    if (rc != VN_OK || read_size != expected) {
        free(data);
        return (rc != VN_OK) ? rc : VN_E_IO;
    }
    cache->data = data;
    cache->data_size = expected;
    cache->loaded = 1u;
    store->stamp += 1u;
    cache->stamp = store->stamp;
    store->bytes_used += expected;
    (void)format;
    return VN_OK;
}

int runtime_texture_store_prepare_ops(VNRuntimeTextureStore* store,
                                      const VNRenderOp* ops,
                                      vn_u32 op_count) {
    vn_u32 working_set_bytes;
    vn_u32 i;
    int rc;

    if (store == (VNRuntimeTextureStore*)0 ||
        (ops == (const VNRenderOp*)0 && op_count != 0u)) {
        return VN_E_INVALID_ARG;
    }
    for (i = 0u; i < store->entry_count; ++i) {
        store->entries[i].pinned = 0u;
    }
    working_set_bytes = 0u;
    for (i = 0u; i < op_count; ++i) {
        vn_u16 texture_id;
        const ResourceEntry* resource;
        vn_u32 expected;
        vn_u8 format;
        if ((ops[i].flags & VN_OP_FLAG_RESOURCE_TEXTURE) == 0u) {
            continue;
        }
        texture_id = ops[i].tex_id;
        if ((vn_u32)texture_id >= store->entry_count) {
            rc = VN_E_FORMAT;
            goto fail;
        }
        if (store->entries[texture_id].pinned != 0u) {
            continue;
        }
        resource = vnpak_get(store->pak, (vn_u32)texture_id);
        rc = runtime_texture_expected_size(resource, &expected, &format);
        if (rc != VN_OK) {
            goto fail;
        }
        if (working_set_bytes > store->byte_limit ||
            expected > store->byte_limit - working_set_bytes) {
            rc = VN_E_NOMEM;
            goto fail;
        }
        working_set_bytes += expected;
        store->entries[texture_id].pinned = 1u;
        (void)format;
    }
    for (i = 0u; i < op_count; ++i) {
        vn_u16 texture_id;
        if ((ops[i].flags & VN_OP_FLAG_RESOURCE_TEXTURE) == 0u) {
            continue;
        }
        texture_id = ops[i].tex_id;
        rc = runtime_texture_store_load(store, texture_id);
        if (rc != VN_OK) {
            goto fail;
        }
        store->stamp += 1u;
        store->entries[texture_id].stamp = store->stamp;
    }
    return VN_OK;

fail:
    for (i = 0u; i < store->entry_count; ++i) {
        store->entries[i].pinned = 0u;
    }
    return rc;
}

int runtime_texture_store_lookup(void* user, vn_u16 texture_id, VNTextureView* out_view) {
    VNRuntimeTextureStore* store;
    const ResourceEntry* resource;
    VNRuntimeTextureCacheEntry* cache;
    vn_u32 expected;
    vn_u8 format;
    int rc;

    store = (VNRuntimeTextureStore*)user;
    if (store == (VNRuntimeTextureStore*)0 || out_view == (VNTextureView*)0 ||
        (vn_u32)texture_id >= store->entry_count) {
        return VN_E_INVALID_ARG;
    }
    cache = &store->entries[texture_id];
    if (cache->loaded == 0u || cache->data == (vn_u8*)0) {
        return VN_E_RENDER_STATE;
    }
    resource = vnpak_get(store->pak, (vn_u32)texture_id);
    rc = runtime_texture_expected_size(resource, &expected, &format);
    if (rc != VN_OK || expected != cache->data_size) {
        return (rc != VN_OK) ? rc : VN_E_FORMAT;
    }
    out_view->data = cache->data;
    out_view->data_size = cache->data_size;
    out_view->width = resource->width;
    out_view->height = resource->height;
    out_view->format = format;
    return VN_OK;
}
