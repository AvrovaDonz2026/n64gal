#include <stdio.h>
#include <string.h>

#include "vn_renderer.h"
#include "vn_runtime.h"
#include "vn_error.h"
#include "../../src/backend/common/pixel_pipeline.h"
#include "../../src/core/runtime_texture.h"

#define TEST_WIDTH 8u
#define TEST_HEIGHT 6u
#define TEST_PIXELS (TEST_WIDTH * TEST_HEIGHT)

typedef struct {
    vn_u8 rgba16[8];
    vn_u8 ci8[514];
    vn_u8 ia8[2];
    vn_u8 truncated[1];
    vn_u8 crossfade_new[8];
} TestTextureSource;

static void test_source_init(TestTextureSource* source) {
    if (source == (TestTextureSource*)0) {
        return;
    }
    (void)memset(source, 0, sizeof(*source));

    source->rgba16[0] = 0xF8u;
    source->rgba16[1] = 0x01u;
    source->rgba16[2] = 0x07u;
    source->rgba16[3] = 0xC1u;
    source->rgba16[4] = 0x00u;
    source->rgba16[5] = 0x3Fu;
    source->rgba16[6] = 0xFFu;
    source->rgba16[7] = 0xFEu;

    source->ci8[2] = 0xFFu;
    source->ci8[3] = 0xC1u;
    source->ci8[4] = 0xF8u;
    source->ci8[5] = 0x3Fu;
    source->ci8[512] = 1u;
    source->ci8[513] = 2u;

    source->ia8[0] = 0xFFu;
    source->ia8[1] = 0x88u;
    source->truncated[0] = 0u;

    source->crossfade_new[0] = 0xF8u;
    source->crossfade_new[1] = 0x01u;
    source->crossfade_new[2] = 0x07u;
    source->crossfade_new[3] = 0xC1u;
    source->crossfade_new[4] = 0x00u;
    source->crossfade_new[5] = 0x3Eu;
    source->crossfade_new[6] = 0xFFu;
    source->crossfade_new[7] = 0xFEu;
}

static int test_texture_lookup(void* user,
                               vn_u16 tex_id,
                               VNTextureView* out_view) {
    TestTextureSource* source;

    if (user == (void*)0 || out_view == (VNTextureView*)0) {
        return VN_E_INVALID_ARG;
    }
    source = (TestTextureSource*)user;

    if (tex_id == 1u) {
        out_view->data = source->rgba16;
        out_view->data_size = (vn_u32)sizeof(source->rgba16);
        out_view->width = 2u;
        out_view->height = 2u;
        out_view->format = VN_TEXTURE_FORMAT_RGBA16;
        return VN_OK;
    }
    if (tex_id == 2u) {
        out_view->data = source->ci8;
        out_view->data_size = (vn_u32)sizeof(source->ci8);
        out_view->width = 2u;
        out_view->height = 1u;
        out_view->format = VN_TEXTURE_FORMAT_CI8;
        return VN_OK;
    }
    if (tex_id == 3u) {
        out_view->data = source->ia8;
        out_view->data_size = (vn_u32)sizeof(source->ia8);
        out_view->width = 2u;
        out_view->height = 1u;
        out_view->format = VN_TEXTURE_FORMAT_IA8;
        return VN_OK;
    }
    if (tex_id == 4u) {
        out_view->data = source->truncated;
        out_view->data_size = (vn_u32)sizeof(source->truncated);
        out_view->width = 2u;
        out_view->height = 2u;
        out_view->format = VN_TEXTURE_FORMAT_RGBA16;
        return VN_OK;
    }
    if (tex_id == 5u) {
        out_view->data = source->truncated;
        out_view->data_size = (vn_u32)sizeof(source->truncated);
        out_view->width = 1u;
        out_view->height = 1u;
        out_view->format = 99u;
        return VN_OK;
    }
    if (tex_id == 6u) {
        out_view->data = source->crossfade_new;
        out_view->data_size = (vn_u32)sizeof(source->crossfade_new);
        out_view->width = 2u;
        out_view->height = 2u;
        out_view->format = VN_TEXTURE_FORMAT_RGBA16;
        return VN_OK;
    }
    return VN_E_FORMAT;
}

static void fill_pixels(vn_u32* pixels, vn_u32 count, vn_u32 value) {
    vn_u32 i;
    for (i = 0u; i < count; ++i) {
        pixels[i] = value;
    }
}

static void fill_clear_op(VNRenderOp* op, vn_u8 gray) {
    (void)memset(op, 0, sizeof(*op));
    op->op = VN_OP_CLEAR;
    op->alpha = gray;
}

static void fill_resource_op(VNRenderOp* op,
                             vn_u8 kind,
                             vn_u16 tex_id,
                             vn_i16 x,
                             vn_i16 y,
                             vn_u16 w,
                             vn_u16 h,
                             vn_u8 alpha) {
    (void)memset(op, 0, sizeof(*op));
    op->op = kind;
    op->layer = 7u;
    op->tex_id = tex_id;
    op->x = x;
    op->y = y;
    op->w = w;
    op->h = h;
    op->alpha = alpha;
    op->flags = (vn_u8)(VN_OP_FLAG_RESOURCE_TEXTURE | 15u);
}

static int expect_pixel(const char* label,
                        const vn_u32* pixels,
                        vn_u32 index,
                        vn_u32 expected) {
    if (pixels[index] != expected) {
        (void)fprintf(stderr,
                      "%s pixel=%u expected=0x%08X actual=0x%08X\n",
                      label,
                      (unsigned int)index,
                      (unsigned int)expected,
                      (unsigned int)pixels[index]);
        return 1;
    }
    return 0;
}

static int test_reference_formats(TestTextureSource* source) {
    vn_u32 pixels[16];
    VNRenderOp op;
    int rc;

    rc = vn_texture_source_bind(test_texture_lookup, source);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "texture bind failed rc=%d\n", rc);
        return 1;
    }

    fill_pixels(pixels, 16u, 0xFF202020u);
    fill_resource_op(&op, VN_OP_SPRITE, 1u, 0, 0, 4u, 4u, 255u);
    rc = vn_pp_draw_resource_texture(pixels, 4u, 4u, &op, (const VNRenderRect*)0);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "rgba16 draw failed rc=%d\n", rc);
        return 1;
    }
    if (expect_pixel("rgba16 red scale", pixels, 0u, 0xFFFF0000u) != 0 ||
        expect_pixel("rgba16 red repeat", pixels, 5u, 0xFFFF0000u) != 0 ||
        expect_pixel("rgba16 green", pixels, 3u, 0xFF00FF00u) != 0 ||
        expect_pixel("rgba16 blue", pixels, 12u, 0xFF0000FFu) != 0 ||
        expect_pixel("rgba16 transparent", pixels, 15u, 0xFF202020u) != 0) {
        return 1;
    }

    fill_pixels(pixels, 16u, 0xFF000000u);
    fill_resource_op(&op, VN_OP_TEXT, 1u, 0, 0, 2u, 1u, 128u);
    rc = vn_pp_draw_resource_texture(pixels, 4u, 4u, &op, (const VNRenderRect*)0);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "rgba16 alpha draw failed rc=%d\n", rc);
        return 1;
    }
    if (expect_pixel("rgba16 text raw red", pixels, 0u, 0xFF800000u) != 0 ||
        expect_pixel("rgba16 text raw green", pixels, 1u, 0xFF008000u) != 0) {
        return 1;
    }

    fill_pixels(pixels, 16u, 0xFF000000u);
    fill_resource_op(&op, VN_OP_SPRITE, 2u, 0, 0, 4u, 1u, 255u);
    rc = vn_pp_draw_resource_texture(pixels, 4u, 4u, &op, (const VNRenderRect*)0);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "ci8 draw failed rc=%d\n", rc);
        return 1;
    }
    if (expect_pixel("ci8 yellow", pixels, 0u, 0xFFFFFF00u) != 0 ||
        expect_pixel("ci8 yellow repeat", pixels, 1u, 0xFFFFFF00u) != 0 ||
        expect_pixel("ci8 magenta", pixels, 2u, 0xFFFF00FFu) != 0 ||
        expect_pixel("ci8 magenta repeat", pixels, 3u, 0xFFFF00FFu) != 0) {
        return 1;
    }

    fill_pixels(pixels, 16u, 0xFF000000u);
    fill_resource_op(&op, VN_OP_SPRITE, 3u, 0, 0, 2u, 1u, 128u);
    rc = vn_pp_draw_resource_texture(pixels, 4u, 4u, &op, (const VNRenderRect*)0);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "ia8 draw failed rc=%d\n", rc);
        return 1;
    }
    if (expect_pixel("ia8 opaque", pixels, 0u, 0xFF808080u) != 0 ||
        expect_pixel("ia8 source alpha", pixels, 1u, 0xFF242424u) != 0) {
        return 1;
    }

    fill_resource_op(&op, VN_OP_SPRITE, 4u, 0, 0, 1u, 1u, 255u);
    rc = vn_pp_draw_resource_texture(pixels, 4u, 4u, &op, (const VNRenderRect*)0);
    if (rc != VN_E_FORMAT) {
        (void)fprintf(stderr, "truncated texture expected format rc=%d\n", rc);
        return 1;
    }
    fill_resource_op(&op, VN_OP_SPRITE, 5u, 0, 0, 1u, 1u, 255u);
    rc = vn_pp_draw_resource_texture(pixels, 4u, 4u, &op, (const VNRenderRect*)0);
    if (rc != VN_E_UNSUPPORTED) {
        (void)fprintf(stderr, "unknown texture format expected unsupported rc=%d\n", rc);
        return 1;
    }
    return 0;
}

static int render_frame(vn_u32 flags,
                        const VNRenderOp* ops,
                        vn_u32 op_count,
                        int use_dirty,
                        vn_u32* out_pixels,
                        char* out_backend,
                        vn_u32 backend_cap) {
    RendererConfig cfg;
    const vn_u32* framebuffer;
    const char* backend_name;
    vn_u32 width;
    vn_u32 height;
    int rc;

    cfg.width = TEST_WIDTH;
    cfg.height = TEST_HEIGHT;
    cfg.flags = flags;
    rc = renderer_init(&cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "renderer init failed rc=%d flags=0x%X\n", rc, (unsigned int)flags);
        return 1;
    }

    renderer_begin_frame();
    if (use_dirty != 0) {
        VNRenderRect rect;
        VNRenderDirtySubmit dirty;
        rect.x = 0u;
        rect.y = 0u;
        rect.w = TEST_WIDTH;
        rect.h = TEST_HEIGHT;
        dirty.width = TEST_WIDTH;
        dirty.height = TEST_HEIGHT;
        dirty.rect_count = 1u;
        dirty.full_redraw = 0u;
        dirty.rects = &rect;
        renderer_submit_dirty(ops, op_count, &dirty);
    } else {
        renderer_submit(ops, op_count);
    }
    renderer_end_frame();

    framebuffer = (const vn_u32*)0;
    width = 0u;
    height = 0u;
    rc = renderer_get_framebuffer(&framebuffer, &width, &height);
    if (rc != VN_OK || framebuffer == (const vn_u32*)0 || width != TEST_WIDTH || height != TEST_HEIGHT) {
        (void)fprintf(stderr,
                      "framebuffer view failed rc=%d width=%u height=%u\n",
                      rc,
                      (unsigned int)width,
                      (unsigned int)height);
        renderer_shutdown();
        return 1;
    }
    (void)memcpy(out_pixels, framebuffer, sizeof(vn_u32) * TEST_PIXELS);
    backend_name = renderer_backend_name();
    if (out_backend != (char*)0 && backend_cap != 0u) {
        (void)strncpy(out_backend, backend_name, (size_t)backend_cap - 1u);
        out_backend[backend_cap - 1u] = '\0';
    }
    renderer_shutdown();
    return 0;
}

static int compare_pixels(const char* label,
                          const vn_u32* expected,
                          const vn_u32* actual) {
    vn_u32 i;
    for (i = 0u; i < TEST_PIXELS; ++i) {
        if (expected[i] != actual[i]) {
            (void)fprintf(stderr,
                          "%s mismatch pixel=%u expected=0x%08X actual=0x%08X\n",
                          label,
                          (unsigned int)i,
                          (unsigned int)expected[i],
                          (unsigned int)actual[i]);
            return 1;
        }
    }
    return 0;
}

static int test_backend_parity(void) {
    static const vn_u32 flags[] = {
        VN_RENDERER_FLAG_FORCE_AVX2,
        VN_RENDERER_FLAG_FORCE_NEON,
        VN_RENDERER_FLAG_FORCE_RVV
    };
    static const char* names[] = {"avx2", "neon", "rvv"};
    VNRenderOp ops[4];
    vn_u32 scalar_pixels[TEST_PIXELS];
    vn_u32 backend_pixels[TEST_PIXELS];
    char backend_name[16];
    vn_u32 i;
    int compared;

    fill_clear_op(&ops[0], 24u);
    fill_resource_op(&ops[1], VN_OP_SPRITE, 1u, -1, 0, 4u, 4u, 220u);
    fill_resource_op(&ops[2], VN_OP_TEXT, 2u, 3, 1, 5u, 2u, 255u);
    fill_resource_op(&ops[3], VN_OP_SPRITE, 3u, 1, 4, 6u, 2u, 173u);

    if (render_frame(VN_RENDERER_FLAG_FORCE_SCALAR,
                     ops,
                     4u,
                     VN_FALSE,
                     scalar_pixels,
                     backend_name,
                     sizeof(backend_name)) != 0) {
        return 1;
    }
    if (strcmp(backend_name, "scalar") != 0) {
        (void)fprintf(stderr, "forced scalar got backend=%s\n", backend_name);
        return 1;
    }

    compared = 0;
    for (i = 0u; i < 3u; ++i) {
        if (render_frame(flags[i],
                         ops,
                         4u,
                         VN_FALSE,
                         backend_pixels,
                         backend_name,
                         sizeof(backend_name)) != 0) {
            return 1;
        }
        if (strcmp(backend_name, names[i]) != 0) {
            if (strcmp(backend_name, "scalar") == 0) {
                continue;
            }
            (void)fprintf(stderr, "requested backend=%s got=%s\n", names[i], backend_name);
            return 1;
        }
        if (compare_pixels(names[i], scalar_pixels, backend_pixels) != 0) {
            return 1;
        }
        compared += 1;
    }
    (void)printf("test_resource_texture_backend parity compared=%d\n", compared);
    return 0;
}

static void fill_crossfade_ops(VNRenderOp* ops) {
    fill_clear_op(&ops[0], 0u);
    fill_resource_op(&ops[1], VN_OP_SPRITE, 1u, 0, 0, 2u, 2u, 127u);
    fill_resource_op(&ops[2], VN_OP_SPRITE, 6u, 0, 0, 2u, 2u, 128u);
    ops[1].layer = 0u;
    ops[2].layer = 0u;
    ops[1].flags = (vn_u8)(VN_OP_FLAG_RESOURCE_TEXTURE |
                            VN_OP_FLAG_RESOURCE_CROSSFADE_FROM);
    ops[2].flags = (vn_u8)(VN_OP_FLAG_RESOURCE_TEXTURE |
                            VN_OP_FLAG_RESOURCE_CROSSFADE_TO);
}

static int test_resource_crossfade(void) {
    static const vn_u32 flags[] = {
        VN_RENDERER_FLAG_FORCE_AVX2,
        VN_RENDERER_FLAG_FORCE_NEON,
        VN_RENDERER_FLAG_FORCE_RVV
    };
    VNRenderOp ops[3];
    VNRenderOp invalid_to;
    vn_u32 scalar_pixels[TEST_PIXELS];
    vn_u32 actual_pixels[TEST_PIXELS];
    vn_u32 direct_pixels[4];
    char backend_name[16];
    vn_u32 i;
    int rc;

    fill_crossfade_ops(ops);
    fill_pixels(direct_pixels, 4u, 0xFF000000u);
    rc = vn_pp_draw_resource_crossfade(direct_pixels,
                                       2u,
                                       2u,
                                       &ops[1],
                                       &ops[2],
                                       (const VNRenderRect*)0);
    if (rc != VN_OK ||
        direct_pixels[0] != 0xFFFF0000u ||
        direct_pixels[1] != 0xFF00FF00u ||
        direct_pixels[2] != 0xFF00007Fu ||
        direct_pixels[3] != 0xFF000000u) {
        (void)fprintf(stderr,
                      "resource crossfade pixels mismatch rc=%d p0=0x%08X p2=0x%08X\n",
                      rc,
                      (unsigned int)direct_pixels[0],
                      (unsigned int)direct_pixels[2]);
        return 1;
    }
    ops[1].alpha = 255u;
    ops[2].alpha = 0u;
    fill_pixels(direct_pixels, 4u, 0xFF000000u);
    rc = vn_pp_draw_resource_crossfade(direct_pixels,
                                       2u,
                                       2u,
                                       &ops[1],
                                       &ops[2],
                                       (const VNRenderRect*)0);
    if (rc != VN_OK || direct_pixels[2] != 0xFF0000FFu) {
        (void)fprintf(stderr, "resource crossfade start endpoint mismatch rc=%d\n", rc);
        return 1;
    }
    ops[1].alpha = 0u;
    ops[2].alpha = 255u;
    fill_pixels(direct_pixels, 4u, 0xFF000000u);
    rc = vn_pp_draw_resource_crossfade(direct_pixels,
                                       2u,
                                       2u,
                                       &ops[1],
                                       &ops[2],
                                       (const VNRenderRect*)0);
    if (rc != VN_OK || direct_pixels[2] != 0xFF000000u) {
        (void)fprintf(stderr, "resource crossfade end endpoint mismatch rc=%d\n", rc);
        return 1;
    }
    ops[1].alpha = 127u;
    ops[2].alpha = 128u;
    invalid_to = ops[2];
    invalid_to.w = 1u;
    if (vn_pp_draw_resource_crossfade(direct_pixels,
                                      2u,
                                      2u,
                                      &ops[1],
                                      &invalid_to,
                                      (const VNRenderRect*)0) != VN_E_FORMAT) {
        (void)fprintf(stderr, "malformed resource crossfade pair not rejected\n");
        return 1;
    }

    if (render_frame(VN_RENDERER_FLAG_FORCE_SCALAR,
                     ops,
                     3u,
                     VN_FALSE,
                     scalar_pixels,
                     backend_name,
                     (vn_u32)sizeof(backend_name)) != 0 ||
        scalar_pixels[0] != 0xFFFF0000u ||
        scalar_pixels[1] != 0xFF00FF00u ||
        scalar_pixels[TEST_WIDTH] != 0xFF00007Fu ||
        scalar_pixels[TEST_WIDTH + 1u] != 0xFF000000u) {
        (void)fprintf(stderr, "scalar resource crossfade dispatch mismatch\n");
        return 1;
    }
    if (render_frame(VN_RENDERER_FLAG_FORCE_SCALAR,
                     ops,
                     3u,
                     VN_TRUE,
                     actual_pixels,
                     backend_name,
                     (vn_u32)sizeof(backend_name)) != 0 ||
        compare_pixels("scalar dirty crossfade", scalar_pixels, actual_pixels) != 0) {
        return 1;
    }
    for (i = 0u; i < 3u; ++i) {
        if (render_frame(flags[i],
                         ops,
                         3u,
                         VN_FALSE,
                         actual_pixels,
                         backend_name,
                         (vn_u32)sizeof(backend_name)) != 0 ||
            compare_pixels("backend crossfade", scalar_pixels, actual_pixels) != 0) {
            return 1;
        }
        if (render_frame(flags[i],
                         ops,
                         3u,
                         VN_TRUE,
                         actual_pixels,
                         backend_name,
                         (vn_u32)sizeof(backend_name)) != 0 ||
            compare_pixels("backend dirty crossfade", scalar_pixels, actual_pixels) != 0) {
            return 1;
        }
    }
    return 0;
}

static int render_two_frames(int use_dirty, vn_u32* out_pixels) {
    RendererConfig cfg;
    VNRenderOp first_ops[2];
    VNRenderOp second_ops[2];
    VNRenderRect rect;
    VNRenderDirtySubmit dirty;
    const vn_u32* framebuffer;
    vn_u32 width;
    vn_u32 height;
    int rc;

    cfg.width = TEST_WIDTH;
    cfg.height = TEST_HEIGHT;
    cfg.flags = VN_RENDERER_FLAG_FORCE_SCALAR;
    rc = renderer_init(&cfg);
    if (rc != VN_OK) {
        return 1;
    }

    fill_clear_op(&first_ops[0], 33u);
    fill_resource_op(&first_ops[1], VN_OP_SPRITE, 1u, 0, 1, 4u, 4u, 255u);
    fill_clear_op(&second_ops[0], 33u);
    fill_resource_op(&second_ops[1], VN_OP_SPRITE, 2u, 4, 1, 4u, 4u, 255u);

    renderer_begin_frame();
    renderer_submit(first_ops, 2u);
    renderer_end_frame();

    rect.x = 0u;
    rect.y = 1u;
    rect.w = 8u;
    rect.h = 4u;
    dirty.width = TEST_WIDTH;
    dirty.height = TEST_HEIGHT;
    dirty.rect_count = 1u;
    dirty.full_redraw = 0u;
    dirty.rects = &rect;

    renderer_begin_frame();
    if (use_dirty != 0) {
        renderer_submit_dirty(second_ops, 2u, &dirty);
    } else {
        renderer_submit(second_ops, 2u);
    }
    renderer_end_frame();

    rc = renderer_get_framebuffer(&framebuffer, &width, &height);
    if (rc != VN_OK || width != TEST_WIDTH || height != TEST_HEIGHT) {
        renderer_shutdown();
        return 1;
    }
    (void)memcpy(out_pixels, framebuffer, sizeof(vn_u32) * TEST_PIXELS);
    renderer_shutdown();
    return 0;
}

static int test_dirty_clip(void) {
    vn_u32 full_pixels[TEST_PIXELS];
    vn_u32 dirty_pixels[TEST_PIXELS];

    if (render_two_frames(VN_FALSE, full_pixels) != 0 ||
        render_two_frames(VN_TRUE, dirty_pixels) != 0) {
        (void)fprintf(stderr, "dirty render sequence failed\n");
        return 1;
    }
    return compare_pixels("dirty resource texture", full_pixels, dirty_pixels);
}

static int test_framebuffer_state(void) {
    const vn_u32* pixels;
    vn_u32 width;
    vn_u32 height;
    int rc;

    pixels = (const vn_u32*)1;
    width = 1u;
    height = 1u;
    rc = renderer_get_framebuffer(&pixels, &width, &height);
    if (rc != VN_E_RENDER_STATE || pixels != (const vn_u32*)0 || width != 0u || height != 0u) {
        (void)fprintf(stderr, "uninitialized framebuffer state mismatch rc=%d\n", rc);
        return 1;
    }
    if (renderer_get_framebuffer((const vn_u32**)0, &width, &height) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "framebuffer null argument not rejected\n");
        return 1;
    }
    return 0;
}

static vn_u32 content_crc32_byte(vn_u32 crc, vn_u8 value) {
    vn_u32 i;

    crc ^= (vn_u32)value;
    for (i = 0u; i < 8u; ++i) {
        if ((crc & 1u) != 0u) {
            crc = (crc >> 1) ^ 0xEDB88320u;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}

static int run_content_golden(const char* requested_backend,
                              char* out_backend,
                              vn_u32 out_backend_size,
                              vn_u32* out_crc) {
    VNRunConfig cfg;
    VNRunResult result;
    VNRuntimeSession* session;
    VNRuntimeFrameView frame;
    vn_u32 crc;
    vn_u32 x;
    vn_u32 y;
    vn_u32 i;
    int rc;

    if (requested_backend == (const char*)0 || out_backend == (char*)0 ||
        out_backend_size == 0u || out_crc == (vn_u32*)0) {
        return 1;
    }
    vn_run_config_init(&cfg);
    cfg.pack_path = "assets/demo/content-demo.vnpak";
    cfg.backend_name = requested_backend;
    cfg.width = 64u;
    cfg.height = 48u;
    cfg.frames = 8u;
    cfg.dt_ms = 40u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;
    cfg.perf_flags = 0u;
    session = (VNRuntimeSession*)0;
    (void)memset(&result, 0, sizeof(result));
    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "content session create failed backend=%s rc=%d\n", requested_backend, rc);
        return 1;
    }
    for (i = 0u; i < cfg.frames; ++i) {
        rc = vn_runtime_session_step(session, &result);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "content session step failed backend=%s rc=%d\n", requested_backend, rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
    }
    vn_runtime_frame_view_init(&frame);
    rc = vn_runtime_session_get_frame_view(session, &frame);
    if (rc != VN_OK || frame.pixels == (const vn_u32*)0 ||
        frame.width != 64u || frame.height != 48u || frame.stride_pixels < 64u ||
        result.backend_name == (const char*)0) {
        (void)fprintf(stderr, "content frame view failed backend=%s rc=%d\n", requested_backend, rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    crc = 0xFFFFFFFFu;
    for (y = 0u; y < (vn_u32)frame.height; ++y) {
        for (x = 0u; x < (vn_u32)frame.width; ++x) {
            vn_u32 pixel;
            pixel = frame.pixels[y * frame.stride_pixels + x];
            crc = content_crc32_byte(crc, (vn_u8)((pixel >> 16) & 0xFFu));
            crc = content_crc32_byte(crc, (vn_u8)((pixel >> 8) & 0xFFu));
            crc = content_crc32_byte(crc, (vn_u8)(pixel & 0xFFu));
        }
    }
    (void)strncpy(out_backend, result.backend_name, (size_t)out_backend_size - 1u);
    out_backend[out_backend_size - 1u] = '\0';
    *out_crc = crc ^ 0xFFFFFFFFu;
    (void)vn_runtime_session_destroy(session);
    return 0;
}

static int test_content_pack_golden(void) {
    static const char* requested[] = {"scalar", "avx2", "neon", "rvv"};
    static const vn_u32 expected_crc = 0x995FF007u;
    VNRunConfig cfg;
    VNRunResult result;
    char actual[16];
    vn_u32 crc;
    vn_u32 i;
    int compared;

    compared = 0;
    for (i = 0u; i < (vn_u32)(sizeof(requested) / sizeof(requested[0])); ++i) {
        if (run_content_golden(requested[i], actual, (vn_u32)sizeof(actual), &crc) != 0) {
            return 1;
        }
        if (crc != expected_crc) {
            (void)fprintf(stderr,
                          "content golden mismatch requested=%s actual=%s expected=0x%08X got=0x%08X\n",
                          requested[i],
                          actual,
                          (unsigned int)expected_crc,
                          (unsigned int)crc);
            return 1;
        }
        if (strcmp(actual, requested[i]) == 0) {
            compared += 1;
        } else if (strcmp(actual, "scalar") != 0) {
            (void)fprintf(stderr, "content backend fallback mismatch requested=%s actual=%s\n", requested[i], actual);
            return 1;
        }
    }
    vn_run_config_init(&cfg);
    cfg.pack_path = "assets/demo/content-demo.vnpak";
    cfg.scene_name = "Gallery";
    cfg.backend_name = "scalar";
    cfg.width = 64u;
    cfg.height = 48u;
    cfg.frames = 64u;
    cfg.dt_ms = 40u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;
    cfg.hold_on_end = 1u;
    (void)memset(&result, 0, sizeof(result));
    if (vn_runtime_run(&cfg, &result) != VN_OK || result.frame_reuse_hits < 32u) {
        (void)fprintf(stderr,
                      "stable content fade did not reuse frames hits=%u misses=%u\n",
                      (unsigned int)result.frame_reuse_hits,
                      (unsigned int)result.frame_reuse_misses);
        return 1;
    }
    (void)printf("test_resource_texture_backend content_crc=0x%08X compared=%d\n",
                 (unsigned int)expected_crc,
                 compared);
    return 0;
}

static int test_runtime_texture_cache(void) {
    VNPak pak;
    VNRuntimeTextureStore store;
    VNRenderOp ops[2];
    vn_u32 saved_crc;
    vn_u32 saved_size;
    int rc;

    rc = vnpak_open(&pak, "assets/demo/content-demo.vnpak");
    if (rc != VN_OK) {
        (void)fprintf(stderr, "texture cache pack open failed rc=%d\n", rc);
        return 1;
    }
    rc = runtime_texture_store_init(&store, &pak);
    if (rc != VN_OK) {
        vnpak_close(&pak);
        return 1;
    }
    store.byte_limit = 1280u;
    fill_resource_op(&ops[0], VN_OP_SPRITE, 2u, 0, 0, 1u, 1u, 255u);
    fill_resource_op(&ops[1], VN_OP_SPRITE, 3u, 0, 0, 1u, 1u, 255u);
    if (runtime_texture_store_prepare_ops(&store, ops, 2u) != VN_OK ||
        store.entries[2].loaded == 0u || store.entries[3].loaded == 0u ||
        store.entries[2].pinned == 0u || store.entries[3].pinned == 0u ||
        store.bytes_used != 1280u) {
        (void)fprintf(stderr, "texture cache frame working set did not stay pinned\n");
        runtime_texture_store_destroy(&store);
        vnpak_close(&pak);
        return 1;
    }
    runtime_texture_store_destroy(&store);

    rc = runtime_texture_store_init(&store, &pak);
    if (rc != VN_OK) {
        vnpak_close(&pak);
        return 1;
    }
    store.byte_limit = 1280u;
    fill_resource_op(&ops[0], VN_OP_SPRITE, 2u, 0, 0, 1u, 1u, 255u);
    if (runtime_texture_store_prepare_ops(&store, ops, 1u) != VN_OK) {
        runtime_texture_store_destroy(&store);
        vnpak_close(&pak);
        return 1;
    }
    fill_resource_op(&ops[0], VN_OP_SPRITE, 4u, 0, 0, 1u, 1u, 255u);
    if (runtime_texture_store_prepare_ops(&store, ops, 1u) != VN_OK) {
        runtime_texture_store_destroy(&store);
        vnpak_close(&pak);
        return 1;
    }
    fill_resource_op(&ops[0], VN_OP_SPRITE, 2u, 0, 0, 1u, 1u, 255u);
    if (runtime_texture_store_prepare_ops(&store, ops, 1u) != VN_OK) {
        runtime_texture_store_destroy(&store);
        vnpak_close(&pak);
        return 1;
    }
    fill_resource_op(&ops[0], VN_OP_SPRITE, 3u, 0, 0, 1u, 1u, 255u);
    if (runtime_texture_store_prepare_ops(&store, ops, 1u) != VN_OK ||
        store.entries[2].loaded == 0u || store.entries[3].loaded == 0u ||
        store.entries[4].loaded != 0u || store.bytes_used != 1280u) {
        (void)fprintf(stderr, "texture cache LRU order mismatch bytes=%u\n", (unsigned int)store.bytes_used);
        runtime_texture_store_destroy(&store);
        vnpak_close(&pak);
        return 1;
    }
    runtime_texture_store_destroy(&store);

    rc = runtime_texture_store_init(&store, &pak);
    if (rc != VN_OK) {
        vnpak_close(&pak);
        return 1;
    }
    store.byte_limit = 1024u;
    fill_resource_op(&ops[0], VN_OP_SPRITE, 2u, 0, 0, 1u, 1u, 255u);
    fill_resource_op(&ops[1], VN_OP_SPRITE, 3u, 0, 0, 1u, 1u, 255u);
    if (runtime_texture_store_prepare_ops(&store, ops, 2u) != VN_E_NOMEM ||
        store.entries[2].loaded != 0u || store.entries[2].pinned != 0u ||
        store.bytes_used != 0u) {
        (void)fprintf(stderr, "texture cache pinned working set mismatch\n");
        runtime_texture_store_destroy(&store);
        vnpak_close(&pak);
        return 1;
    }
    fill_resource_op(&ops[0], VN_OP_SPRITE, 4u, 0, 0, 1u, 1u, 255u);
    if (runtime_texture_store_prepare_ops(&store, ops, 1u) != VN_OK ||
        store.entries[2].pinned != 0u || store.entries[4].loaded == 0u) {
        (void)fprintf(stderr, "texture cache prepare reset mismatch\n");
        runtime_texture_store_destroy(&store);
        vnpak_close(&pak);
        return 1;
    }
    runtime_texture_store_destroy(&store);

    saved_crc = pak.entries[2].crc32;
    pak.entries[2].crc32 ^= 1u;
    rc = runtime_texture_store_init(&store, &pak);
    if (rc != VN_OK) {
        pak.entries[2].crc32 = saved_crc;
        vnpak_close(&pak);
        return 1;
    }
    fill_resource_op(&ops[0], VN_OP_SPRITE, 2u, 0, 0, 1u, 1u, 255u);
    rc = runtime_texture_store_prepare_ops(&store, ops, 1u);
    runtime_texture_store_destroy(&store);
    pak.entries[2].crc32 = saved_crc;
    if (rc != VN_E_FORMAT) {
        (void)fprintf(stderr, "texture cache bad CRC mismatch rc=%d\n", rc);
        vnpak_close(&pak);
        return 1;
    }

    saved_size = pak.entries[2].data_size;
    pak.entries[2].data_size -= 1u;
    rc = runtime_texture_store_init(&store, &pak);
    if (rc != VN_OK) {
        pak.entries[2].data_size = saved_size;
        vnpak_close(&pak);
        return 1;
    }
    rc = runtime_texture_store_prepare_ops(&store, ops, 1u);
    runtime_texture_store_destroy(&store);
    pak.entries[2].data_size = saved_size;
    vnpak_close(&pak);
    if (rc != VN_E_FORMAT) {
        (void)fprintf(stderr, "texture cache bad payload mismatch rc=%d\n", rc);
        return 1;
    }
    return 0;
}

int main(void) {
    TestTextureSource source;
    vn_u32 scratch[4];
    VNRenderOp op;
    int rc;

    test_source_init(&source);
    if (test_framebuffer_state() != 0 || test_reference_formats(&source) != 0) {
        vn_texture_source_unbind();
        return 1;
    }
    if (test_backend_parity() != 0 || test_resource_crossfade() != 0 ||
        test_dirty_clip() != 0) {
        vn_texture_source_unbind();
        return 1;
    }

    vn_texture_source_unbind();
    fill_pixels(scratch, 4u, 0xFF000000u);
    fill_resource_op(&op, VN_OP_SPRITE, 1u, 0, 0, 1u, 1u, 255u);
    rc = vn_pp_draw_resource_texture(scratch, 2u, 2u, &op, (const VNRenderRect*)0);
    if (rc != VN_E_RENDER_STATE) {
        (void)fprintf(stderr, "unbound source expected render state rc=%d\n", rc);
        return 1;
    }
    if (vn_texture_source_bind((VNTextureLookupFn)0, &source) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "null lookup not rejected\n");
        return 1;
    }
    if (test_runtime_texture_cache() != 0 || test_content_pack_golden() != 0) {
        return 1;
    }

    (void)printf("test_resource_texture_backend ok\n");
    return 0;
}
