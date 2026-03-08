#include <stdlib.h>
#include <string.h>

#include "vn_backend.h"
#include "vn_renderer.h"
#include "vn_error.h"

#include "../common/pixel_pipeline.h"
#include "../../core/build_config.h"

#if VN_NEON_IMPL_AVAILABLE
#include <arm_neon.h>
#endif

static int vn_neon_runtime_supported(void) {
#if VN_NEON_IMPL_AVAILABLE
    return VN_TRUE;
#else
    return VN_FALSE;
#endif
}

static RendererConfig g_neon_cfg;
static vn_u32* g_neon_framebuffer = (vn_u32*)0;
static vn_u32 g_neon_stride = 0u;
static vn_u32 g_neon_height = 0u;
static vn_u32 g_neon_pixels = 0u;
static vn_u8* g_neon_u_lut = (vn_u8*)0;
static vn_u8* g_neon_v_lut = (vn_u8*)0;
static vn_u32 g_neon_u_lut_cap = 0u;
static vn_u32 g_neon_v_lut_cap = 0u;
static int g_neon_ready = VN_FALSE;

static int vn_neon_clip_rect_region(vn_i16 x,
                                    vn_i16 y,
                                    vn_u16 w,
                                    vn_u16 h,
                                    const VNRenderRect* clip_rect,
                                    vn_u32* out_x0,
                                    vn_u32* out_y0,
                                    vn_u32* out_x1,
                                    vn_u32* out_y1) {
    int x0;
    int y0;
    int x1;
    int y1;
    int clip_x0;
    int clip_y0;
    int clip_x1;
    int clip_y1;

    if (out_x0 == (vn_u32*)0 || out_y0 == (vn_u32*)0 || out_x1 == (vn_u32*)0 || out_y1 == (vn_u32*)0) {
        return VN_FALSE;
    }
    if (g_neon_stride == 0u || g_neon_height == 0u) {
        return VN_FALSE;
    }

    x0 = (int)x;
    y0 = (int)y;
    x1 = x0 + (int)w;
    y1 = y0 + (int)h;

    if (clip_rect != (const VNRenderRect*)0) {
        if (clip_rect->w == 0u || clip_rect->h == 0u) {
            return VN_FALSE;
        }
        clip_x0 = (int)clip_rect->x;
        clip_y0 = (int)clip_rect->y;
        clip_x1 = clip_x0 + (int)clip_rect->w;
        clip_y1 = clip_y0 + (int)clip_rect->h;
        if (x0 < clip_x0) {
            x0 = clip_x0;
        }
        if (y0 < clip_y0) {
            y0 = clip_y0;
        }
        if (x1 > clip_x1) {
            x1 = clip_x1;
        }
        if (y1 > clip_y1) {
            y1 = clip_y1;
        }
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > (int)g_neon_stride) {
        x1 = (int)g_neon_stride;
    }
    if (y1 > (int)g_neon_height) {
        y1 = (int)g_neon_height;
    }

    if (x0 >= x1 || y0 >= y1) {
        return VN_FALSE;
    }

    *out_x0 = (vn_u32)x0;
    *out_y0 = (vn_u32)y0;
    *out_x1 = (vn_u32)x1;
    *out_y1 = (vn_u32)y1;
    return VN_TRUE;
}

#if VN_NEON_IMPL_AVAILABLE
static uint32x4_t vn_neon_div255_round_u32_preloaded(uint32x4_t value,
                                                     uint32x4_t bias,
                                                     uint32x4_t one) {
    value = vaddq_u32(value, bias);
    value = vaddq_u32(value, one);
    value = vaddq_u32(value, vshrq_n_u32(value, 8));
    return vshrq_n_u32(value, 8);
}

static void vn_neon_fill_u32(vn_u32* dst, vn_u32 count, vn_u32 value) {
    uint32x4_t vec;
    vn_u32 i;

    vec = vdupq_n_u32((uint32_t)value);
    i = 0u;
    while ((i + 4u) <= count) {
        vst1q_u32((uint32_t*)(void*)(dst + i), vec);
        i += 4u;
    }
    while (i < count) {
        dst[i] = value;
        i += 1u;
    }
}

static void vn_neon_blend_u32_uniform(vn_u32* dst, vn_u32 count, vn_u32 color, vn_u8 alpha) {
    uint32x4_t mask;
    uint32x4_t alpha_mask;
    uint32x4_t bias_vec;
    uint32x4_t one_vec;
    uint32x4_t src_r_vec;
    uint32x4_t src_g_vec;
    uint32x4_t src_b_vec;
    vn_u32 src_r_prod;
    vn_u32 src_g_prod;
    vn_u32 src_b_prod;
    vn_u32 inv;
    uint32_t inv_u32;
    vn_u32 i;

    if (dst == (vn_u32*)0 || count == 0u || alpha == 0u) {
        return;
    }
    if (alpha >= 255u) {
        vn_neon_fill_u32(dst, count, color);
        return;
    }

    mask = vdupq_n_u32((uint32_t)0xFFu);
    alpha_mask = vdupq_n_u32((uint32_t)0xFF000000u);
    bias_vec = vdupq_n_u32((uint32_t)127u);
    one_vec = vdupq_n_u32((uint32_t)1u);
    inv = (vn_u32)(255u - (vn_u32)alpha);
    inv_u32 = (uint32_t)inv;
    src_r_prod = ((color >> 16) & 0xFFu) * (vn_u32)alpha;
    src_g_prod = ((color >> 8) & 0xFFu) * (vn_u32)alpha;
    src_b_prod = (color & 0xFFu) * (vn_u32)alpha;
    src_r_vec = vdupq_n_u32((uint32_t)src_r_prod);
    src_g_vec = vdupq_n_u32((uint32_t)src_g_prod);
    src_b_vec = vdupq_n_u32((uint32_t)src_b_prod);

    i = 0u;
    while ((i + 4u) <= count) {
        uint32x4_t dst_px;
        uint32x4_t dr;
        uint32x4_t dg;
        uint32x4_t db;
        uint32x4_t rr;
        uint32x4_t rg;
        uint32x4_t rb;
        uint32x4_t out;

        dst_px = vld1q_u32((const uint32_t*)(const void*)(dst + i));
        dr = vandq_u32(vshrq_n_u32(dst_px, 16), mask);
        dg = vandq_u32(vshrq_n_u32(dst_px, 8), mask);
        db = vandq_u32(dst_px, mask);

        rr = vmulq_n_u32(dr, inv_u32);
        rr = vaddq_u32(rr, src_r_vec);
        rr = vn_neon_div255_round_u32_preloaded(rr, bias_vec, one_vec);

        rg = vmulq_n_u32(dg, inv_u32);
        rg = vaddq_u32(rg, src_g_vec);
        rg = vn_neon_div255_round_u32_preloaded(rg, bias_vec, one_vec);

        rb = vmulq_n_u32(db, inv_u32);
        rb = vaddq_u32(rb, src_b_vec);
        rb = vn_neon_div255_round_u32_preloaded(rb, bias_vec, one_vec);

        rr = vshlq_n_u32(rr, 16);
        rg = vshlq_n_u32(rg, 8);
        out = vorrq_u32(vorrq_u32(rr, rg), rb);
        out = vorrq_u32(out, alpha_mask);
        vst1q_u32((uint32_t*)(void*)(dst + i), out);
        i += 4u;
    }
    while (i < count) {
        dst[i] = vn_pp_blend_rgb(dst[i], color, alpha);
        i += 1u;
    }
}
#else
static void vn_neon_fill_u32(vn_u32* dst, vn_u32 count, vn_u32 value) {
    vn_u32 i;

    i = 0u;
    while (i < count) {
        dst[i] = value;
        i += 1u;
    }
}

static void vn_neon_blend_u32_uniform(vn_u32* dst, vn_u32 count, vn_u32 color, vn_u8 alpha) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || count == 0u || alpha == 0u) {
        return;
    }
    if (alpha >= 255u) {
        vn_neon_fill_u32(dst, count, color);
        return;
    }

    i = 0u;
    while (i < count) {
        dst[i] = vn_pp_blend_rgb(dst[i], color, alpha);
        i += 1u;
    }
}
#endif

static void vn_neon_fill_rect_uniform_clipped(vn_i16 x,
                                              vn_i16 y,
                                              vn_u16 w,
                                              vn_u16 h,
                                              vn_u32 color,
                                              vn_u8 alpha,
                                              const VNRenderRect* clip_rect) {
    vn_u32 x0;
    vn_u32 y0;
    vn_u32 x1;
    vn_u32 y1;
    vn_u32 yy;

    if (g_neon_framebuffer == (vn_u32*)0) {
        return;
    }
    if (vn_neon_clip_rect_region(x, y, w, h, clip_rect, &x0, &y0, &x1, &y1) == VN_FALSE) {
        return;
    }

    if (alpha >= 255u) {
        for (yy = y0; yy < y1; ++yy) {
            vn_u32* row_ptr;
            row_ptr = g_neon_framebuffer + yy * g_neon_stride + x0;
            vn_neon_fill_u32(row_ptr, x1 - x0, color);
        }
        return;
    }

    for (yy = y0; yy < y1; ++yy) {
        vn_u32* row_ptr;

        row_ptr = g_neon_framebuffer + yy * g_neon_stride + x0;
        vn_neon_blend_u32_uniform(row_ptr, x1 - x0, color, alpha);
    }
}

static void vn_neon_clear_rect(vn_u8 gray, const VNRenderRect* clip_rect) {
    if (clip_rect == (const VNRenderRect*)0) {
        if (g_neon_framebuffer == (vn_u32*)0 || g_neon_pixels == 0u) {
            return;
        }
        vn_neon_fill_u32(g_neon_framebuffer, g_neon_pixels, vn_pp_make_gray(gray));
        return;
    }
    vn_neon_fill_rect_uniform_clipped(0,
                                      0,
                                      g_neon_cfg.width,
                                      g_neon_cfg.height,
                                      vn_pp_make_gray(gray),
                                      255u,
                                      clip_rect);
}

static void vn_neon_fill_rect_uniform(vn_i16 x, vn_i16 y, vn_u16 w, vn_u16 h, vn_u32 color, vn_u8 alpha) {
    vn_neon_fill_rect_uniform_clipped(x, y, w, h, color, alpha, (const VNRenderRect*)0);
}

static void vn_neon_build_coord_lut(vn_u8* out_lut, vn_u32 count, vn_u32 local_start, vn_u16 extent) {
    vn_u32 i;
    vn_u32 denom;
    vn_u32 value;

    if (out_lut == (vn_u8*)0 || count == 0u) {
        return;
    }
    if (extent <= 1u) {
        for (i = 0u; i < count; ++i) {
            out_lut[i] = 0u;
        }
        return;
    }

    denom = (vn_u32)extent - 1u;
    value = local_start * 255u;
    for (i = 0u; i < count; ++i) {
        vn_u32 q;
        q = value / denom;
        if (q > 255u) {
            q = 255u;
        }
        out_lut[i] = (vn_u8)q;
        value += 255u;
    }
}

static int vn_neon_clamp_u8_int(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

static vn_u32 vn_neon_hash32(vn_u32 x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

static vn_u32 vn_neon_blend_rgb_local(vn_u32 dst, vn_u32 src, vn_u8 alpha) {
    vn_u32 inv;
    vn_u32 dr;
    vn_u32 dg;
    vn_u32 db;
    vn_u32 sr;
    vn_u32 sg;
    vn_u32 sb;
    vn_u32 rr;
    vn_u32 rg;
    vn_u32 rb;

    if (alpha >= 255u) {
        return src;
    }
    if (alpha == 0u) {
        return dst;
    }

    inv = (vn_u32)(255u - alpha);
    dr = (dst >> 16) & 0xFFu;
    dg = (dst >> 8) & 0xFFu;
    db = dst & 0xFFu;
    sr = (src >> 16) & 0xFFu;
    sg = (src >> 8) & 0xFFu;
    sb = src & 0xFFu;

    rr = (sr * (vn_u32)alpha + dr * inv + 127u) / 255u;
    rg = (sg * (vn_u32)alpha + dg * inv + 127u) / 255u;
    rb = (sb * (vn_u32)alpha + db * inv + 127u) / 255u;

    return (vn_u32)(0xFF000000u | (rr << 16) | (rg << 8) | rb);
}

typedef struct VN_NEONTexturedRowParams {
    vn_u32 seed_xor;
    vn_u32 checker_xor;
    vn_u32 v8;
    int base_r;
    int base_g;
    int base_b;
    int text_blue_bias;
    int sprite_blue_bias;
    vn_u8 op;
#if VN_NEON_IMPL_AVAILABLE
    uint32_t seed_xor_lanes[4];
    uint32_t checker_xor_lanes[4];
    uint32_t v8_lanes[4];
    int32_t base_r_lanes[4];
    int32_t base_g_lanes[4];
    int32_t base_b_lanes[4];
    int32_t text_blue_bias_lanes[4];
    int32_t sprite_blue_bias_lanes[4];
#endif
} VN_NEONTexturedRowParams;

static void vn_neon_init_textured_row_params(VN_NEONTexturedRowParams* params,
                                             vn_u32 v8,
                                             vn_u16 tex_id,
                                             vn_u8 layer,
                                             vn_u8 flags,
                                             vn_u8 op) {
    if (params == (VN_NEONTexturedRowParams*)0) {
        return;
    }

    params->v8 = (v8 & 0xFFu);
    params->seed_xor = ((vn_u32)tex_id * 2654435761u) ^ params->v8 ^ ((vn_u32)tex_id << 16);
    params->checker_xor = ((params->v8 >> 5) & 0xFFu) ^ ((vn_u32)tex_id & 7u);
    params->base_r = (int)layer * 7;
    params->base_g = (int)layer * 5;
    params->base_b = (int)layer * 3;
    if ((flags & 1u) != 0u) {
        params->base_g += 14;
    }
    if ((flags & 2u) != 0u) {
        params->base_b += 20;
    }
    if ((flags & 4u) != 0u) {
        params->base_r += 28;
        params->base_g -= 12;
    }
    if ((flags & 8u) != 0u) {
        params->base_r += 12;
        params->base_g += 12;
        params->base_b -= 8;
    }
    params->text_blue_bias = 24 + (int)layer * 6;
    params->sprite_blue_bias = 10;
    params->op = op;
#if VN_NEON_IMPL_AVAILABLE
    {
        int lane;

        for (lane = 0; lane < 4; ++lane) {
            params->seed_xor_lanes[lane] = (uint32_t)params->seed_xor;
            params->checker_xor_lanes[lane] = (uint32_t)params->checker_xor;
            params->v8_lanes[lane] = (uint32_t)params->v8;
            params->base_r_lanes[lane] = (int32_t)params->base_r;
            params->base_g_lanes[lane] = (int32_t)params->base_g;
            params->base_b_lanes[lane] = (int32_t)params->base_b;
            params->text_blue_bias_lanes[lane] = (int32_t)params->text_blue_bias;
            params->sprite_blue_bias_lanes[lane] = (int32_t)params->sprite_blue_bias;
        }
    }
#endif
}

static vn_u32 vn_neon_sample_combine_texel(vn_u32 u8, const VN_NEONTexturedRowParams* params) {
    vn_u32 seed;
    vn_u32 h;
    int r;
    int g;
    int b;

    if (params == (const VN_NEONTexturedRowParams*)0) {
        return 0u;
    }

    u8 &= 0xFFu;
    seed = (u8 << 8) ^ params->seed_xor;
    h = vn_neon_hash32(seed);

    r = (int)(h & 0xFFu);
    g = (int)((h >> 8) & 0xFFu);
    b = (int)((h >> 16) & 0xFFu);

    if ((((u8 >> 5) ^ params->checker_xor) & 1u) != 0u) {
        r += 24;
        g += 24;
        b += 24;
    } else if (((u8 + params->v8) & 0x20u) != 0u) {
        r -= 16;
        g -= 10;
        b -= 16;
    }

    r = vn_neon_clamp_u8_int(r);
    g = vn_neon_clamp_u8_int(g);
    b = vn_neon_clamp_u8_int(b);

    r += params->base_r;
    g += params->base_g;
    b += params->base_b;

    if (params->op == VN_OP_TEXT) {
        int y;

        y = (r * 54 + g * 183 + b * 19) >> 8;
        r = y + 52;
        g = y + 44;
        b = y + params->text_blue_bias;
    } else if (params->op == VN_OP_SPRITE) {
        b += params->sprite_blue_bias;
    }

    r = vn_neon_clamp_u8_int(r);
    g = vn_neon_clamp_u8_int(g);
    b = vn_neon_clamp_u8_int(b);
    return (vn_u32)(0xFF000000u | ((vn_u32)r << 16) | ((vn_u32)g << 8) | (vn_u32)b);
}

#if VN_NEON_IMPL_AVAILABLE
static uint32x4_t vn_neon_sample_combine_chunk_u32x4(uint32x4_t u_vec,
                                                      const VN_NEONTexturedRowParams* params);
static uint32x4_t vn_neon_sample_combine_u_lut_chunk_u32x4(const vn_u8* u_lut,
                                                            vn_u32 base_idx,
                                                            const VN_NEONTexturedRowParams* params);
static uint32x4_t vn_neon_blend_rgb_chunk_u32x4(uint32x4_t dst_px,
                                                uint32x4_t src_px,
                                                vn_u8 alpha);
#endif

static void vn_neon_build_textured_row_palette(vn_u32* palette,
                                               const VN_NEONTexturedRowParams* params) {
    vn_u32 i;
#if VN_NEON_IMPL_AVAILABLE
    uint32x4_t u_vec;
    uint32x4_t step_vec;
#endif

    if (palette == (vn_u32*)0 || params == (const VN_NEONTexturedRowParams*)0) {
        return;
    }

#if VN_NEON_IMPL_AVAILABLE
    u_vec = vdupq_n_u32((uint32_t)0u);
    u_vec = vsetq_lane_u32((uint32_t)1u, u_vec, 1);
    u_vec = vsetq_lane_u32((uint32_t)2u, u_vec, 2);
    u_vec = vsetq_lane_u32((uint32_t)3u, u_vec, 3);
    step_vec = vdupq_n_u32((uint32_t)4u);
    i = 0u;
    while ((i + 4u) <= 256u) {
        uint32x4_t src_vec;

        src_vec = vn_neon_sample_combine_chunk_u32x4(u_vec, params);
        vst1q_u32((uint32_t*)(void*)(palette + i), src_vec);
        u_vec = vaddq_u32(u_vec, step_vec);
        i += 4u;
    }
#else
    i = 0u;
#endif
    while (i < 256u) {
        palette[i] = vn_neon_sample_combine_texel(i, params);
        i += 1u;
    }
}

static void vn_neon_sample_texels_row(vn_u32* dst,
                                      const vn_u8* u_lut,
                                      vn_u32 count,
                                      const VN_NEONTexturedRowParams* params) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || params == (const VN_NEONTexturedRowParams*)0) {
        return;
    }

#if VN_NEON_IMPL_AVAILABLE
    i = 0u;
    while ((i + 4u) <= count) {
        uint32x4_t src_vec;

        src_vec = vn_neon_sample_combine_u_lut_chunk_u32x4(u_lut, i, params);
        vst1q_u32((uint32_t*)(void*)(dst + i), src_vec);
        i += 4u;
    }
#else
    i = 0u;
#endif
    while (i < count) {
        dst[i] = vn_neon_sample_combine_texel((vn_u32)u_lut[i], params);
        i += 1u;
    }
}

static void vn_neon_sample_blend_texels_row(vn_u32* dst,
                                            const vn_u8* u_lut,
                                            vn_u32 count,
                                            const VN_NEONTexturedRowParams* params,
                                            vn_u8 alpha) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || params == (const VN_NEONTexturedRowParams*)0) {
        return;
    }
    if (alpha == 0u) {
        return;
    }
    if (alpha >= 255u) {
        vn_neon_sample_texels_row(dst, u_lut, count, params);
        return;
    }

#if VN_NEON_IMPL_AVAILABLE
    i = 0u;
    while ((i + 4u) <= count) {
        uint32x4_t dst_vec;
        uint32x4_t src_vec;

        dst_vec = vld1q_u32((const uint32_t*)(const void*)(dst + i));
        src_vec = vn_neon_sample_combine_u_lut_chunk_u32x4(u_lut, i, params);
        dst_vec = vn_neon_blend_rgb_chunk_u32x4(dst_vec, src_vec, alpha);
        vst1q_u32((uint32_t*)(void*)(dst + i), dst_vec);
        i += 4u;
    }
#else
    i = 0u;
#endif
    while (i < count) {
        vn_u32 src;

        src = vn_neon_sample_combine_texel((vn_u32)u_lut[i], params);
        dst[i] = vn_neon_blend_rgb_local(dst[i], src, alpha);
        i += 1u;
    }
}

#if VN_NEON_IMPL_AVAILABLE
static uint32x4_t vn_neon_u32x4_from_u_lut(const vn_u8* u_lut,
                                           vn_u32 base_idx) {
    uint32x4_t vec;

    vec = vdupq_n_u32((uint32_t)0u);
    vec = vsetq_lane_u32((uint32_t)u_lut[base_idx + 0u], vec, 0);
    vec = vsetq_lane_u32((uint32_t)u_lut[base_idx + 1u], vec, 1);
    vec = vsetq_lane_u32((uint32_t)u_lut[base_idx + 2u], vec, 2);
    vec = vsetq_lane_u32((uint32_t)u_lut[base_idx + 3u], vec, 3);
    return vec;
}

static uint32x4_t vn_neon_hash32_u32x4(uint32x4_t x) {
    const uint32x4_t mul0 = vdupq_n_u32((uint32_t)0x7FEB352Du);
    const uint32x4_t mul1 = vdupq_n_u32((uint32_t)0x846CA68Bu);

    x = veorq_u32(x, vshrq_n_u32(x, 16));
    x = vmulq_u32(x, mul0);
    x = veorq_u32(x, vshrq_n_u32(x, 15));
    x = vmulq_u32(x, mul1);
    x = veorq_u32(x, vshrq_n_u32(x, 16));
    return x;
}

static uint32x4_t vn_neon_sample_combine_chunk_u32x4(uint32x4_t u_vec,
                                                      const VN_NEONTexturedRowParams* params) {
    const uint32x4_t mask_ff = vdupq_n_u32((uint32_t)0xFFu);
    const uint32x4_t one_vec = vdupq_n_u32((uint32_t)1u);
    const uint32x4_t alt_vec = vdupq_n_u32((uint32_t)0x20u);
    const uint32x4_t alpha_mask = vdupq_n_u32((uint32_t)0xFF000000u);
    const int32x4_t zero_vec = vdupq_n_s32(0);
    const int32x4_t max_vec = vdupq_n_s32(255);
    uint32x4_t seed_vec;
    uint32x4_t hash_vec;
    uint32x4_t checker_mask;
    uint32x4_t alt_mask;
    int32x4_t r_vec;
    int32x4_t g_vec;
    int32x4_t b_vec;

    if (params == (const VN_NEONTexturedRowParams*)0) {
        return vdupq_n_u32((uint32_t)0u);
    }

    u_vec = vandq_u32(u_vec, mask_ff);
    seed_vec = veorq_u32(vshlq_n_u32(u_vec, 8), vld1q_u32(params->seed_xor_lanes));
    hash_vec = vn_neon_hash32_u32x4(seed_vec);

    r_vec = vreinterpretq_s32_u32(vandq_u32(hash_vec, mask_ff));
    g_vec = vreinterpretq_s32_u32(vandq_u32(vshrq_n_u32(hash_vec, 8), mask_ff));
    b_vec = vreinterpretq_s32_u32(vandq_u32(vshrq_n_u32(hash_vec, 16), mask_ff));

    checker_mask = vceqq_u32(vandq_u32(veorq_u32(vshrq_n_u32(u_vec, 5),
                                                 vld1q_u32(params->checker_xor_lanes)),
                                       one_vec),
                             one_vec);
    alt_mask = vceqq_u32(vandq_u32(vaddq_u32(u_vec, vld1q_u32(params->v8_lanes)), alt_vec), alt_vec);
    alt_mask = vandq_u32(vmvnq_u32(checker_mask), alt_mask);

    r_vec = vaddq_s32(r_vec, vreinterpretq_s32_u32(vandq_u32(checker_mask, vdupq_n_u32((uint32_t)24u))));
    g_vec = vaddq_s32(g_vec, vreinterpretq_s32_u32(vandq_u32(checker_mask, vdupq_n_u32((uint32_t)24u))));
    b_vec = vaddq_s32(b_vec, vreinterpretq_s32_u32(vandq_u32(checker_mask, vdupq_n_u32((uint32_t)24u))));

    r_vec = vsubq_s32(r_vec, vreinterpretq_s32_u32(vandq_u32(alt_mask, vdupq_n_u32((uint32_t)16u))));
    g_vec = vsubq_s32(g_vec, vreinterpretq_s32_u32(vandq_u32(alt_mask, vdupq_n_u32((uint32_t)10u))));
    b_vec = vsubq_s32(b_vec, vreinterpretq_s32_u32(vandq_u32(alt_mask, vdupq_n_u32((uint32_t)16u))));

    r_vec = vmaxq_s32(r_vec, zero_vec);
    g_vec = vmaxq_s32(g_vec, zero_vec);
    b_vec = vmaxq_s32(b_vec, zero_vec);
    r_vec = vminq_s32(r_vec, max_vec);
    g_vec = vminq_s32(g_vec, max_vec);
    b_vec = vminq_s32(b_vec, max_vec);

    r_vec = vaddq_s32(r_vec, vld1q_s32(params->base_r_lanes));
    g_vec = vaddq_s32(g_vec, vld1q_s32(params->base_g_lanes));
    b_vec = vaddq_s32(b_vec, vld1q_s32(params->base_b_lanes));

    if (params->op == VN_OP_TEXT) {
        int32x4_t y_vec;

        y_vec = vmulq_n_s32(r_vec, 54);
        y_vec = vaddq_s32(y_vec, vmulq_n_s32(g_vec, 183));
        y_vec = vaddq_s32(y_vec, vmulq_n_s32(b_vec, 19));
        y_vec = vshrq_n_s32(y_vec, 8);
        r_vec = vaddq_s32(y_vec, vdupq_n_s32(52));
        g_vec = vaddq_s32(y_vec, vdupq_n_s32(44));
        b_vec = vaddq_s32(y_vec, vld1q_s32(params->text_blue_bias_lanes));
    } else if (params->op == VN_OP_SPRITE) {
        b_vec = vaddq_s32(b_vec, vld1q_s32(params->sprite_blue_bias_lanes));
    }

    r_vec = vmaxq_s32(r_vec, zero_vec);
    g_vec = vmaxq_s32(g_vec, zero_vec);
    b_vec = vmaxq_s32(b_vec, zero_vec);
    r_vec = vminq_s32(r_vec, max_vec);
    g_vec = vminq_s32(g_vec, max_vec);
    b_vec = vminq_s32(b_vec, max_vec);

    return vorrq_u32(alpha_mask,
                     vorrq_u32(vshlq_n_u32(vreinterpretq_u32_s32(r_vec), 16),
                               vorrq_u32(vshlq_n_u32(vreinterpretq_u32_s32(g_vec), 8),
                                         vreinterpretq_u32_s32(b_vec))));
}

static uint32x4_t vn_neon_sample_combine_u_lut_chunk_u32x4(const vn_u8* u_lut,
                                                            vn_u32 base_idx,
                                                            const VN_NEONTexturedRowParams* params) {
    return vn_neon_sample_combine_chunk_u32x4(vn_neon_u32x4_from_u_lut(u_lut, base_idx), params);
}

static uint32x4_t vn_neon_blend_rgb_chunk_u32x4(uint32x4_t dst_px,
                                                uint32x4_t src_px,
                                                vn_u8 alpha) {
    const uint32x4_t mask_rb = vdupq_n_u32((uint32_t)0x00FF00FFu);
    const uint32x4_t mask_g = vdupq_n_u32((uint32_t)0x0000FF00u);
    const uint32x4_t bias_rb = vdupq_n_u32((uint32_t)0x007F007Fu);
    const uint32x4_t bias_g = vdupq_n_u32((uint32_t)0x00007F00u);
    const uint32x4_t one_rb = vdupq_n_u32((uint32_t)0x00010001u);
    const uint32x4_t one_g = vdupq_n_u32((uint32_t)0x00000100u);
    const uint32x4_t alpha_mask = vdupq_n_u32((uint32_t)0xFF000000u);
    const uint32_t inv_u32 = (uint32_t)(255u - (vn_u32)alpha);
    const uint32_t alpha_u32 = (uint32_t)alpha;
    uint32x4_t src_rb;
    uint32x4_t src_g;
    uint32x4_t dst_rb;
    uint32x4_t dst_g;
    uint32x4_t rb;
    uint32x4_t g;

    if (alpha >= 255u) {
        return src_px;
    }
    if (alpha == 0u) {
        return dst_px;
    }

    src_rb = vandq_u32(src_px, mask_rb);
    src_g = vandq_u32(src_px, mask_g);
    dst_rb = vandq_u32(dst_px, mask_rb);
    dst_g = vandq_u32(dst_px, mask_g);

    rb = vaddq_u32(vmulq_n_u32(src_rb, alpha_u32),
                   vmulq_n_u32(dst_rb, inv_u32));
    rb = vaddq_u32(rb, bias_rb);
    rb = vaddq_u32(rb,
                   vaddq_u32(one_rb,
                             vandq_u32(vshrq_n_u32(rb, 8), mask_rb)));
    rb = vandq_u32(vshrq_n_u32(rb, 8), mask_rb);

    g = vaddq_u32(vmulq_n_u32(src_g, alpha_u32),
                  vmulq_n_u32(dst_g, inv_u32));
    g = vaddq_u32(g, bias_g);
    g = vaddq_u32(g,
                  vaddq_u32(one_g,
                            vandq_u32(vshrq_n_u32(g, 8), mask_g)));
    g = vandq_u32(vshrq_n_u32(g, 8), mask_g);

    return vorrq_u32(vorrq_u32(rb, g), alpha_mask);
}

static uint32x4_t vn_neon_palette_chunk_u32x4(const vn_u8* u_lut,
                                              const vn_u32* palette,
                                              vn_u32 base_idx) {
    uint32x4_t vec;

    vec = vdupq_n_u32((uint32_t)0u);
    vec = vsetq_lane_u32((uint32_t)palette[(vn_u32)u_lut[base_idx + 0u]], vec, 0);
    vec = vsetq_lane_u32((uint32_t)palette[(vn_u32)u_lut[base_idx + 1u]], vec, 1);
    vec = vsetq_lane_u32((uint32_t)palette[(vn_u32)u_lut[base_idx + 2u]], vec, 2);
    vec = vsetq_lane_u32((uint32_t)palette[(vn_u32)u_lut[base_idx + 3u]], vec, 3);
    return vec;
}

static void vn_neon_sample_texels_palette_row(vn_u32* dst,
                                              const vn_u8* u_lut,
                                              vn_u32 count,
                                              const vn_u32* palette) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || palette == (const vn_u32*)0) {
        return;
    }

    i = 0u;
    while ((i + 4u) <= count) {
        uint32x4_t src_vec;

        src_vec = vn_neon_palette_chunk_u32x4(u_lut, palette, i);
        vst1q_u32((uint32_t*)(void*)(dst + i), src_vec);
        i += 4u;
    }
    while (i < count) {
        dst[i] = palette[(vn_u32)u_lut[i]];
        i += 1u;
    }
}

static void vn_neon_sample_blend_texels_palette_row(vn_u32* dst,
                                                    const vn_u8* u_lut,
                                                    vn_u32 count,
                                                    const vn_u32* palette,
                                                    vn_u8 alpha) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || palette == (const vn_u32*)0 || alpha == 0u) {
        return;
    }
    if (alpha >= 255u) {
        vn_neon_sample_texels_palette_row(dst, u_lut, count, palette);
        return;
    }

    i = 0u;
    while ((i + 4u) <= count) {
        uint32x4_t dst_px;
        uint32x4_t src_px;
        uint32x4_t out;

        dst_px = vld1q_u32((const uint32_t*)(const void*)(dst + i));
        src_px = vn_neon_palette_chunk_u32x4(u_lut, palette, i);
        out = vn_neon_blend_rgb_chunk_u32x4(dst_px, src_px, alpha);
        vst1q_u32((uint32_t*)(void*)(dst + i), out);
        i += 4u;
    }
    while (i < count) {
        vn_u32 src;

        src = palette[(vn_u32)u_lut[i]];
        dst[i] = vn_neon_blend_rgb_local(dst[i], src, alpha);
        i += 1u;
    }
}
#else
static void vn_neon_sample_texels_palette_row(vn_u32* dst,
                                              const vn_u8* u_lut,
                                              vn_u32 count,
                                              const vn_u32* palette) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || palette == (const vn_u32*)0) {
        return;
    }

    for (i = 0u; i < count; ++i) {
        dst[i] = palette[(vn_u32)u_lut[i]];
    }
}

static void vn_neon_sample_blend_texels_palette_row(vn_u32* dst,
                                                    const vn_u8* u_lut,
                                                    vn_u32 count,
                                                    const vn_u32* palette,
                                                    vn_u8 alpha) {
    vn_u32 i;

    if (dst == (vn_u32*)0 || u_lut == (const vn_u8*)0 || palette == (const vn_u32*)0 || alpha == 0u) {
        return;
    }
    if (alpha >= 255u) {
        vn_neon_sample_texels_palette_row(dst, u_lut, count, palette);
        return;
    }

    for (i = 0u; i < count; ++i) {
        vn_u32 src;

        src = palette[(vn_u32)u_lut[i]];
        dst[i] = vn_neon_blend_rgb_local(dst[i], src, alpha);
    }
}
#endif

static void vn_neon_draw_textured_rect_clipped(const VNRenderOp* op,
                                               const VNRenderRect* clip_rect) {
    vn_u32 x0;
    vn_u32 y0;
    vn_u32 x1;
    vn_u32 y1;
    vn_u32 vis_w;
    vn_u32 vis_h;
    int local_x_start_i;
    int local_y_start_i;
    vn_u32 local_x_start;
    vn_u32 local_y_start;
    vn_u32 row_rel;
    int use_row_palette;
    int have_row_palette;
    vn_u32 cached_v8;
    vn_u32 row_palette[256];
    VN_NEONTexturedRowParams params;

    if (op == (const VNRenderOp*)0 || g_neon_framebuffer == (vn_u32*)0) {
        return;
    }
    if (op->alpha == 0u) {
        return;
    }
    if (vn_neon_clip_rect_region(op->x, op->y, op->w, op->h, clip_rect, &x0, &y0, &x1, &y1) == VN_FALSE) {
        return;
    }

    vis_w = x1 - x0;
    vis_h = y1 - y0;
    if (vis_w == 0u || vis_h == 0u) {
        return;
    }
    if (g_neon_u_lut == (vn_u8*)0 || g_neon_v_lut == (vn_u8*)0) {
        return;
    }
    if (vis_w > g_neon_u_lut_cap || vis_h > g_neon_v_lut_cap) {
        return;
    }

    local_x_start_i = (int)x0 - (int)op->x;
    local_y_start_i = (int)y0 - (int)op->y;
    if (local_x_start_i < 0 || local_y_start_i < 0) {
        return;
    }
    local_x_start = (vn_u32)local_x_start_i;
    local_y_start = (vn_u32)local_y_start_i;

    vn_neon_build_coord_lut(g_neon_u_lut, vis_w, local_x_start, op->w);
    vn_neon_build_coord_lut(g_neon_v_lut, vis_h, local_y_start, op->h);

    use_row_palette = ((vis_w >= 384u && vis_h >= 64u) ? VN_TRUE : VN_FALSE);
    have_row_palette = VN_FALSE;
    cached_v8 = 0u;

    for (row_rel = 0u; row_rel < vis_h; ++row_rel) {
        vn_u32 yy;
        vn_u32 row_off;
        vn_u32 v8;
        vn_u32* row_ptr;

        yy = y0 + row_rel;
        row_off = yy * g_neon_stride;
        v8 = (vn_u32)g_neon_v_lut[row_rel];
        row_ptr = g_neon_framebuffer + row_off + x0;

        if (use_row_palette != VN_FALSE) {
            if (have_row_palette == VN_FALSE || v8 != cached_v8) {
                vn_neon_init_textured_row_params(&params,
                                                 v8,
                                                 op->tex_id,
                                                 op->layer,
                                                 op->flags,
                                                 op->op);
                vn_neon_build_textured_row_palette(row_palette, &params);
                cached_v8 = v8;
                have_row_palette = VN_TRUE;
            }

            if (op->alpha >= 255u) {
                vn_neon_sample_texels_palette_row(row_ptr,
                                                  g_neon_u_lut,
                                                  vis_w,
                                                  row_palette);
            } else {
                vn_neon_sample_blend_texels_palette_row(row_ptr,
                                                        g_neon_u_lut,
                                                        vis_w,
                                                        row_palette,
                                                        op->alpha);
            }
            continue;
        }

        vn_neon_init_textured_row_params(&params,
                                         v8,
                                         op->tex_id,
                                         op->layer,
                                         op->flags,
                                         op->op);

        if (op->alpha >= 255u) {
            vn_neon_sample_texels_row(row_ptr,
                                      g_neon_u_lut,
                                      vis_w,
                                      &params);
        } else {
            vn_neon_sample_blend_texels_row(row_ptr,
                                            g_neon_u_lut,
                                            vis_w,
                                            &params,
                                            op->alpha);
        }
    }
}

static void vn_neon_draw_textured_rect(const VNRenderOp* op) {
    vn_neon_draw_textured_rect_clipped(op, (const VNRenderRect*)0);
}

static int neon_init(const RendererConfig* cfg) {
    vn_u32 pixels;
    size_t u_lut_bytes;
    size_t v_lut_bytes;

    if (cfg == (const RendererConfig*)0 || cfg->width == 0u || cfg->height == 0u) {
        return VN_E_INVALID_ARG;
    }
    if (vn_neon_runtime_supported() == VN_FALSE) {
        return VN_E_UNSUPPORTED;
    }

    pixels = (vn_u32)cfg->width * (vn_u32)cfg->height;
    if ((cfg->height != 0u) && (pixels / (vn_u32)cfg->height != (vn_u32)cfg->width)) {
        return VN_E_FORMAT;
    }

    g_neon_framebuffer = (vn_u32*)malloc((size_t)pixels * sizeof(vn_u32));
    if (g_neon_framebuffer == (vn_u32*)0) {
        return VN_E_NOMEM;
    }
    u_lut_bytes = (size_t)cfg->width * sizeof(vn_u8);
    v_lut_bytes = (size_t)cfg->height * sizeof(vn_u8);
    g_neon_u_lut = (vn_u8*)malloc(u_lut_bytes);
    g_neon_v_lut = (vn_u8*)malloc(v_lut_bytes);
    if (g_neon_u_lut == (vn_u8*)0 || g_neon_v_lut == (vn_u8*)0) {
        if (g_neon_u_lut != (vn_u8*)0) {
            free(g_neon_u_lut);
        }
        if (g_neon_v_lut != (vn_u8*)0) {
            free(g_neon_v_lut);
        }
        g_neon_u_lut = (vn_u8*)0;
        g_neon_v_lut = (vn_u8*)0;
        free(g_neon_framebuffer);
        g_neon_framebuffer = (vn_u32*)0;
        return VN_E_NOMEM;
    }
    (void)memset(g_neon_framebuffer, 0, (size_t)pixels * sizeof(vn_u32));

    g_neon_cfg = *cfg;
    g_neon_stride = (vn_u32)cfg->width;
    g_neon_height = (vn_u32)cfg->height;
    g_neon_pixels = pixels;
    g_neon_u_lut_cap = (vn_u32)cfg->width;
    g_neon_v_lut_cap = (vn_u32)cfg->height;
    g_neon_ready = VN_TRUE;
    return VN_OK;
}

static void neon_shutdown(void) {
    if (g_neon_framebuffer != (vn_u32*)0) {
        free(g_neon_framebuffer);
    }
    if (g_neon_u_lut != (vn_u8*)0) {
        free(g_neon_u_lut);
    }
    if (g_neon_v_lut != (vn_u8*)0) {
        free(g_neon_v_lut);
    }
    g_neon_framebuffer = (vn_u32*)0;
    g_neon_u_lut = (vn_u8*)0;
    g_neon_v_lut = (vn_u8*)0;
    g_neon_stride = 0u;
    g_neon_height = 0u;
    g_neon_pixels = 0u;
    g_neon_u_lut_cap = 0u;
    g_neon_v_lut_cap = 0u;
    g_neon_cfg.width = 0u;
    g_neon_cfg.height = 0u;
    g_neon_cfg.flags = 0u;
    g_neon_ready = VN_FALSE;
}

static void neon_begin_frame(void) {
}

static int neon_submit_ops(const VNRenderOp* ops, vn_u32 op_count) {
    vn_u32 i;

    if (g_neon_ready == VN_FALSE) {
        return VN_E_RENDER_STATE;
    }
    if (ops == (const VNRenderOp*)0 && op_count != 0u) {
        return VN_E_INVALID_ARG;
    }

    for (i = 0u; i < op_count; ++i) {
        const VNRenderOp* op;
        op = &ops[i];
        if (op->op == VN_OP_CLEAR) {
            vn_neon_clear_rect(op->alpha, (const VNRenderRect*)0);
        } else if (op->op == VN_OP_SPRITE || op->op == VN_OP_TEXT) {
            vn_neon_draw_textured_rect(op);
        } else if (op->op == VN_OP_FADE) {
            vn_neon_fill_rect_uniform(0, 0, g_neon_cfg.width, g_neon_cfg.height, 0xFF000000u, op->alpha);
        } else {
            return VN_E_FORMAT;
        }
    }
    return VN_OK;
}

static int neon_submit_ops_dirty(const VNRenderOp* ops,
                                 vn_u32 op_count,
                                 const VNRenderDirtySubmit* dirty_submit) {
    const VNRenderOp* clear_op;
    vn_u32 rect_index;

    if (g_neon_ready == VN_FALSE) {
        return VN_E_RENDER_STATE;
    }
    if (dirty_submit == (const VNRenderDirtySubmit*)0) {
        return VN_E_INVALID_ARG;
    }
    if (ops == (const VNRenderOp*)0 && op_count != 0u) {
        return VN_E_INVALID_ARG;
    }
    if (dirty_submit->rect_count != 0u && dirty_submit->rects == (const VNRenderRect*)0) {
        return VN_E_INVALID_ARG;
    }
    if (dirty_submit->width != g_neon_cfg.width || dirty_submit->height != g_neon_cfg.height) {
        return VN_E_INVALID_ARG;
    }
    if (dirty_submit->full_redraw != 0u || op_count == 0u) {
        return neon_submit_ops(ops, op_count);
    }
    if (dirty_submit->rect_count == 0u) {
        return VN_OK;
    }
    if (ops[0].op != VN_OP_CLEAR) {
        return neon_submit_ops(ops, op_count);
    }

    clear_op = &ops[0];
    for (rect_index = 0u; rect_index < dirty_submit->rect_count; ++rect_index) {
        const VNRenderRect* clip_rect;
        vn_u32 i;

        clip_rect = &dirty_submit->rects[rect_index];
        vn_neon_clear_rect(clear_op->alpha, clip_rect);
        for (i = 1u; i < op_count; ++i) {
            const VNRenderOp* op;
            op = &ops[i];
            if (op->op == VN_OP_SPRITE || op->op == VN_OP_TEXT) {
                vn_neon_draw_textured_rect_clipped(op, clip_rect);
            } else if (op->op == VN_OP_FADE) {
                vn_neon_fill_rect_uniform_clipped(0,
                                                  0,
                                                  g_neon_cfg.width,
                                                  g_neon_cfg.height,
                                                  0xFF000000u,
                                                  op->alpha,
                                                  clip_rect);
            } else if (op->op != VN_OP_CLEAR) {
                return VN_E_FORMAT;
            }
        }
    }
    return VN_OK;
}

static void neon_end_frame(void) {
}

static void neon_query_caps(VNBackendCaps* out_caps) {
    if (out_caps == (VNBackendCaps*)0) {
        return;
    }
    out_caps->has_simd = 1u;
    out_caps->has_lut_blend = 0u;
    out_caps->has_tmem_cache = 0u;
}

static const VNRenderBackend g_neon_backend = {
    "neon",
    VN_ARCH_NEON,
    neon_init,
    neon_shutdown,
    neon_begin_frame,
    neon_submit_ops,
    neon_end_frame,
    neon_query_caps,
    neon_submit_ops_dirty
};

int vn_register_neon_backend(void) {
    return vn_backend_register(&g_neon_backend);
}

vn_u32 vn_neon_backend_debug_frame_crc32(void) {
    if (g_neon_ready == VN_FALSE) {
        return 0u;
    }
    return vn_pp_frame_crc32(g_neon_framebuffer, g_neon_pixels);
}

vn_u32 vn_neon_backend_debug_copy_framebuffer(vn_u32* out_pixels, vn_u32 pixel_count) {
    if (g_neon_ready == VN_FALSE || out_pixels == (vn_u32*)0 || pixel_count < g_neon_pixels) {
        return 0u;
    }
    (void)memcpy(out_pixels, g_neon_framebuffer, (size_t)g_neon_pixels * sizeof(vn_u32));
    return g_neon_pixels;
}
