#include <stdio.h>

#include "../../src/core/dynamic_resolution.h"

static int require_dims(const VNDynResTier* tier,
                        vn_u16 width,
                        vn_u16 height,
                        const char* label) {
    if (tier == (const VNDynResTier*)0) {
        (void)fprintf(stderr, "missing tier for %s\n", label);
        return 1;
    }
    if (tier->width != width || tier->height != height) {
        (void)fprintf(stderr,
                      "%s dims mismatch got=%ux%u expected=%ux%u\n",
                      label,
                      (unsigned int)tier->width,
                      (unsigned int)tier->height,
                      (unsigned int)width,
                      (unsigned int)height);
        return 1;
    }
    return 0;
}

static int pump_switch(VNDynResState* state,
                       double frame_ms,
                       vn_u32 samples,
                       vn_u32 expected_next_tier,
                       const char* label) {
    vn_u32 i;
    vn_u32 next_tier;
    double p95_ms;
    int switched;

    next_tier = 0u;
    p95_ms = 0.0;
    switched = 0;
    for (i = 0u; i < samples; ++i) {
        switched = vn_dynres_should_switch(state, frame_ms, &next_tier, &p95_ms);
    }
    if (switched == 0) {
        (void)fprintf(stderr, "%s expected switch after %u samples\n", label, (unsigned int)samples);
        return 1;
    }
    if (next_tier != expected_next_tier) {
        (void)fprintf(stderr,
                      "%s tier mismatch got=%u expected=%u p95=%.3f\n",
                      label,
                      (unsigned int)next_tier,
                      (unsigned int)expected_next_tier,
                      p95_ms);
        return 1;
    }
    if (vn_dynres_apply_tier(state, next_tier) == 0) {
        (void)fprintf(stderr, "%s apply failed\n", label);
        return 1;
    }
    return 0;
}

int main(void) {
    VNDynResState state;

    vn_dynres_init(&state, 600u, 800u);
    if (vn_dynres_get_tier_count(&state) != 3u) {
        (void)fprintf(stderr, "tier_count=%u expected=3\n", (unsigned int)vn_dynres_get_tier_count(&state));
        return 1;
    }
    if (require_dims(vn_dynres_get_tier(&state, 0u), 600u, 800u, "R0") != 0) {
        return 1;
    }
    if (require_dims(vn_dynres_get_tier(&state, 1u), 450u, 600u, "R1") != 0) {
        return 1;
    }
    if (require_dims(vn_dynres_get_tier(&state, 2u), 300u, 400u, "R2") != 0) {
        return 1;
    }
    if (pump_switch(&state, 20.0, VN_DYNRES_DOWN_WINDOW, 1u, "downshift-to-R1") != 0) {
        return 1;
    }
    if (vn_dynres_get_current_tier(&state) != 1u) {
        (void)fprintf(stderr, "expected current tier R1\n");
        return 1;
    }
    if (pump_switch(&state, 20.0, VN_DYNRES_DOWN_WINDOW, 2u, "downshift-to-R2") != 0) {
        return 1;
    }
    if (vn_dynres_get_current_tier(&state) != 2u) {
        (void)fprintf(stderr, "expected current tier R2\n");
        return 1;
    }
    if (pump_switch(&state, 11.0, VN_DYNRES_UP_WINDOW, 1u, "upshift-to-R1") != 0) {
        return 1;
    }
    if (pump_switch(&state, 11.0, VN_DYNRES_UP_WINDOW, 0u, "upshift-to-R0") != 0) {
        return 1;
    }
    if (vn_dynres_get_switch_count(&state) != 4u) {
        (void)fprintf(stderr,
                      "switch_count=%u expected=4\n",
                      (unsigned int)vn_dynres_get_switch_count(&state));
        return 1;
    }

    (void)printf("test_dynamic_resolution ok tier=%s switches=%u\n",
                 vn_dynres_tier_name(vn_dynres_get_current_tier(&state)),
                 (unsigned int)vn_dynres_get_switch_count(&state));
    return 0;
}
