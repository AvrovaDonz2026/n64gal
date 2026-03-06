#include "vn_backend.h"

#include "pixel_pipeline.h"

static vn_u32 g_pp_crc32_table[256];
static int g_pp_crc32_table_ready = VN_FALSE;

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
