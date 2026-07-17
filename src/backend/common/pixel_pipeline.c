#include "vn_backend.h"
#include "vn_error.h"

#include "pixel_pipeline.h"

static vn_u32 g_pp_crc32_table[256];
static int g_pp_crc32_table_ready = VN_FALSE;
static VNTextureLookupFn g_pp_texture_lookup = (VNTextureLookupFn)0;
static void* g_pp_texture_lookup_user = (void*)0;

static int vn_pp_clamp_u8_int(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

static vn_u32 vn_pp_hash32(vn_u32 x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

static void vn_pp_crc32_table_init(void) {
    vn_u32 i;

    if (g_pp_crc32_table_ready != VN_FALSE) {
        return;
    }
    for (i = 0u; i < 256u; ++i) {
        vn_u32 c;
        vn_u32 j;

        c = i;
        for (j = 0u; j < 8u; ++j) {
            if ((c & 1u) != 0u) {
                c = (c >> 1) ^ 0xEDB88320u;
            } else {
                c >>= 1;
            }
        }
        g_pp_crc32_table[i] = c;
    }
    g_pp_crc32_table_ready = VN_TRUE;
}

vn_u32 vn_pp_make_gray(vn_u8 gray) {
    return (vn_u32)(0xFF000000u | ((vn_u32)gray << 16) | ((vn_u32)gray << 8) | (vn_u32)gray);
}

vn_u32 vn_pp_blend_rgb(vn_u32 dst, vn_u32 src, vn_u8 alpha) {
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

vn_u8 vn_pp_mul_alpha(vn_u8 a, vn_u8 b) {
    vn_u32 result;

    result = ((vn_u32)a * (vn_u32)b + 127u) / 255u;
    return (vn_u8)(result & 0xFFu);
}

vn_u32 vn_pp_sample_texel(vn_u16 tex_id, vn_u32 u8, vn_u32 v8) {
    vn_u32 seed;
    vn_u32 h;
    int r;
    int g;
    int b;
    vn_u32 checker;

    u8 &= 0xFFu;
    v8 &= 0xFFu;

    seed = ((vn_u32)tex_id * 2654435761u) ^ (u8 << 8) ^ v8 ^ ((vn_u32)tex_id << 16);
    h = vn_pp_hash32(seed);

    r = (int)(h & 0xFFu);
    g = (int)((h >> 8) & 0xFFu);
    b = (int)((h >> 16) & 0xFFu);

    checker = (((u8 >> 5) ^ (v8 >> 5) ^ ((vn_u32)tex_id & 7u)) & 1u);
    if (checker != 0u) {
        r += 24;
        g += 24;
        b += 24;
    } else if (((u8 + v8) & 0x20u) != 0u) {
        r -= 16;
        g -= 10;
        b -= 16;
    }

    r = vn_pp_clamp_u8_int(r);
    g = vn_pp_clamp_u8_int(g);
    b = vn_pp_clamp_u8_int(b);
    return (vn_u32)(0xFF000000u | ((vn_u32)r << 16) | ((vn_u32)g << 8) | (vn_u32)b);
}

vn_u32 vn_pp_combine_texel(vn_u32 texel, vn_u8 layer, vn_u8 flags, vn_u8 op) {
    int r;
    int g;
    int b;

    r = (int)((texel >> 16) & 0xFFu);
    g = (int)((texel >> 8) & 0xFFu);
    b = (int)(texel & 0xFFu);

    r += (int)layer * 7;
    g += (int)layer * 5;
    b += (int)layer * 3;

    if ((flags & 1u) != 0u) {
        g += 14;
    }
    if ((flags & 2u) != 0u) {
        b += 20;
    }
    if ((flags & 4u) != 0u) {
        r += 28;
        g -= 12;
    }
    if ((flags & 8u) != 0u) {
        r += 12;
        g += 12;
        b -= 8;
    }

    if (op == VN_OP_TEXT) {
        int y;
        y = (r * 54 + g * 183 + b * 19) >> 8;
        r = y + 52;
        g = y + 44;
        b = y + 24 + (int)layer * 6;
    } else if (op == VN_OP_SPRITE) {
        b += 10;
    }

    r = vn_pp_clamp_u8_int(r);
    g = vn_pp_clamp_u8_int(g);
    b = vn_pp_clamp_u8_int(b);
    return (vn_u32)(0xFF000000u | ((vn_u32)r << 16) | ((vn_u32)g << 8) | (vn_u32)b);
}

int vn_texture_source_bind(VNTextureLookupFn lookup, void* user) {
    if (lookup == (VNTextureLookupFn)0) {
        return VN_E_INVALID_ARG;
    }
    g_pp_texture_lookup = lookup;
    g_pp_texture_lookup_user = user;
    return VN_OK;
}

void vn_texture_source_unbind(void) {
    g_pp_texture_lookup = (VNTextureLookupFn)0;
    g_pp_texture_lookup_user = (void*)0;
}

static int vn_pp_clip_resource_rect(vn_u32 framebuffer_width,
                                    vn_u32 framebuffer_height,
                                    const VNRenderOp* op,
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

    if (framebuffer_width == 0u || framebuffer_height == 0u ||
        framebuffer_width > 2147483647u || framebuffer_height > 2147483647u ||
        op == (const VNRenderOp*)0 ||
        out_x0 == (vn_u32*)0 || out_y0 == (vn_u32*)0 ||
        out_x1 == (vn_u32*)0 || out_y1 == (vn_u32*)0) {
        return VN_FALSE;
    }

    x0 = (int)op->x;
    y0 = (int)op->y;
    x1 = x0 + (int)op->w;
    y1 = y0 + (int)op->h;

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
    if (x1 > (int)framebuffer_width) {
        x1 = (int)framebuffer_width;
    }
    if (y1 > (int)framebuffer_height) {
        y1 = (int)framebuffer_height;
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

static int vn_pp_validate_texture_view(const VNTextureView* view) {
    vn_u32 row_bytes;
    vn_u32 available;

    if (view == (const VNTextureView*)0 || view->data == (const vn_u8*)0 ||
        view->width == 0u || view->height == 0u) {
        return VN_E_FORMAT;
    }

    if (view->format == VN_TEXTURE_FORMAT_RGBA16) {
        row_bytes = (vn_u32)view->width * 2u;
        if ((vn_u32)view->height > view->data_size / row_bytes) {
            return VN_E_FORMAT;
        }
        return VN_OK;
    }
    if (view->format == VN_TEXTURE_FORMAT_CI8) {
        if (view->data_size < 512u) {
            return VN_E_FORMAT;
        }
        available = view->data_size - 512u;
        if ((vn_u32)view->height > available / (vn_u32)view->width) {
            return VN_E_FORMAT;
        }
        return VN_OK;
    }
    if (view->format == VN_TEXTURE_FORMAT_IA8) {
        if ((vn_u32)view->height > view->data_size / (vn_u32)view->width) {
            return VN_E_FORMAT;
        }
        return VN_OK;
    }
    return VN_E_UNSUPPORTED;
}

static vn_u8 vn_pp_expand_5_to_8(vn_u32 value) {
    value &= 31u;
    return (vn_u8)((value << 3) | (value >> 2));
}

static vn_u32 vn_pp_decode_rgba16(const vn_u8* data) {
    vn_u32 packed;
    vn_u32 alpha;
    vn_u8 r;
    vn_u8 g;
    vn_u8 b;

    packed = ((vn_u32)data[0] << 8) | (vn_u32)data[1];
    alpha = ((packed & 1u) != 0u) ? 255u : 0u;
    r = vn_pp_expand_5_to_8((packed >> 11) & 31u);
    g = vn_pp_expand_5_to_8((packed >> 6) & 31u);
    b = vn_pp_expand_5_to_8((packed >> 1) & 31u);
    return (vn_u32)((alpha << 24) | ((vn_u32)r << 16) | ((vn_u32)g << 8) | (vn_u32)b);
}

static vn_u32 vn_pp_sample_resource_texel(const VNTextureView* view,
                                           vn_u32 source_index) {
    if (view->format == VN_TEXTURE_FORMAT_RGBA16) {
        return vn_pp_decode_rgba16(view->data + source_index * 2u);
    }
    if (view->format == VN_TEXTURE_FORMAT_CI8) {
        vn_u32 palette_index;
        palette_index = (vn_u32)view->data[512u + source_index];
        return vn_pp_decode_rgba16(view->data + palette_index * 2u);
    }
    {
        vn_u8 packed;
        vn_u32 intensity;
        vn_u32 alpha;
        packed = view->data[source_index];
        intensity = (vn_u32)((packed >> 4) & 15u) * 17u;
        alpha = (vn_u32)(packed & 15u) * 17u;
        return (vn_u32)((alpha << 24) | (intensity << 16) | (intensity << 8) | intensity);
    }
}

static int vn_pp_lookup_resource_texture(vn_u16 texture_id,
                                         VNTextureView* out_view) {
    int rc;

    if (out_view == (VNTextureView*)0) {
        return VN_E_INVALID_ARG;
    }
    if (g_pp_texture_lookup == (VNTextureLookupFn)0) {
        return VN_E_RENDER_STATE;
    }
    out_view->data = (const vn_u8*)0;
    out_view->data_size = 0u;
    out_view->width = 0u;
    out_view->height = 0u;
    out_view->format = 0u;
    rc = g_pp_texture_lookup(g_pp_texture_lookup_user, texture_id, out_view);
    if (rc != VN_OK) {
        return rc;
    }
    return vn_pp_validate_texture_view(out_view);
}

int vn_pp_draw_resource_texture(vn_u32* framebuffer,
                                vn_u32 framebuffer_width,
                                vn_u32 framebuffer_height,
                                const VNRenderOp* op,
                                const VNRenderRect* clip_rect) {
    VNTextureView view;
    vn_u32 x0;
    vn_u32 y0;
    vn_u32 x1;
    vn_u32 y1;
    vn_u32 yy;
    int rc;

    if (framebuffer == (vn_u32*)0 || op == (const VNRenderOp*)0) {
        return VN_E_INVALID_ARG;
    }
    if (op->op != VN_OP_SPRITE && op->op != VN_OP_TEXT) {
        return VN_E_FORMAT;
    }
    if (op->alpha == 0u ||
        vn_pp_clip_resource_rect(framebuffer_width,
                                 framebuffer_height,
                                 op,
                                 clip_rect,
                                 &x0,
                                 &y0,
                                 &x1,
                                 &y1) == VN_FALSE) {
        return VN_OK;
    }
    rc = vn_pp_lookup_resource_texture(op->tex_id, &view);
    if (rc != VN_OK) {
        return rc;
    }

    for (yy = y0; yy < y1; ++yy) {
        vn_u32 local_y;
        vn_u32 source_y;
        vn_u32 source_row;
        vn_u32 framebuffer_row;
        vn_u32 xx;

        local_y = (vn_u32)((int)yy - (int)op->y);
        source_y = (local_y * (vn_u32)view.height) / (vn_u32)op->h;
        if (source_y >= (vn_u32)view.height) {
            source_y = (vn_u32)view.height - 1u;
        }
        source_row = source_y * (vn_u32)view.width;
        framebuffer_row = yy * framebuffer_width;

        for (xx = x0; xx < x1; ++xx) {
            vn_u32 local_x;
            vn_u32 source_x;
            vn_u32 texel;
            vn_u32 color;
            vn_u8 source_alpha;
            vn_u8 effective_alpha;
            vn_u32 framebuffer_index;

            local_x = (vn_u32)((int)xx - (int)op->x);
            source_x = (local_x * (vn_u32)view.width) / (vn_u32)op->w;
            if (source_x >= (vn_u32)view.width) {
                source_x = (vn_u32)view.width - 1u;
            }
            texel = vn_pp_sample_resource_texel(&view, source_row + source_x);
            source_alpha = (vn_u8)((texel >> 24) & 0xFFu);
            effective_alpha = vn_pp_mul_alpha(source_alpha, op->alpha);
            if (effective_alpha == 0u) {
                continue;
            }

            color = 0xFF000000u | (texel & 0x00FFFFFFu);
            framebuffer_index = framebuffer_row + xx;
            if (effective_alpha >= 255u) {
                framebuffer[framebuffer_index] = color;
            } else {
                framebuffer[framebuffer_index] = vn_pp_blend_rgb(framebuffer[framebuffer_index],
                                                                  color,
                                                                  effective_alpha);
            }
        }
    }
    return VN_OK;
}

static int vn_pp_resource_crossfade_pair_is_valid(const VNRenderOp* from_op,
                                                   const VNRenderOp* to_op) {
    if (from_op == (const VNRenderOp*)0 || to_op == (const VNRenderOp*)0) {
        return VN_FALSE;
    }
    if (from_op->op != VN_OP_SPRITE || to_op->op != VN_OP_SPRITE ||
        from_op->flags != (VN_OP_FLAG_RESOURCE_TEXTURE |
                           VN_OP_FLAG_RESOURCE_CROSSFADE_FROM) ||
        to_op->flags != (VN_OP_FLAG_RESOURCE_TEXTURE |
                         VN_OP_FLAG_RESOURCE_CROSSFADE_TO) ||
        from_op->layer != to_op->layer ||
        from_op->x != to_op->x || from_op->y != to_op->y ||
        from_op->w == 0u || from_op->h == 0u ||
        from_op->w != to_op->w || from_op->h != to_op->h ||
        (vn_u32)from_op->alpha + (vn_u32)to_op->alpha != 255u) {
        return VN_FALSE;
    }
    return VN_TRUE;
}

static vn_u32 vn_pp_crossfade_channel(vn_u32 dst,
                                      vn_u32 from,
                                      vn_u32 to,
                                      vn_u32 from_weight,
                                      vn_u32 to_weight,
                                      vn_u32 dst_weight) {
    return (from * from_weight +
            to * to_weight +
            dst * dst_weight + 32512u) / 65025u;
}

static vn_u32 vn_pp_crossfade_pixel(vn_u32 dst,
                                    vn_u32 from,
                                    vn_u32 to,
                                    vn_u8 from_opacity,
                                    vn_u8 to_opacity) {
    vn_u32 from_weight;
    vn_u32 to_weight;
    vn_u32 dst_weight;
    vn_u32 r;
    vn_u32 g;
    vn_u32 b;

    from_weight = ((from >> 24) & 0xFFu) * (vn_u32)from_opacity;
    to_weight = ((to >> 24) & 0xFFu) * (vn_u32)to_opacity;
    dst_weight = 65025u - from_weight - to_weight;
    r = vn_pp_crossfade_channel((dst >> 16) & 0xFFu,
                                (from >> 16) & 0xFFu,
                                (to >> 16) & 0xFFu,
                                from_weight,
                                to_weight,
                                dst_weight);
    g = vn_pp_crossfade_channel((dst >> 8) & 0xFFu,
                                (from >> 8) & 0xFFu,
                                (to >> 8) & 0xFFu,
                                from_weight,
                                to_weight,
                                dst_weight);
    b = vn_pp_crossfade_channel(dst & 0xFFu,
                                from & 0xFFu,
                                to & 0xFFu,
                                from_weight,
                                to_weight,
                                dst_weight);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

int vn_pp_draw_resource_crossfade(vn_u32* framebuffer,
                                  vn_u32 framebuffer_width,
                                  vn_u32 framebuffer_height,
                                  const VNRenderOp* from_op,
                                  const VNRenderOp* to_op,
                                  const VNRenderRect* clip_rect) {
    VNTextureView from_view;
    VNTextureView to_view;
    vn_u32 x0;
    vn_u32 y0;
    vn_u32 x1;
    vn_u32 y1;
    vn_u32 yy;
    int rc;

    if (framebuffer == (vn_u32*)0 ||
        from_op == (const VNRenderOp*)0 || to_op == (const VNRenderOp*)0) {
        return VN_E_INVALID_ARG;
    }
    if (vn_pp_resource_crossfade_pair_is_valid(from_op, to_op) == VN_FALSE) {
        return VN_E_FORMAT;
    }
    if (vn_pp_clip_resource_rect(framebuffer_width,
                                 framebuffer_height,
                                 from_op,
                                 clip_rect,
                                 &x0,
                                 &y0,
                                 &x1,
                                 &y1) == VN_FALSE) {
        return VN_OK;
    }
    rc = vn_pp_lookup_resource_texture(from_op->tex_id, &from_view);
    if (rc != VN_OK) {
        return rc;
    }
    rc = vn_pp_lookup_resource_texture(to_op->tex_id, &to_view);
    if (rc != VN_OK) {
        return rc;
    }

    for (yy = y0; yy < y1; ++yy) {
        vn_u32 local_y;
        vn_u32 from_y;
        vn_u32 to_y;
        vn_u32 from_row;
        vn_u32 to_row;
        vn_u32 framebuffer_row;
        vn_u32 xx;

        local_y = (vn_u32)((int)yy - (int)from_op->y);
        from_y = (local_y * (vn_u32)from_view.height) / (vn_u32)from_op->h;
        to_y = (local_y * (vn_u32)to_view.height) / (vn_u32)to_op->h;
        if (from_y >= (vn_u32)from_view.height) {
            from_y = (vn_u32)from_view.height - 1u;
        }
        if (to_y >= (vn_u32)to_view.height) {
            to_y = (vn_u32)to_view.height - 1u;
        }
        from_row = from_y * (vn_u32)from_view.width;
        to_row = to_y * (vn_u32)to_view.width;
        framebuffer_row = yy * framebuffer_width;

        for (xx = x0; xx < x1; ++xx) {
            vn_u32 local_x;
            vn_u32 from_x;
            vn_u32 to_x;
            vn_u32 from_texel;
            vn_u32 to_texel;
            vn_u32 framebuffer_index;

            local_x = (vn_u32)((int)xx - (int)from_op->x);
            from_x = (local_x * (vn_u32)from_view.width) / (vn_u32)from_op->w;
            to_x = (local_x * (vn_u32)to_view.width) / (vn_u32)to_op->w;
            if (from_x >= (vn_u32)from_view.width) {
                from_x = (vn_u32)from_view.width - 1u;
            }
            if (to_x >= (vn_u32)to_view.width) {
                to_x = (vn_u32)to_view.width - 1u;
            }
            from_texel = vn_pp_sample_resource_texel(&from_view, from_row + from_x);
            to_texel = vn_pp_sample_resource_texel(&to_view, to_row + to_x);
            framebuffer_index = framebuffer_row + xx;
            framebuffer[framebuffer_index] = vn_pp_crossfade_pixel(
                framebuffer[framebuffer_index],
                from_texel,
                to_texel,
                from_op->alpha,
                to_op->alpha);
        }
    }
    return VN_OK;
}

vn_u32 vn_pp_frame_crc32(const vn_u32* pixels, vn_u32 count) {
    const vn_u8* p;
    vn_u32 bytes;
    vn_u32 crc;
    vn_u32 i;

    if (pixels == (const vn_u32*)0 || count == 0u) {
        return 0u;
    }

    if (count > (0xFFFFFFFFu / 4u)) {
        return 0u;
    }

    vn_pp_crc32_table_init();

    p = (const vn_u8*)(const void*)pixels;
    bytes = count * 4u;
    crc = 0xFFFFFFFFu;
    for (i = 0u; i < bytes; ++i) {
        vn_u32 idx;
        idx = (vn_u32)((crc ^ (vn_u32)p[i]) & 0xFFu);
        crc = g_pp_crc32_table[idx] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}
