#ifndef VN_RENDERER_H
#define VN_RENDERER_H

#include "vn_types.h"
#include "vn_backend.h"

struct RendererConfig {
    vn_u16 width;
    vn_u16 height;
    vn_u32 flags;
};

#define VN_RENDERER_FLAG_SIMD         (1u << 0)
#define VN_RENDERER_FLAG_VSYNC        (1u << 1)
#define VN_RENDERER_FLAG_FORCE_SCALAR (1u << 8)
#define VN_RENDERER_FLAG_FORCE_AVX2   (1u << 9)
#define VN_RENDERER_FLAG_FORCE_NEON   (1u << 10)
#define VN_RENDERER_FLAG_FORCE_RVV    (1u << 11)

int renderer_init(const RendererConfig* cfg);
void renderer_begin_frame(void);
void renderer_submit(const VNRenderOp* ops, vn_u32 op_count);
void renderer_end_frame(void);
void renderer_shutdown(void);
const char* renderer_backend_name(void);

#endif
