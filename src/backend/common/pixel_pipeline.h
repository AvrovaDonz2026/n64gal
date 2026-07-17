#ifndef VN_PIXEL_PIPELINE_H
#define VN_PIXEL_PIPELINE_H

#include "vn_types.h"

vn_u32 vn_pp_make_gray(vn_u8 gray);
vn_u32 vn_pp_blend_rgb(vn_u32 dst, vn_u32 src, vn_u8 alpha);
vn_u8 vn_pp_mul_alpha(vn_u8 a, vn_u8 b);

vn_u32 vn_pp_sample_texel(vn_u16 tex_id, vn_u32 u8, vn_u32 v8);
vn_u32 vn_pp_combine_texel(vn_u32 texel, vn_u8 layer, vn_u8 flags, vn_u8 op);

int vn_pp_draw_resource_texture(vn_u32* framebuffer,
                                vn_u32 framebuffer_width,
                                vn_u32 framebuffer_height,
                                const VNRenderOp* op,
                                const VNRenderRect* clip_rect);
int vn_pp_draw_resource_crossfade(vn_u32* framebuffer,
                                  vn_u32 framebuffer_width,
                                  vn_u32 framebuffer_height,
                                  const VNRenderOp* from_op,
                                  const VNRenderOp* to_op,
                                  const VNRenderRect* clip_rect);

vn_u32 vn_pp_frame_crc32(const vn_u32* pixels, vn_u32 count);

#endif
