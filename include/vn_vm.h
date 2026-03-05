#ifndef VN_VM_H
#define VN_VM_H

#include "vn_types.h"

typedef struct {
    const vn_u8* script_pc;
    vn_u32 flags;
} VNState;

#define VN_VM_FLAG_WAITING (1u << 0)

void vm_step(VNState* s, vn_u32 delta_ms);
int vm_is_waiting(const VNState* s);

#endif
