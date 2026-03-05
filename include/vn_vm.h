#ifndef VN_VM_H
#define VN_VM_H

#include "vn_types.h"

typedef struct {
    const vn_u8* script_base;
    const vn_u8* script_pc;
    vn_u32 script_size;
    vn_u32 wait_ms;
    vn_u16 call_stack[16];
    vn_u8 call_sp;
    vn_u16 current_text_id;
    vn_u16 current_text_speed_ms;
    vn_u8 fade_layer_mask;
    vn_u8 fade_target_alpha;
    vn_u16 fade_duration_ms;
    vn_u16 current_bgm_id;
    vn_u8 current_bgm_loop;
    vn_u16 pending_se_id;
    vn_u8 pending_se_flag;
    vn_u8 last_choice_count;
    vn_u16 last_choice_text_id;
    vn_u32 flags;
} VNState;

#define VN_VM_FLAG_WAITING (1u << 0)
#define VN_VM_FLAG_ENDED   (1u << 1)
#define VN_VM_FLAG_ERROR   (1u << 2)

#define VN_VM_OP_TEXT 0x03u
#define VN_VM_OP_WAIT 0x04u
#define VN_VM_OP_CHOICE 0x05u
#define VN_VM_OP_GOTO 0x06u
#define VN_VM_OP_CALL 0x07u
#define VN_VM_OP_RETURN 0x08u
#define VN_VM_OP_FADE 0x09u
#define VN_VM_OP_BGM  0x0Au
#define VN_VM_OP_SE   0x0Bu
#define VN_VM_OP_END  0xFFu

int vm_init(VNState* s, const vn_u8* script, vn_u32 script_size);
void vm_step(VNState* s, vn_u32 delta_ms);
int vm_is_waiting(const VNState* s);
int vm_is_ended(const VNState* s);
int vm_has_error(const VNState* s);
vn_u16 vm_current_text_id(const VNState* s);
vn_u16 vm_current_text_speed_ms(const VNState* s);
vn_u8 vm_fade_layer_mask(const VNState* s);
vn_u8 vm_fade_target_alpha(const VNState* s);
vn_u16 vm_fade_duration_ms(const VNState* s);
vn_u16 vm_current_bgm_id(const VNState* s);
vn_u8 vm_current_bgm_loop(const VNState* s);
vn_u16 vm_take_se_id(VNState* s);
vn_u8 vm_last_choice_count(const VNState* s);
vn_u16 vm_last_choice_text_id(const VNState* s);

#endif
