#ifndef VN_BACKEND_H
#define VN_BACKEND_H

#include "vn_types.h"

typedef struct RendererConfig RendererConfig;

#define VN_ARCH_SCALAR 1
#define VN_ARCH_AVX2   2
#define VN_ARCH_NEON   3
#define VN_ARCH_RVV    4

#define VN_ARCH_MASK_SCALAR (1u << 0)
#define VN_ARCH_MASK_AVX2   (1u << 1)
#define VN_ARCH_MASK_NEON   (1u << 2)
#define VN_ARCH_MASK_RVV    (1u << 3)
#define VN_ARCH_MASK_ALL    (VN_ARCH_MASK_SCALAR | VN_ARCH_MASK_AVX2 | VN_ARCH_MASK_NEON | VN_ARCH_MASK_RVV)

#define VN_OP_CLEAR  1
#define VN_OP_SPRITE 2
#define VN_OP_TEXT   3
#define VN_OP_FADE   4

typedef struct {
    vn_u8 op;
    vn_u8 layer;
    vn_u16 tex_id;
    vn_i16 x;
    vn_i16 y;
    vn_u16 w;
    vn_u16 h;
    vn_u8 alpha;
    vn_u8 flags;
} VNRenderOp;

typedef struct {
    vn_u16 x;
    vn_u16 y;
    vn_u16 w;
    vn_u16 h;
} VNRenderRect;

typedef struct {
    vn_u16 width;
    vn_u16 height;
    vn_u32 rect_count;
    vn_u32 full_redraw;
    const VNRenderRect* rects;
} VNRenderDirtySubmit;

typedef struct {
    vn_u32 has_simd;
    vn_u32 has_lut_blend;
    vn_u32 has_tmem_cache;
} VNBackendCaps;

typedef struct {
    const char* name;
    vn_u32 arch_tag;
    int (*init)(const RendererConfig* cfg);
    void (*shutdown)(void);
    void (*begin_frame)(void);
    int (*submit_ops)(const VNRenderOp* ops, vn_u32 op_count);
    void (*end_frame)(void);
    void (*query_caps)(VNBackendCaps* out_caps);
    int (*submit_ops_dirty)(const VNRenderOp* ops,
                            vn_u32 op_count,
                            const VNRenderDirtySubmit* dirty_submit);
} VNRenderBackend;

int vn_backend_register(const VNRenderBackend* be);
const VNRenderBackend* vn_backend_select(vn_u32 prefer_arch_mask);
const VNRenderBackend* vn_backend_get_active(void);
void vn_backend_reset_registry(void);

#endif
