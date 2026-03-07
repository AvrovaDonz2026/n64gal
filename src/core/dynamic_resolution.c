#include <string.h>

#include "dynamic_resolution.h"

static vn_u16 dynres_scale_dim(vn_u16 base, vn_u32 numer, vn_u32 denom) {
    vn_u32 scaled;

    if (base == 0u || denom == 0u) {
        return 1u;
    }
    scaled = (((vn_u32)base * numer) + (denom / 2u)) / denom;
    if (scaled == 0u) {
        scaled = 1u;
    }
    if (scaled > 65535u) {
        scaled = 65535u;
    }
    return (vn_u16)scaled;
}

static void dynres_push_history(VNDynResState* state, double frame_ms) {
    if (state == (VNDynResState*)0) {
        return;
    }
    if (frame_ms < 0.0) {
        frame_ms = 0.0;
    }
    state->frame_ms_history[state->history_cursor] = frame_ms;
    state->history_cursor = (state->history_cursor + 1u) % VN_DYNRES_UP_WINDOW;
    if (state->history_count < VN_DYNRES_UP_WINDOW) {
        state->history_count += 1u;
    }
}

static double dynres_window_p95(const VNDynResState* state, vn_u32 sample_count) {
    double samples[VN_DYNRES_UP_WINDOW];
    vn_u32 i;
    vn_u32 history_index;
    vn_u32 rank;

    if (state == (const VNDynResState*)0 || sample_count == 0u || sample_count > state->history_count) {
        return 0.0;
    }

    for (i = 0u; i < sample_count; ++i) {
        history_index = (state->history_cursor + VN_DYNRES_UP_WINDOW - sample_count + i) % VN_DYNRES_UP_WINDOW;
        samples[i] = state->frame_ms_history[history_index];
    }

    for (i = 1u; i < sample_count; ++i) {
        double value;
        vn_u32 pos;

        value = samples[i];
        pos = i;
        while (pos > 0u && samples[pos - 1u] > value) {
            samples[pos] = samples[pos - 1u];
            pos -= 1u;
        }
        samples[pos] = value;
    }

    rank = (95u * sample_count + 99u) / 100u;
    if (rank == 0u) {
        rank = 1u;
    }
    if (rank > sample_count) {
        rank = sample_count;
    }
    return samples[rank - 1u];
}

void vn_dynres_reset_history(VNDynResState* state) {
    if (state == (VNDynResState*)0) {
        return;
    }
    state->history_count = 0u;
    state->history_cursor = 0u;
}

void vn_dynres_init(VNDynResState* state, vn_u16 base_width, vn_u16 base_height) {
    VNDynResTier tier;

    if (state == (VNDynResState*)0) {
        return;
    }

    (void)memset(state, 0, sizeof(*state));
    if (base_width == 0u) {
        base_width = 1u;
    }
    if (base_height == 0u) {
        base_height = 1u;
    }

    state->tiers[0].width = base_width;
    state->tiers[0].height = base_height;
    state->tier_count = 1u;

    tier.width = dynres_scale_dim(base_width, 3u, 4u);
    tier.height = dynres_scale_dim(base_height, 3u, 4u);
    if (tier.width != state->tiers[state->tier_count - 1u].width ||
        tier.height != state->tiers[state->tier_count - 1u].height) {
        state->tiers[state->tier_count] = tier;
        state->tier_count += 1u;
    }

    tier.width = dynres_scale_dim(base_width, 1u, 2u);
    tier.height = dynres_scale_dim(base_height, 1u, 2u);
    if (state->tier_count < VN_DYNRES_MAX_TIERS &&
        (tier.width != state->tiers[state->tier_count - 1u].width ||
         tier.height != state->tiers[state->tier_count - 1u].height)) {
        state->tiers[state->tier_count] = tier;
        state->tier_count += 1u;
    }

    state->current_tier = 0u;
    state->switch_count = 0u;
    vn_dynres_reset_history(state);
}

vn_u32 vn_dynres_get_tier_count(const VNDynResState* state) {
    if (state == (const VNDynResState*)0) {
        return 0u;
    }
    return state->tier_count;
}

vn_u32 vn_dynres_get_current_tier(const VNDynResState* state) {
    if (state == (const VNDynResState*)0) {
        return 0u;
    }
    return state->current_tier;
}

vn_u32 vn_dynres_get_switch_count(const VNDynResState* state) {
    if (state == (const VNDynResState*)0) {
        return 0u;
    }
    return state->switch_count;
}

const VNDynResTier* vn_dynres_get_tier(const VNDynResState* state, vn_u32 tier_index) {
    if (state == (const VNDynResState*)0 || tier_index >= state->tier_count) {
        return (const VNDynResTier*)0;
    }
    return &state->tiers[tier_index];
}

const VNDynResTier* vn_dynres_get_current_dims(const VNDynResState* state) {
    if (state == (const VNDynResState*)0) {
        return (const VNDynResTier*)0;
    }
    return vn_dynres_get_tier(state, state->current_tier);
}

const char* vn_dynres_tier_name(vn_u32 tier_index) {
    if (tier_index == 1u) {
        return "R1";
    }
    if (tier_index == 2u) {
        return "R2";
    }
    return "R0";
}

int vn_dynres_should_switch(VNDynResState* state,
                            double frame_ms,
                            vn_u32* out_next_tier,
                            double* out_window_p95_ms) {
    double p95_ms;

    if (out_next_tier != (vn_u32*)0) {
        *out_next_tier = 0u;
    }
    if (out_window_p95_ms != (double*)0) {
        *out_window_p95_ms = 0.0;
    }
    if (state == (VNDynResState*)0 || state->tier_count == 0u) {
        return 0;
    }

    dynres_push_history(state, frame_ms);

    if ((state->current_tier + 1u) < state->tier_count &&
        state->history_count >= VN_DYNRES_DOWN_WINDOW) {
        p95_ms = dynres_window_p95(state, VN_DYNRES_DOWN_WINDOW);
        if (out_window_p95_ms != (double*)0) {
            *out_window_p95_ms = p95_ms;
        }
        if (p95_ms > VN_DYNRES_DOWN_P95_MS) {
            if (out_next_tier != (vn_u32*)0) {
                *out_next_tier = state->current_tier + 1u;
            }
            return 1;
        }
    }

    if (state->current_tier > 0u && state->history_count >= VN_DYNRES_UP_WINDOW) {
        p95_ms = dynres_window_p95(state, VN_DYNRES_UP_WINDOW);
        if (out_window_p95_ms != (double*)0) {
            *out_window_p95_ms = p95_ms;
        }
        if (p95_ms < VN_DYNRES_UP_P95_MS) {
            if (out_next_tier != (vn_u32*)0) {
                *out_next_tier = state->current_tier - 1u;
            }
            return 1;
        }
    }

    return 0;
}

int vn_dynres_apply_tier(VNDynResState* state, vn_u32 next_tier) {
    if (state == (VNDynResState*)0 || next_tier >= state->tier_count) {
        return 0;
    }
    if (next_tier == state->current_tier) {
        return 1;
    }
    state->current_tier = next_tier;
    state->switch_count += 1u;
    vn_dynres_reset_history(state);
    return 1;
}
