#include <stdio.h>
#include <string.h>

#include "vn_renderer.h"
#include "vn_error.h"
#include "../../src/core/build_config.h"

#define VN_PARITY_WIDTH 320u
#define VN_PARITY_HEIGHT 128u
#define VN_PARITY_PIXELS (VN_PARITY_WIDTH * VN_PARITY_HEIGHT)

vn_u32 vn_scalar_backend_debug_frame_crc32(void);
vn_u32 vn_avx2_backend_debug_frame_crc32(void);

vn_u32 vn_scalar_backend_debug_copy_framebuffer(vn_u32* out_pixels, vn_u32 pixel_count);
vn_u32 vn_avx2_backend_debug_copy_framebuffer(vn_u32* out_pixels, vn_u32 pixel_count);

typedef void (*VNCaseBuilder)(VNRenderOp* ops, vn_u32* out_count);

static void vn_fill_clear_op(VNRenderOp* op, vn_u8 gray) {
    if (op == (VNRenderOp*)0) {
        return;
    }

    op->op = VN_OP_CLEAR;
    op->layer = 0u;
    op->tex_id = 0u;
    op->x = 0;
    op->y = 0;
    op->w = 0u;
    op->h = 0u;
    op->alpha = gray;
    op->flags = 0u;
}

static void vn_fill_textured_op(VNRenderOp* op,
                                vn_u8 kind,
                                vn_i16 x,
                                vn_i16 y,
                                vn_u16 w,
                                vn_u16 h,
                                vn_u8 alpha,
                                vn_u8 layer,
                                vn_u16 tex_id,
                                vn_u8 flags) {
    if (op == (VNRenderOp*)0) {
        return;
    }

    op->op = kind;
    op->layer = layer;
    op->tex_id = tex_id;
    op->x = x;
    op->y = y;
    op->w = w;
    op->h = h;
    op->alpha = alpha;
    op->flags = flags;
}

static void build_case_sprite_direct_opaque(VNRenderOp* ops, vn_u32* out_count) {
    if (ops == (VNRenderOp*)0 || out_count == (vn_u32*)0) {
        return;
    }

    vn_fill_clear_op(&ops[0], 40u);
    vn_fill_textured_op(&ops[1], VN_OP_SPRITE, 16, 10, 256u, 48u, 255u, 1u, 21u, 1u);
    *out_count = 2u;
}

static void build_case_text_direct_alpha(VNRenderOp* ops, vn_u32* out_count) {
    if (ops == (VNRenderOp*)0 || out_count == (vn_u32*)0) {
        return;
    }

    vn_fill_clear_op(&ops[0], 18u);
    vn_fill_textured_op(&ops[1], VN_OP_TEXT, 12, 20, 256u, 40u, 192u, 2u, 88u, 9u);
    *out_count = 2u;
}

static void build_case_sprite_palette_opaque(VNRenderOp* ops, vn_u32* out_count) {
    if (ops == (VNRenderOp*)0 || out_count == (vn_u32*)0) {
        return;
    }

    vn_fill_clear_op(&ops[0], 64u);
    vn_fill_textured_op(&ops[1], VN_OP_SPRITE, 8, 18, 257u, 64u, 255u, 2u, 31u, 3u);
    *out_count = 2u;
}

static void build_case_text_palette_alpha(VNRenderOp* ops, vn_u32* out_count) {
    if (ops == (VNRenderOp*)0 || out_count == (vn_u32*)0) {
        return;
    }

    vn_fill_clear_op(&ops[0], 22u);
    vn_fill_textured_op(&ops[1], VN_OP_TEXT, 14, 24, 257u, 48u, 176u, 3u, 101u, 9u);
    *out_count = 2u;
}

static int debug_copy_for_backend(const char* backend_name,
                                  vn_u32* out_pixels,
                                  vn_u32 pixel_count,
                                  vn_u32* out_crc) {
    if (backend_name == (const char*)0 || out_pixels == (vn_u32*)0 || out_crc == (vn_u32*)0) {
        return 1;
    }
    if (strcmp(backend_name, "scalar") == 0) {
        *out_crc = vn_scalar_backend_debug_frame_crc32();
        return (vn_scalar_backend_debug_copy_framebuffer(out_pixels, pixel_count) == pixel_count) ? 0 : 1;
    }
    if (strcmp(backend_name, "avx2") == 0) {
        *out_crc = vn_avx2_backend_debug_frame_crc32();
        return (vn_avx2_backend_debug_copy_framebuffer(out_pixels, pixel_count) == pixel_count) ? 0 : 1;
    }

    (void)fprintf(stderr, "unsupported debug backend=%s\n", backend_name);
    return 1;
}

static int render_case(vn_u32 flags,
                       const VNRenderOp* ops,
                       vn_u32 op_count,
                       char* out_backend,
                       size_t out_backend_cap,
                       vn_u32* out_crc,
                       vn_u32* out_pixels) {
    RendererConfig cfg;
    const char* backend_name;
    int rc;

    if (ops == (const VNRenderOp*)0 ||
        out_backend == (char*)0 ||
        out_backend_cap == 0u ||
        out_crc == (vn_u32*)0 ||
        out_pixels == (vn_u32*)0) {
        return 1;
    }

    cfg.width = VN_PARITY_WIDTH;
    cfg.height = VN_PARITY_HEIGHT;
    cfg.flags = flags;
    rc = renderer_init(&cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "renderer_init failed rc=%d flags=0x%X\n", rc, (unsigned int)flags);
        return 1;
    }

    backend_name = renderer_backend_name();
    (void)strncpy(out_backend, backend_name, out_backend_cap - 1u);
    out_backend[out_backend_cap - 1u] = '\0';

    renderer_begin_frame();
    renderer_submit(ops, op_count);
    renderer_end_frame();

    rc = debug_copy_for_backend(backend_name, out_pixels, VN_PARITY_PIXELS, out_crc);
    renderer_shutdown();
    return rc;
}

static int compare_pixels(const char* case_name,
                          const vn_u32* expected_pixels,
                          const vn_u32* actual_pixels,
                          vn_u32 pixel_count) {
    vn_u32 i;

    if (case_name == (const char*)0 ||
        expected_pixels == (const vn_u32*)0 ||
        actual_pixels == (const vn_u32*)0) {
        return 1;
    }

    for (i = 0u; i < pixel_count; ++i) {
        if (expected_pixels[i] != actual_pixels[i]) {
            (void)fprintf(stderr,
                          "case=%s pixel mismatch idx=%u expected=0x%08X actual=0x%08X\n",
                          case_name,
                          (unsigned int)i,
                          (unsigned int)expected_pixels[i],
                          (unsigned int)actual_pixels[i]);
            return 1;
        }
    }
    return 0;
}

static int compare_case(const char* case_name,
                        VNCaseBuilder builder,
                        int* out_compared_count) {
    VNRenderOp ops[2];
    vn_u32 op_count;
    vn_u32 scalar_crc;
    vn_u32 avx2_crc;
    vn_u32 scalar_pixels[VN_PARITY_PIXELS];
    vn_u32 avx2_pixels[VN_PARITY_PIXELS];
    char backend_name[16];
    int rc;

    if (case_name == (const char*)0 || builder == (VNCaseBuilder)0 || out_compared_count == (int*)0) {
        return 1;
    }

    op_count = 0u;
    builder(ops, &op_count);
    if (op_count == 0u) {
        (void)fprintf(stderr, "case=%s missing ops\n", case_name);
        return 1;
    }

    scalar_crc = 0u;
    rc = render_case(VN_RENDERER_FLAG_FORCE_SCALAR,
                     ops,
                     op_count,
                     backend_name,
                     sizeof(backend_name),
                     &scalar_crc,
                     scalar_pixels);
    if (rc != 0) {
        return 1;
    }
    if (strcmp(backend_name, "scalar") != 0) {
        (void)fprintf(stderr, "case=%s forced scalar got=%s\n", case_name, backend_name);
        return 1;
    }

    avx2_crc = 0u;
    rc = render_case(VN_RENDERER_FLAG_FORCE_AVX2,
                     ops,
                     op_count,
                     backend_name,
                     sizeof(backend_name),
                     &avx2_crc,
                     avx2_pixels);
    if (rc != 0) {
        return 1;
    }
    if (strcmp(backend_name, "scalar") == 0) {
        return 2;
    }
    if (strcmp(backend_name, "avx2") != 0) {
        (void)fprintf(stderr, "case=%s forced avx2 got=%s\n", case_name, backend_name);
        return 1;
    }
    if (scalar_crc != avx2_crc) {
        (void)fprintf(stderr,
                      "case=%s crc mismatch scalar=0x%08X avx2=0x%08X\n",
                      case_name,
                      (unsigned int)scalar_crc,
                      (unsigned int)avx2_crc);
        return 1;
    }
    if (compare_pixels(case_name, scalar_pixels, avx2_pixels, VN_PARITY_PIXELS) != 0) {
        return 1;
    }

    *out_compared_count += 1;
    (void)printf("test_avx2_fastpath_parity matched case=%s crc=0x%08X\n",
                 case_name,
                 (unsigned int)scalar_crc);
    return 0;
}

int main(void) {
    int compared_count;
    int rc;

#if !(VN_ARCH_X64 || VN_ARCH_X86)
    (void)printf("test_avx2_fastpath_parity skipped (non-x86 host)\n");
    return 0;
#endif

    compared_count = 0;

    rc = compare_case("sprite_direct_opaque_256", build_case_sprite_direct_opaque, &compared_count);
    if (rc == 2) {
        (void)printf("test_avx2_fastpath_parity skipped (no avx2 support)\n");
        return 0;
    }
    if (rc != 0) {
        return 1;
    }
    if (compare_case("text_direct_alpha_256", build_case_text_direct_alpha, &compared_count) != 0) {
        return 1;
    }
    if (compare_case("sprite_palette_opaque_257", build_case_sprite_palette_opaque, &compared_count) != 0) {
        return 1;
    }
    if (compare_case("text_palette_alpha_257", build_case_text_palette_alpha, &compared_count) != 0) {
        return 1;
    }

    (void)printf("test_avx2_fastpath_parity ok compared=%d\n", compared_count);
    return 0;
}
