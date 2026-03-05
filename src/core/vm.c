#include "vn_vm.h"

#define VN_VM_STEP_GUARD 128u

static vn_u16 vm_read_u16_le(const vn_u8* p) {
    return (vn_u16)((vn_u16)p[0] | ((vn_u16)p[1] << 8));
}

static vn_u32 vm_pc_off(const VNState* s) {
    return (vn_u32)(s->script_pc - s->script_base);
}

static int vm_need(const VNState* s, vn_u32 bytes) {
    vn_u32 need;
    need = vm_pc_off(s) + bytes;
    return (need <= s->script_size) ? VN_TRUE : VN_FALSE;
}

static void vm_fail(VNState* s) {
    s->flags |= VN_VM_FLAG_ERROR;
    s->flags |= VN_VM_FLAG_ENDED;
    s->flags &= ~VN_VM_FLAG_WAITING;
}

int vm_init(VNState* s, const vn_u8* script, vn_u32 script_size) {
    vn_u32 i;

    if (s == (VNState*)0 || script == (const vn_u8*)0 || script_size == 0u) {
        return VN_FALSE;
    }

    s->script_base = script;
    s->script_pc = script;
    s->script_size = script_size;
    s->wait_ms = 0u;
    for (i = 0u; i < 16u; ++i) {
        s->call_stack[i] = 0u;
    }
    s->call_sp = 0u;
    s->current_text_id = 0u;
    s->current_text_speed_ms = 0u;
    s->fade_layer_mask = 0u;
    s->fade_target_alpha = 0u;
    s->fade_duration_ms = 0u;
    s->current_bgm_id = 0u;
    s->current_bgm_loop = 0u;
    s->pending_se_id = 0u;
    s->pending_se_flag = 0u;
    s->last_choice_count = 0u;
    s->last_choice_selected_index = 0u;
    s->external_choice_valid = 0u;
    s->external_choice_index = 0u;
    s->last_choice_text_id = 0u;
    s->choice_serial = 0u;
    s->flags = 0u;
    return VN_TRUE;
}

void vm_set_choice_index(VNState* s, vn_u8 choice_index) {
    if (s == (VNState*)0) {
        return;
    }
    s->external_choice_valid = 1u;
    s->external_choice_index = choice_index;
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
        vn_u8 op;

        guard += 1u;
        if (vm_pc_off(s) >= s->script_size) {
            vm_fail(s);
            return;
        }

        op = *s->script_pc;
        s->script_pc += 1;

        if (op == VN_VM_OP_TEXT) {
            if (vm_need(s, 4u) == VN_FALSE) {
                vm_fail(s);
                return;
            }
            s->current_text_id = vm_read_u16_le(s->script_pc);
            s->script_pc += 2;
            s->current_text_speed_ms = vm_read_u16_le(s->script_pc);
            s->script_pc += 2;
            continue;
        }

        if (op == VN_VM_OP_WAIT) {
            if (vm_need(s, 2u) == VN_FALSE) {
                vm_fail(s);
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

        if (op == VN_VM_OP_CHOICE) {
            vn_u8 count;
            vn_u8 i;
            vn_u8 selected_index;
            vn_u16 selected_target;
            vn_u16 selected_text;

            if (vm_need(s, 1u) == VN_FALSE) {
                vm_fail(s);
                return;
            }
            count = *s->script_pc;
            s->script_pc += 1;
            if (count == 0u) {
                vm_fail(s);
                return;
            }
            if (vm_need(s, (vn_u32)count * 4u) == VN_FALSE) {
                vm_fail(s);
                return;
            }

            selected_target = 0u;
            selected_text = 0u;
            selected_index = 0u;
            if (s->external_choice_valid != 0u) {
                if (s->external_choice_index < count) {
                    selected_index = s->external_choice_index;
                }
                s->external_choice_valid = 0u;
            }
            for (i = 0u; i < count; ++i) {
                vn_u16 text_id;
                vn_u16 target;
                text_id = vm_read_u16_le(s->script_pc);
                s->script_pc += 2;
                target = vm_read_u16_le(s->script_pc);
                s->script_pc += 2;
                if ((vn_u32)target >= s->script_size) {
                    vm_fail(s);
                    return;
                }
                if (i == selected_index) {
                    selected_target = target;
                    selected_text = text_id;
                }
            }

            s->last_choice_count = count;
            s->last_choice_text_id = selected_text;
            s->last_choice_selected_index = selected_index;
            s->choice_serial += 1u;
            s->script_pc = s->script_base + selected_target;
            continue;
        }

        if (op == VN_VM_OP_GOTO) {
            vn_u16 target;
            if (vm_need(s, 2u) == VN_FALSE) {
                vm_fail(s);
                return;
            }
            target = vm_read_u16_le(s->script_pc);
            s->script_pc += 2;
            if ((vn_u32)target >= s->script_size) {
                vm_fail(s);
                return;
            }
            s->script_pc = s->script_base + target;
            continue;
        }

        if (op == VN_VM_OP_CALL) {
            vn_u16 target;
            vn_u16 ret_off;
            if (vm_need(s, 2u) == VN_FALSE) {
                vm_fail(s);
                return;
            }
            if (s->call_sp >= 16u) {
                vm_fail(s);
                return;
            }
            target = vm_read_u16_le(s->script_pc);
            s->script_pc += 2;
            if ((vn_u32)target >= s->script_size) {
                vm_fail(s);
                return;
            }
            ret_off = (vn_u16)vm_pc_off(s);
            s->call_stack[s->call_sp] = ret_off;
            s->call_sp += 1;
            s->script_pc = s->script_base + target;
            continue;
        }

        if (op == VN_VM_OP_RETURN) {
            vn_u16 ret;
            if (s->call_sp == 0u) {
                vm_fail(s);
                return;
            }
            s->call_sp -= 1;
            ret = s->call_stack[s->call_sp];
            if ((vn_u32)ret >= s->script_size) {
                vm_fail(s);
                return;
            }
            s->script_pc = s->script_base + ret;
            continue;
        }

        if (op == VN_VM_OP_FADE) {
            if (vm_need(s, 4u) == VN_FALSE) {
                vm_fail(s);
                return;
            }
            s->fade_layer_mask = s->script_pc[0];
            s->fade_target_alpha = s->script_pc[1];
            s->fade_duration_ms = vm_read_u16_le(s->script_pc + 2);
            s->script_pc += 4;
            continue;
        }

        if (op == VN_VM_OP_BGM) {
            if (vm_need(s, 3u) == VN_FALSE) {
                vm_fail(s);
                return;
            }
            s->current_bgm_id = vm_read_u16_le(s->script_pc);
            s->script_pc += 2;
            s->current_bgm_loop = s->script_pc[0];
            s->script_pc += 1;
            continue;
        }

        if (op == VN_VM_OP_SE) {
            if (vm_need(s, 2u) == VN_FALSE) {
                vm_fail(s);
                return;
            }
            s->pending_se_id = vm_read_u16_le(s->script_pc);
            s->pending_se_flag = 1u;
            s->script_pc += 2;
            continue;
        }

        if (op == VN_VM_OP_END) {
            s->flags |= VN_VM_FLAG_ENDED;
            s->flags &= ~VN_VM_FLAG_WAITING;
            return;
        }

        vm_fail(s);
        return;
    }

    vm_fail(s);
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

int vm_has_error(const VNState* s) {
    if (s == (const VNState*)0) {
        return VN_FALSE;
    }
    return ((s->flags & VN_VM_FLAG_ERROR) != 0u) ? VN_TRUE : VN_FALSE;
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

vn_u8 vm_fade_layer_mask(const VNState* s) {
    if (s == (const VNState*)0) {
        return 0u;
    }
    return s->fade_layer_mask;
}

vn_u8 vm_fade_target_alpha(const VNState* s) {
    if (s == (const VNState*)0) {
        return 0u;
    }
    return s->fade_target_alpha;
}

vn_u16 vm_fade_duration_ms(const VNState* s) {
    if (s == (const VNState*)0) {
        return 0u;
    }
    return s->fade_duration_ms;
}

vn_u16 vm_current_bgm_id(const VNState* s) {
    if (s == (const VNState*)0) {
        return 0u;
    }
    return s->current_bgm_id;
}

vn_u8 vm_current_bgm_loop(const VNState* s) {
    if (s == (const VNState*)0) {
        return 0u;
    }
    return s->current_bgm_loop;
}

vn_u16 vm_take_se_id(VNState* s) {
    vn_u16 value;
    if (s == (VNState*)0 || s->pending_se_flag == 0u) {
        return 0u;
    }
    value = s->pending_se_id;
    s->pending_se_id = 0u;
    s->pending_se_flag = 0u;
    return value;
}

vn_u8 vm_last_choice_count(const VNState* s) {
    if (s == (const VNState*)0) {
        return 0u;
    }
    return s->last_choice_count;
}

vn_u16 vm_last_choice_text_id(const VNState* s) {
    if (s == (const VNState*)0) {
        return 0u;
    }
    return s->last_choice_text_id;
}

vn_u8 vm_last_choice_selected_index(const VNState* s) {
    if (s == (const VNState*)0) {
        return 0u;
    }
    return s->last_choice_selected_index;
}

vn_u32 vm_choice_serial(const VNState* s) {
    if (s == (const VNState*)0) {
        return 0u;
    }
    return s->choice_serial;
}
