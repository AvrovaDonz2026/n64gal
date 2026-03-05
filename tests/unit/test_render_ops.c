#include <stdio.h>

#include "vn_frontend.h"
#include "vn_error.h"

static int expect_scene(vn_u32 scene_id, vn_u32 expected_count, vn_u8 expected_last_op) {
    VNRuntimeState state;
    VNRenderOp ops[8];
    vn_u32 count;
    int rc;

    state.frame_index = 17u;
    state.clear_color = 220u;
    state.scene_id = scene_id;
    state.resource_count = 2u;
    state.text_id = 300u;
    state.text_speed_ms = 20u;
    state.vm_waiting = 0u;
    state.vm_ended = 0u;
    state.vm_error = 0u;
    state.vm_fade_active = 0u;
    state.fade_layer_mask = 0u;
    state.fade_alpha = 0u;
    state.fade_duration_ms = 0u;
    state.bgm_id = 0u;
    state.bgm_loop = 0u;
    state.se_id = 0u;
    state.choice_count = 0u;
    state.choice_text_id = 0u;
    state.choice_selected_index = 0u;

    count = 8u;
    rc = build_render_ops(&state, ops, &count);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "build_render_ops failed scene=%u rc=%d\n", (unsigned int)scene_id, rc);
        return 1;
    }
    if (count != expected_count) {
        (void)fprintf(stderr, "op count mismatch scene=%u got=%u expected=%u\n",
                      (unsigned int)scene_id,
                      (unsigned int)count,
                      (unsigned int)expected_count);
        return 1;
    }
    if (ops[0].op != VN_OP_CLEAR || ops[1].op != VN_OP_SPRITE || ops[2].op != VN_OP_TEXT) {
        (void)fprintf(stderr, "base op sequence mismatch scene=%u\n", (unsigned int)scene_id);
        return 1;
    }
    if (ops[expected_count - 1u].op != expected_last_op) {
        (void)fprintf(stderr, "last op mismatch scene=%u got=%u expected=%u\n",
                      (unsigned int)scene_id,
                      (unsigned int)ops[expected_count - 1u].op,
                      (unsigned int)expected_last_op);
        return 1;
    }
    return 0;
}

int main(void) {
    VNRuntimeState state;
    VNRenderOp ops[8];
    vn_u32 count;
    int rc;

    if (expect_scene(VN_SCENE_S0, 3u, VN_OP_TEXT) != 0) {
        return 1;
    }
    if (expect_scene(VN_SCENE_S1, 4u, VN_OP_FADE) != 0) {
        return 1;
    }
    if (expect_scene(VN_SCENE_S3, 4u, VN_OP_FADE) != 0) {
        return 1;
    }

    state.frame_index = 9u;
    state.clear_color = 0u;
    state.scene_id = VN_SCENE_S0;
    state.resource_count = 0u;
    state.text_id = 111u;
    state.text_speed_ms = 18u;
    state.vm_waiting = 1u;
    state.vm_ended = 0u;
    state.vm_error = 0u;
    state.vm_fade_active = 0u;
    state.fade_layer_mask = 0u;
    state.fade_alpha = 0u;
    state.fade_duration_ms = 0u;
    state.bgm_id = 0u;
    state.bgm_loop = 0u;
    state.se_id = 0u;
    state.choice_count = 0u;
    state.choice_text_id = 0u;
    state.choice_selected_index = 0u;
    count = 8u;
    rc = build_render_ops(&state, ops, &count);
    if (rc != VN_OK || count != 4u || ops[3].op != VN_OP_FADE || ops[3].flags != 1u) {
        (void)fprintf(stderr, "vm waiting should force fade op\n");
        return 1;
    }

    state.frame_index = 0u;
    state.clear_color = 0u;
    state.scene_id = VN_SCENE_S0;
    state.resource_count = 0u;
    state.text_id = 111u;
    state.text_speed_ms = 18u;
    state.vm_waiting = 0u;
    state.vm_ended = 0u;
    state.vm_error = 0u;
    state.vm_fade_active = 1u;
    state.fade_layer_mask = 3u;
    state.fade_alpha = 200u;
    state.fade_duration_ms = 90u;
    state.bgm_id = 0u;
    state.bgm_loop = 0u;
    state.se_id = 55u;
    state.choice_count = 2u;
    state.choice_text_id = 500u;
    state.choice_selected_index = 1u;
    count = 8u;
    rc = build_render_ops(&state, ops, &count);
    if (rc != VN_OK || count != 4u) {
        (void)fprintf(stderr, "vm fade active should produce 4 ops\n");
        return 1;
    }
    if (ops[1].flags != 1u || ops[2].flags != 11u || ops[3].alpha != 200u || ops[3].flags != 2u || ops[3].tex_id != 3u) {
        (void)fprintf(stderr, "vm fade/bgm/choice encoded flags mismatch\n");
        return 1;
    }

    state.frame_index = 0u;
    state.clear_color = 0u;
    state.scene_id = VN_SCENE_S3;
    state.resource_count = 0u;
    state.text_id = 0u;
    state.text_speed_ms = 0u;
    state.vm_waiting = 0u;
    state.vm_ended = 0u;
    state.vm_error = 0u;
    state.vm_fade_active = 0u;
    state.fade_layer_mask = 0u;
    state.fade_alpha = 0u;
    state.fade_duration_ms = 0u;
    state.bgm_id = 0u;
    state.bgm_loop = 0u;
    state.se_id = 0u;
    state.choice_count = 0u;
    state.choice_text_id = 0u;
    state.choice_selected_index = 0u;
    count = 3u;
    rc = build_render_ops(&state, ops, &count);
    if (rc != VN_E_NOMEM) {
        (void)fprintf(stderr, "expected VN_E_NOMEM, got rc=%d\n", rc);
        return 1;
    }
    if (count != 4u) {
        (void)fprintf(stderr, "expected required count=4 got=%u\n", (unsigned int)count);
        return 1;
    }

    (void)printf("test_render_ops ok\n");
    return 0;
}
