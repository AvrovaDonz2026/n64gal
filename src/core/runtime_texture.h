#ifndef VN_RUNTIME_TEXTURE_H
#define VN_RUNTIME_TEXTURE_H

#include "vn_pack.h"
#include "vn_backend.h"

#define VN_RUNTIME_TEXTURE_CACHE_MAX_BYTES (32u * 1024u * 1024u)

typedef struct {
    vn_u8* data;
    vn_u32 data_size;
    vn_u32 stamp;
    vn_u8 loaded;
    vn_u8 pinned;
} VNRuntimeTextureCacheEntry;

typedef struct {
    const VNPak* pak;
    VNRuntimeTextureCacheEntry* entries;
    vn_u32 entry_count;
    vn_u32 bytes_used;
    vn_u32 byte_limit;
    vn_u32 stamp;
} VNRuntimeTextureStore;

int runtime_texture_store_init(VNRuntimeTextureStore* store, const VNPak* pak);
void runtime_texture_store_destroy(VNRuntimeTextureStore* store);
int runtime_texture_store_prepare_ops(VNRuntimeTextureStore* store,
                                      const VNRenderOp* ops,
                                      vn_u32 op_count);
int runtime_texture_store_lookup(void* user, vn_u16 texture_id, VNTextureView* out_view);

#endif
