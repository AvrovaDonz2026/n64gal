#ifndef VN_AVX2_INTERNAL_H
#define VN_AVX2_INTERNAL_H

#include <stdlib.h>
#include <string.h>

#include "vn_backend.h"
#include "vn_renderer.h"
#include "vn_error.h"

#include "../common/pixel_pipeline.h"
#include "../../core/build_config.h"

#if VN_AVX2_GNU_STYLE_IMPL
#include <immintrin.h>
#define VN_AVX2_IMPL_AVAILABLE 1
#define VN_AVX2_GNU_IMPL 1
#elif VN_AVX2_MSVC_STYLE_IMPL
#include <intrin.h>
#include <immintrin.h>
#define VN_AVX2_IMPL_AVAILABLE 1
#define VN_AVX2_MSVC_IMPL 1
#else
#define VN_AVX2_IMPL_AVAILABLE 0
#endif

#if defined(VN_AVX2_GNU_IMPL)
#define VN_AVX2_TARGET_ATTR __attribute__((target("avx2")))
#elif defined(VN_AVX2_MSVC_IMPL)
#define VN_AVX2_TARGET_ATTR
#else
#define VN_AVX2_TARGET_ATTR
#endif

extern RendererConfig g_avx2_cfg;
extern vn_u32* g_avx2_framebuffer;
extern vn_u32 g_avx2_stride;
extern vn_u32 g_avx2_height;
extern vn_u32 g_avx2_pixels;
extern vn_u8* g_avx2_u_lut;
extern vn_u8* g_avx2_v_lut;
extern vn_u32 g_avx2_u_lut_cap;
extern vn_u32 g_avx2_v_lut_cap;
extern int g_avx2_ready;

vn_u32 vn_avx2_blend_rgb_local(vn_u32 dst, vn_u32 src, vn_u8 alpha);

int vn_avx2_clip_rect_region(vn_i16 x,
                             vn_i16 y,
                             vn_u16 w,
                             vn_u16 h,
                             const VNRenderRect* clip_rect,
                             vn_u32* out_x0,
                             vn_u32* out_y0,
                             vn_u32* out_x1,
                             vn_u32* out_y1);

void vn_avx2_draw_textured_rect(const VNRenderOp* op);
void vn_avx2_draw_textured_rect_clipped(const VNRenderOp* op,
                                        const VNRenderRect* clip_rect);

#endif
