#include "vn_vm.h"

#define VN_VM_STEP_GUARD 64u

static vn_u16 vm_read_u16_le(const vn_u8* p) {
    return (vn_u16)((vn_u16)p[0] | ((vn_u16)p[1] << 8));
}

int vm_init(VNState* s, const vn_u8* script, vn_u32 script_size) {
    if (s == (VNState*)0 || script == (const vn_u8*)0 || script_size == 0u) {
        return VN_FALSE;
    }

    s->script_base = script;
    s->script_pc = script;
    s->script_size = script_size;
    s->wait_ms = 0u;
    s->current_text_id = 0u;
    s->current_text_speed_ms = 0u;
    s->flags = 0u;
    return VN_TRUE;
}

void vm_step(VNState* s, vn_u32 delta_ms) {
    vn_u32 remaining;
    vn_u32 guard;

    if (s == (VNState*)0 || s->script_base == (const vn_u8*)0 || s->script_size == 0u) {
        return;
    }
    if ((s->flags & VN_VM_FLAG_ENDED) != 0u) {
        return;
    }

    remaining = delta_ms;
    if (s->wait_ms > 0u) {
        if (remaining < s->wait_ms) {
            s->wait_ms -= remaining;
            s->flags |= VN_VM_FLAG_WAITING;
            return;
        }
        remaining -= s->wait_ms;
        s->wait_ms = 0u;
        s->flags &= ~VN_VM_FLAG_WAITING;
    }

    guard = 0u;
    while (guard < VN_VM_STEP_GUARD) {
        vn_u32 pc_off;
        vn_u8 op;

        guard += 1u;
        pc_off = (vn_u32)(s->script_pc - s->script_base);
        if (pc_off >= s->script_size) {
            s->flags |= VN_VM_FLAG_ERROR;
            s->flags |= VN_VM_FLAG_ENDED;
            return;
        }

        op = *s->script_pc;
        s->script_pc += 1;

        if (op == VN_VM_OP_TEXT) {
            vn_u32 need_off;
            need_off = (vn_u32)(s->script_pc - s->script_base) + 4u;
            if (need_off > s->script_size) {
                s->flags |= VN_VM_FLAG_ERROR;
                s->flags |= VN_VM_FLAG_ENDED;
                return;
            }
            s->current_text_id = vm_read_u16_le(s->script_pc);
            s->script_pc += 2;
            s->current_text_speed_ms = vm_read_u16_le(s->script_pc);
            s->script_pc += 2;
            continue;
        }

        if (op == VN_VM_OP_WAIT) {
            vn_u32 need_off;
            need_off = (vn_u32)(s->script_pc - s->script_base) + 2u;
            if (need_off > s->script_size) {
                s->flags |= VN_VM_FLAG_ERROR;
                s->flags |= VN_VM_FLAG_ENDED;
                return;
            }
            s->wait_ms = (vn_u32)vm_read_u16_le(s->script_pc);
            s->script_pc += 2;
            if (s->wait_ms > 0u) {
                if (remaining < s->wait_ms) {
                    s->wait_ms -= remaining;
                    s->flags |= VN_VM_FLAG_WAITING;
                    return;
                }
                remaining -= s->wait_ms;
                s->wait_ms = 0u;
                s->flags &= ~VN_VM_FLAG_WAITING;
            }
            continue;
        }

        if (op == VN_VM_OP_GOTO) {
            vn_u16 target;
            vn_u32 need_off;
            need_off = (vn_u32)(s->script_pc - s->script_base) + 2u;
            if (need_off > s->script_size) {
                s->flags |= VN_VM_FLAG_ERROR;
                s->flags |= VN_VM_FLAG_ENDED;
                return;
            }
            target = vm_read_u16_le(s->script_pc);
            s->script_pc += 2;
            if ((vn_u32)target >= s->script_size) {
                s->flags |= VN_VM_FLAG_ERROR;
                s->flags |= VN_VM_FLAG_ENDED;
                return;
            }
            s->script_pc = s->script_base + target;
            continue;
        }

        if (op == VN_VM_OP_END) {
            s->flags |= VN_VM_FLAG_ENDED;
            s->flags &= ~VN_VM_FLAG_WAITING;
            return;
        }

        s->flags |= VN_VM_FLAG_ERROR;
        s->flags |= VN_VM_FLAG_ENDED;
        return;
    }

    s->flags |= VN_VM_FLAG_ERROR;
    s->flags |= VN_VM_FLAG_ENDED;
}

int vm_is_waiting(const VNState* s) {
    if (s == (const VNState*)0) {
        return VN_FALSE;
    }
    return ((s->flags & VN_VM_FLAG_WAITING) != 0u) ? VN_TRUE : VN_FALSE;
}

int vm_is_ended(const VNState* s) {
    if (s == (const VNState*)0) {
        return VN_FALSE;
    }
    return ((s->flags & VN_VM_FLAG_ENDED) != 0u) ? VN_TRUE : VN_FALSE;
}

vn_u16 vm_current_text_id(const VNState* s) {
    if (s == (const VNState*)0) {
        return 0u;
    }
    return s->current_text_id;
}

vn_u16 vm_current_text_speed_ms(const VNState* s) {
    if (s == (const VNState*)0) {
        return 0u;
    }
    return s->current_text_speed_ms;
}
