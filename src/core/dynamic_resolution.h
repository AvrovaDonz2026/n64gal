#ifndef VN_DYNAMIC_RESOLUTION_H
#define VN_DYNAMIC_RESOLUTION_H

#include "vn_types.h"

#define VN_DYNRES_MAX_TIERS 3u
#define VN_DYNRES_DOWN_WINDOW 120u
#define VN_DYNRES_UP_WINDOW 300u
#define VN_DYNRES_DOWN_P95_MS 16.67
#define VN_DYNRES_UP_P95_MS 13.00

typedef struct {
    vn_u16 width;
    vn_u16 height;
} VNDynResTier;

typedef struct {
    VNDynResTier tiers[VN_DYNRES_MAX_TIERS];
    double frame_ms_history[VN_DYNRES_UP_WINDOW];
    vn_u32 tier_count;
    vn_u32 current_tier;
    vn_u32 switch_count;
    vn_u32 history_count;
    vn_u32 history_cursor;
} VNDynResState;

void vn_dynres_init(VNDynResState* state, vn_u16 base_width, vn_u16 base_height);
void vn_dynres_reset_history(VNDynResState* state);
vn_u32 vn_dynres_get_tier_count(const VNDynResState* state);
vn_u32 vn_dynres_get_current_tier(const VNDynResState* state);
vn_u32 vn_dynres_get_switch_count(const VNDynResState* state);
const VNDynResTier* vn_dynres_get_tier(const VNDynResState* state, vn_u32 tier_index);
const VNDynResTier* vn_dynres_get_current_dims(const VNDynResState* state);
const char* vn_dynres_tier_name(vn_u32 tier_index);
int vn_dynres_should_switch(VNDynResState* state,
                            double frame_ms,
                            vn_u32* out_next_tier,
                            double* out_window_p95_ms);
int vn_dynres_apply_tier(VNDynResState* state, vn_u32 next_tier);

#endif
