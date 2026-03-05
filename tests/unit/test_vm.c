#include <stdio.h>

#include "vn_vm.h"

int main(void) {
    VNState vm;
    const vn_u8 script_complex[] = {
        VN_VM_OP_BGM, 0x34, 0x12, 0x01,
        VN_VM_OP_CALL, 0x0B, 0x00,
        VN_VM_OP_WAIT, 0x20, 0x00,
        VN_VM_OP_END,
        VN_VM_OP_CHOICE, 0x02,
        0x01, 0x02, 0x15, 0x00,
        0x02, 0x02, 0x1E, 0x00,
        VN_VM_OP_FADE, 0x03, 0x88, 0x10, 0x00,
        VN_VM_OP_SE, 0x44, 0x33,
        VN_VM_OP_RETURN,
        VN_VM_OP_TEXT, 0xAA, 0x00, 0x05, 0x00,
        VN_VM_OP_RETURN
    };
    const vn_u8 script_end[] = {
        VN_VM_OP_TEXT, 0x99, 0x00, 0x01, 0x00,
        VN_VM_OP_END
    };
    const vn_u8 script_bad_return[] = {
        VN_VM_OP_RETURN
    };
    int ok;
    vn_u16 se_id;

    ok = vm_init(&vm, script_complex, (vn_u32)sizeof(script_complex));
    if (ok != VN_TRUE) {
        (void)fprintf(stderr, "vm_init failed for complex script\n");
        return 1;
    }

    vm_step(&vm, 16u);

    if (vm_current_bgm_id(&vm) != 0x1234u || vm_current_bgm_loop(&vm) != 1u) {
        (void)fprintf(stderr, "bgm state mismatch\n");
        return 1;
    }
    if (vm_last_choice_count(&vm) != 2u || vm_last_choice_text_id(&vm) != 0x0201u) {
        (void)fprintf(stderr, "choice state mismatch\n");
        return 1;
    }
    if (vm_fade_layer_mask(&vm) != 3u || vm_fade_target_alpha(&vm) != 0x88u || vm_fade_duration_ms(&vm) != 0x0010u) {
        (void)fprintf(stderr, "fade state mismatch\n");
        return 1;
    }

    se_id = vm_take_se_id(&vm);
    if (se_id != 0x3344u) {
        (void)fprintf(stderr, "se id mismatch got=%u\n", (unsigned int)se_id);
        return 1;
    }
    if (vm_take_se_id(&vm) != 0u) {
        (void)fprintf(stderr, "se should be single-shot\n");
        return 1;
    }

    if (vm_is_waiting(&vm) != VN_TRUE) {
        (void)fprintf(stderr, "vm should wait after first step\n");
        return 1;
    }
    if (vm_is_ended(&vm) != VN_FALSE || vm_has_error(&vm) != VN_FALSE) {
        (void)fprintf(stderr, "vm state invalid after first step\n");
        return 1;
    }

    vm_step(&vm, 16u);
    if (vm_is_ended(&vm) != VN_TRUE || vm_has_error(&vm) != VN_FALSE) {
        (void)fprintf(stderr, "vm should end cleanly after second step\n");
        return 1;
    }

    ok = vm_init(&vm, script_end, (vn_u32)sizeof(script_end));
    if (ok != VN_TRUE) {
        (void)fprintf(stderr, "vm_init failed for end script\n");
        return 1;
    }
    vm_step(&vm, 0u);
    if (vm_current_text_id(&vm) != 0x0099u || vm_is_ended(&vm) != VN_TRUE) {
        (void)fprintf(stderr, "end script mismatch\n");
        return 1;
    }

    ok = vm_init(&vm, script_bad_return, (vn_u32)sizeof(script_bad_return));
    if (ok != VN_TRUE) {
        (void)fprintf(stderr, "vm_init failed for bad return script\n");
        return 1;
    }
    vm_step(&vm, 0u);
    if (vm_has_error(&vm) != VN_TRUE || vm_is_ended(&vm) != VN_TRUE) {
        (void)fprintf(stderr, "bad return should set error+ended\n");
        return 1;
    }

    (void)printf("test_vm ok\n");
    return 0;
}
