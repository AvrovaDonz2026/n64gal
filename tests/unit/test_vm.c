#include <stdio.h>

#include "vn_vm.h"

int main(void) {
    VNState vm;
    const vn_u8 script_loop[] = {
        VN_VM_OP_TEXT, 0x34, 0x12, 0x10, 0x00,
        VN_VM_OP_WAIT, 0x20, 0x00,
        VN_VM_OP_GOTO, 0x00, 0x00
    };
    const vn_u8 script_end[] = {
        VN_VM_OP_TEXT, 0x99, 0x00, 0x01, 0x00,
        VN_VM_OP_END
    };
    int ok;

    ok = vm_init(&vm, script_loop, (vn_u32)sizeof(script_loop));
    if (ok != VN_TRUE) {
        (void)fprintf(stderr, "vm_init failed for loop script\n");
        return 1;
    }

    vm_step(&vm, 16u);
    if (vm_current_text_id(&vm) != 0x1234u) {
        (void)fprintf(stderr, "text id mismatch got=%u\n", (unsigned int)vm_current_text_id(&vm));
        return 1;
    }
    if (vm_current_text_speed_ms(&vm) != 16u) {
        (void)fprintf(stderr, "text speed mismatch\n");
        return 1;
    }
    if (vm_is_waiting(&vm) != VN_TRUE) {
        (void)fprintf(stderr, "vm should be waiting after first step\n");
        return 1;
    }

    vm_step(&vm, 16u);
    if (vm_is_waiting(&vm) != VN_TRUE) {
        (void)fprintf(stderr, "vm should re-enter wait after loop jump\n");
        return 1;
    }
    if (vm_is_ended(&vm) != VN_FALSE) {
        (void)fprintf(stderr, "loop script should not end\n");
        return 1;
    }

    ok = vm_init(&vm, script_end, (vn_u32)sizeof(script_end));
    if (ok != VN_TRUE) {
        (void)fprintf(stderr, "vm_init failed for end script\n");
        return 1;
    }

    vm_step(&vm, 0u);
    if (vm_current_text_id(&vm) != 0x0099u) {
        (void)fprintf(stderr, "end script text id mismatch\n");
        return 1;
    }
    if (vm_is_ended(&vm) != VN_TRUE) {
        (void)fprintf(stderr, "vm should be ended\n");
        return 1;
    }

    (void)printf("test_vm ok\n");
    return 0;
}
