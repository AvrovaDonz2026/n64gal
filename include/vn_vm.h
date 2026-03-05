#ifndef VN_VM_H
#define VN_VM_H

#include "vn_types.h"

typedef struct {
    const vn_u8* script_base;
    const vn_u8* script_pc;
    vn_u32 script_size;
    vn_u32 wait_ms;
    vn_u16 current_text_id;
    vn_u16 current_text_speed_ms;
    vn_u32 flags;
} VNState;

#define VN_VM_FLAG_WAITING (1u << 0)
#define VN_VM_FLAG_ENDED   (1u << 1)
#define VN_VM_FLAG_ERROR   (1u << 2)

#define VN_VM_OP_TEXT 0x03u
#define VN_VM_OP_WAIT 0x04u
#define VN_VM_OP_GOTO 0x06u
#define VN_VM_OP_END  0xFFu

int vm_init(VNState* s, const vn_u8* script, vn_u32 script_size);
void vm_step(VNState* s, vn_u32 delta_ms);
int vm_is_waiting(const VNState* s);
int vm_is_ended(const VNState* s);
vn_u16 vm_current_text_id(const VNState* s);
vn_u16 vm_current_text_speed_ms(const VNState* s);

#endif
