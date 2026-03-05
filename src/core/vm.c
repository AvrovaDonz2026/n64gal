#include "vn_vm.h"

void vm_step(VNState* s, vn_u32 delta_ms) {
    (void)delta_ms;
    if (s == (VNState*)0) {
        return;
    }
    if (s->script_pc != (const vn_u8*)0) {
        s->script_pc += 1;
    }
}

int vm_is_waiting(const VNState* s) {
    if (s == (const VNState*)0) {
        return VN_FALSE;
    }
    return ((s->flags & VN_VM_FLAG_WAITING) != 0u) ? VN_TRUE : VN_FALSE;
}
