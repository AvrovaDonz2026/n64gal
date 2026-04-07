#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_internal.h"

static int runtime_perf_flag_enabled(vn_u32 perf_flags, vn_u32 flag) {
    return ((perf_flags & flag) != 0u) ? VN_TRUE : VN_FALSE;
}

vn_u32 runtime_supported_perf_flags(void) {
    return VN_RUNTIME_PERF_OP_CACHE | VN_RUNTIME_PERF_FRAME_REUSE | VN_RUNTIME_PERF_DIRTY_TILE | VN_RUNTIME_PERF_DYNAMIC_RESOLUTION;
}

static void runtime_perf_flag_set(vn_u32* perf_flags, vn_u32 flag, int enabled) {
    if (perf_flags == (vn_u32*)0) {
        return;
    }
    if (enabled != VN_FALSE) {
        *perf_flags |= flag;
    } else {
        *perf_flags &= ~flag;
    }
}

void runtime_dirty_stats_reset(VNRuntimeSession* session);

void runtime_render_cache_invalidate(VNRuntimeSession* session) {
    if (session == (VNRuntimeSession*)0) {
        return;
    }
    session->frame_reuse_valid = 0u;
    session->op_cache_stamp = 0u;
    (void)memset(session->op_cache, 0, sizeof(session->op_cache));
}

void runtime_dirty_planner_reconfigure(VNRuntimeSession* session,
                                       vn_u16 width,
                                       vn_u16 height) {
    vn_u32 dirty_word_count;

    if (session == (VNRuntimeSession*)0) {
        return;
    }

    if (session->dirty_bits != (vn_u32*)0) {
        free(session->dirty_bits);
        session->dirty_bits = (vn_u32*)0;
    }

    dirty_word_count = 0u;
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_DIRTY_TILE) != VN_FALSE) {
        dirty_word_count = vn_dirty_word_count(width, height);
        if (dirty_word_count != 0u) {
            session->dirty_bits = (vn_u32*)malloc((size_t)dirty_word_count * sizeof(vn_u32));
            if (session->dirty_bits == (vn_u32*)0) {
                runtime_perf_flag_set(&session->perf_flags, VN_RUNTIME_PERF_DIRTY_TILE, VN_FALSE);
                dirty_word_count = 0u;
                if (session->emit_logs != 0u) {
                    (void)fprintf(stderr, "dirty tile planner disabled: scratch alloc failed\n");
                }
            }
        }
    }

    vn_dirty_planner_init(&session->dirty_planner, width, height, session->dirty_bits, dirty_word_count);
    runtime_dirty_stats_reset(session);
}

int runtime_renderer_reconfigure(VNRuntimeSession* session,
                                 vn_u16 width,
                                 vn_u16 height) {
    RendererConfig next_cfg;
    vn_u16 old_width;
    vn_u16 old_height;
    int rc;

    if (session == (VNRuntimeSession*)0 || width == 0u || height == 0u) {
        return VN_E_INVALID_ARG;
    }
    if (session->renderer_cfg.width == width && session->renderer_cfg.height == height) {
        return VN_OK;
    }

    old_width = session->renderer_cfg.width;
    old_height = session->renderer_cfg.height;
    next_cfg = session->renderer_cfg;
    next_cfg.width = width;
    next_cfg.height = height;

    if (session->renderer_ready != VN_FALSE) {
        renderer_shutdown();
        session->renderer_ready = VN_FALSE;
    }

    rc = renderer_init(&next_cfg);
    if (rc != VN_OK) {
        session->renderer_cfg.width = old_width;
        session->renderer_cfg.height = old_height;
        rc = renderer_init(&session->renderer_cfg);
        if (rc == VN_OK) {
            session->renderer_ready = VN_TRUE;
        }
        return VN_E_RENDER_STATE;
    }

    session->renderer_cfg = next_cfg;
    session->renderer_ready = VN_TRUE;
    runtime_render_cache_invalidate(session);
    runtime_dirty_planner_reconfigure(session, width, height);
    return VN_OK;
}

int runtime_dynamic_resolution_maybe_switch(VNRuntimeSession* session,
                                            double frame_ms) {
    vn_u32 next_tier;
    vn_u32 current_tier;
    vn_u32 dirty_tile_count;
    vn_u32 dirty_rect_count;
    vn_u32 dirty_full_redraw;
    double window_p95_ms;
    const VNDynResTier* current_dims;
    const VNDynResTier* next_dims;
    int rc;

    if (session == (VNRuntimeSession*)0) {
        return VN_OK;
    }
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_DYNAMIC_RESOLUTION) == VN_FALSE) {
        return VN_OK;
    }

    next_tier = 0u;
    window_p95_ms = 0.0;
    if (vn_dynres_should_switch(&session->dynamic_resolution, frame_ms, &next_tier, &window_p95_ms) == 0) {
        return VN_OK;
    }

    current_tier = vn_dynres_get_current_tier(&session->dynamic_resolution);
    current_dims = vn_dynres_get_current_dims(&session->dynamic_resolution);
    next_dims = vn_dynres_get_tier(&session->dynamic_resolution, next_tier);
    if (current_dims == (const VNDynResTier*)0 || next_dims == (const VNDynResTier*)0) {
        vn_dynres_reset_history(&session->dynamic_resolution);
        return VN_OK;
    }

    dirty_tile_count = session->dirty_tile_count;
    dirty_rect_count = session->dirty_rect_count;
    dirty_full_redraw = session->dirty_full_redraw;

    rc = runtime_renderer_reconfigure(session, next_dims->width, next_dims->height);
    if (rc != VN_OK) {
        vn_dynres_reset_history(&session->dynamic_resolution);
        if (session->emit_logs != 0u) {
            (void)fprintf(stderr,
                          "WARN perf resolution_switch_failed from=%s to=%s rc=%d\n",
                          vn_dynres_tier_name(current_tier),
                          vn_dynres_tier_name(next_tier),
                          rc);
        }
        return rc;
    }

    session->dirty_tile_count = dirty_tile_count;
    session->dirty_rect_count = dirty_rect_count;
    session->dirty_full_redraw = dirty_full_redraw;
    (void)vn_dynres_apply_tier(&session->dynamic_resolution, next_tier);
    if (session->emit_logs != 0u) {
        (void)printf("INFO perf resolution_switch from=%s to=%s resolution=%ux%u p95_ms=%.3f\n",
                     vn_dynres_tier_name(current_tier),
                     vn_dynres_tier_name(next_tier),
                     (unsigned int)next_dims->width,
                     (unsigned int)next_dims->height,
                     window_p95_ms);
    }
    return VN_OK;
}

void runtime_dirty_stats_reset(VNRuntimeSession* session) {
    if (session == (VNRuntimeSession*)0) {
        return;
    }
    session->dirty_tile_count = 0u;
    session->dirty_rect_count = 0u;
    session->dirty_full_redraw = 0u;
    vn_dirty_plan_reset(&session->dirty_plan);
}

static void runtime_dirty_plan_seed(VNRuntimeSession* session,
                                    VNDirtyPlan* plan) {
    VNDirtyPlannerState* state;

    if (session == (VNRuntimeSession*)0 || plan == (VNDirtyPlan*)0) {
        return;
    }
    state = &session->dirty_planner;
    vn_dirty_plan_reset(plan);
    plan->width = state->width;
    plan->height = state->height;
    plan->tiles_x = state->tiles_x;
    plan->tiles_y = state->tiles_y;
    plan->bit_word_count = state->bit_word_count;
}

static void runtime_dirty_plan_force_full_redraw(VNRuntimeSession* session,
                                                 VNDirtyPlan* plan) {
    vn_u32 total_tiles;

    if (session == (VNRuntimeSession*)0 || plan == (VNDirtyPlan*)0) {
        return;
    }
    runtime_dirty_plan_seed(session, plan);
    total_tiles = (vn_u32)plan->tiles_x * (vn_u32)plan->tiles_y;
    plan->valid = 1u;
    plan->full_redraw = 1u;
    plan->dirty_tile_count = total_tiles;
    plan->dirty_rect_count = 0u;
    if (plan->width != 0u && plan->height != 0u) {
        plan->rects[0].x = 0u;
        plan->rects[0].y = 0u;
        plan->rects[0].w = plan->width;
        plan->rects[0].h = plan->height;
        plan->dirty_rect_count = 1u;
    }
}

static int runtime_dirty_plan_clear_equal(const VNRenderOp* a,
                                          const VNRenderOp* b) {
    if (a == (const VNRenderOp*)0 || b == (const VNRenderOp*)0) {
        return VN_FALSE;
    }
    return (a->op == b->op &&
            a->layer == b->layer &&
            a->tex_id == b->tex_id &&
            a->x == b->x &&
            a->y == b->y &&
            a->w == b->w &&
            a->h == b->h &&
            a->alpha == b->alpha &&
            a->flags == b->flags) ? VN_TRUE : VN_FALSE;
}

static int runtime_prepare_dirty_plan_fast_path(VNRuntimeSession* session,
                                                const VNRenderOp* ops,
                                                vn_u32 op_count) {
    VNDirtyPlannerState* state;
    vn_u32 i;

    if (session == (VNRuntimeSession*)0 || (ops == (const VNRenderOp*)0 && op_count != 0u)) {
        return VN_FALSE;
    }
    state = &session->dirty_planner;
    if (state->width == 0u || state->height == 0u || state->dirty_bits == (vn_u32*)0) {
        return VN_FALSE;
    }
    if (state->bit_word_count < vn_dirty_word_count(state->width, state->height)) {
        return VN_FALSE;
    }
    if (state->valid == 0u || state->prev_op_count != op_count || op_count == 0u) {
        runtime_dirty_plan_force_full_redraw(session, &session->dirty_plan);
        return VN_TRUE;
    }
    if (ops[0].op != VN_OP_CLEAR || state->prev_ops[0].op != VN_OP_CLEAR) {
        runtime_dirty_plan_force_full_redraw(session, &session->dirty_plan);
        return VN_TRUE;
    }
    if (runtime_dirty_plan_clear_equal(&ops[0], &state->prev_ops[0]) == VN_FALSE) {
        runtime_dirty_plan_force_full_redraw(session, &session->dirty_plan);
        return VN_TRUE;
    }
    for (i = 1u; i < op_count; ++i) {
        if (ops[i].op == VN_OP_FADE || state->prev_ops[i].op == VN_OP_FADE) {
            runtime_dirty_plan_force_full_redraw(session, &session->dirty_plan);
            return VN_TRUE;
        }
        if (ops[i].op != state->prev_ops[i].op) {
            runtime_dirty_plan_force_full_redraw(session, &session->dirty_plan);
            return VN_TRUE;
        }
    }
    return VN_FALSE;
}

static void runtime_dirty_planner_invalidate(VNRuntimeSession* session) {
    if (session == (VNRuntimeSession*)0) {
        return;
    }
    vn_dirty_planner_invalidate(&session->dirty_planner);
    runtime_dirty_stats_reset(session);
}

void runtime_prepare_dirty_plan(VNRuntimeSession* session,
                                const VNRenderOp* ops,
                                vn_u32 op_count) {
    int rc;

    if (session == (VNRuntimeSession*)0) {
        return;
    }
    runtime_dirty_stats_reset(session);
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_DIRTY_TILE) == VN_FALSE) {
        return;
    }
    if (runtime_prepare_dirty_plan_fast_path(session, ops, op_count) == VN_FALSE) {
        rc = vn_dirty_planner_build(&session->dirty_planner, ops, op_count, &session->dirty_plan);
        if (rc != VN_OK) {
            runtime_dirty_planner_invalidate(session);
            return;
        }
    }
    session->dirty_tile_count = session->dirty_plan.dirty_tile_count;
    session->dirty_rect_count = session->dirty_plan.dirty_rect_count;
    session->dirty_full_redraw = session->dirty_plan.full_redraw;
    session->dirty_tile_frames += 1u;
    session->dirty_tile_total += session->dirty_plan.dirty_tile_count;
    session->dirty_rect_total += session->dirty_plan.dirty_rect_count;
    if (session->dirty_plan.full_redraw != 0u) {
        session->dirty_full_redraws += 1u;
    }
}

void runtime_commit_dirty_plan(VNRuntimeSession* session,
                               const VNRenderOp* ops,
                               vn_u32 op_count) {
    if (session == (VNRuntimeSession*)0) {
        return;
    }
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_DIRTY_TILE) == VN_FALSE) {
        runtime_dirty_planner_invalidate(session);
        return;
    }
    if (session->dirty_plan.full_redraw != 0u) {
        vn_dirty_planner_commit_full_redraw(&session->dirty_planner, ops, op_count);
        return;
    }
    vn_dirty_planner_commit(&session->dirty_planner, ops, op_count);
}

void runtime_submit_render_ops(VNRuntimeSession* session,
                               const VNRenderOp* ops,
                               vn_u32 op_count) {
    VNRenderDirtySubmit dirty_submit;

    if (session == (VNRuntimeSession*)0) {
        return;
    }

    renderer_begin_frame();
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_DIRTY_TILE) != VN_FALSE &&
        session->dirty_plan.valid != 0u &&
        session->dirty_plan.full_redraw == 0u) {
        dirty_submit.width = session->dirty_plan.width;
        dirty_submit.height = session->dirty_plan.height;
        dirty_submit.rect_count = session->dirty_plan.dirty_rect_count;
        dirty_submit.full_redraw = session->dirty_plan.full_redraw;
        dirty_submit.rects = session->dirty_plan.rects;
        renderer_submit_dirty(ops, op_count, &dirty_submit);
    } else {
        renderer_submit(ops, op_count);
    }
    renderer_end_frame();
}

static vn_u32 runtime_render_sprite_phase(const VNRuntimeState* state) {
    if (state == (const VNRuntimeState*)0) {
        return 0u;
    }
    return state->frame_index % 160u;
}

static vn_u32 runtime_render_fade_phase(const VNRuntimeState* state) {
    if (state == (const VNRuntimeState*)0) {
        return 0u;
    }
    return state->frame_index & 0x3Fu;
}

static void runtime_render_key_init(RenderOpCacheKey* out_key, const VNRuntimeState* state) {
    vn_u32 scene_id;
    vn_u32 text_flags;

    if (out_key == (RenderOpCacheKey*)0) {
        return;
    }
    (void)memset(out_key, 0, sizeof(*out_key));
    if (state == (const VNRuntimeState*)0) {
        return;
    }

    scene_id = state->scene_id;
    text_flags = 0u;
    if (state->text_speed_ms > 0u) {
        text_flags |= 1u;
    }
    if (state->choice_count > 0u) {
        text_flags |= 2u;
    }
    if (state->vm_error != 0u) {
        text_flags |= 4u;
    }
    if (state->choice_selected_index > 0u) {
        text_flags |= 8u;
    }

    if (scene_id == VN_SCENE_S10) {
        out_key->op_count = 6u;
    } else if (state->vm_fade_active != 0u || state->vm_waiting != 0u || scene_id == VN_SCENE_S1 || scene_id == VN_SCENE_S3) {
        out_key->op_count = 4u;
    } else {
        out_key->op_count = 3u;
    }
    out_key->clear_gray = state->clear_color & 0xFFu;
    out_key->clear_flags = (state->resource_count > 0u) ? 1u : 0u;
    out_key->scene_id = scene_id;
    out_key->sprite_flags = (state->se_id != 0u) ? 1u : 0u;
    if (state->text_id != 0u) {
        out_key->text_tex_id = state->text_id;
    } else {
        out_key->text_tex_id = 100u + scene_id;
    }
    out_key->text_flags = text_flags;
    out_key->text_alpha = (state->vm_ended != 0u) ? 180u : 255u;

    if (out_key->op_count > 3u) {
        if (state->vm_fade_active != 0u) {
            out_key->fade_flags = 2u;
            out_key->fade_mask = state->fade_layer_mask & 0xFFFFu;
        } else {
            out_key->fade_flags = (state->vm_waiting != 0u) ? 1u : 0u;
            out_key->fade_mask = 0u;
        }
    }
}

static void runtime_render_patch_cached_ops(const VNRuntimeState* state,
                                            VNRenderOp* ops,
                                            vn_u32 op_count) {
    vn_u32 i;
    vn_u32 fade_phase;
    vn_i16 sprite_x;

    if (state == (const VNRuntimeState*)0 ||
        ops == (VNRenderOp*)0 ||
        op_count == 0u) {
        return;
    }

    sprite_x = (vn_i16)(40 + runtime_render_sprite_phase(state));
    fade_phase = runtime_render_fade_phase(state);
    for (i = 0u; i < op_count; ++i) {
        if (ops[i].op == VN_OP_SPRITE) {
            ops[i].x = sprite_x;
            ops[i].flags = (vn_u8)(state->se_id != 0u ? 1u : 0u);
        } else if (ops[i].op == VN_OP_FADE) {
            if (state->vm_fade_active != 0u) {
                ops[i].tex_id = (vn_u16)(state->fade_layer_mask & 0xFFFFu);
                ops[i].alpha = (vn_u8)(state->fade_alpha & 0xFFu);
                ops[i].flags = 2u;
            } else {
                ops[i].tex_id = 0u;
                ops[i].alpha = (vn_u8)(state->vm_waiting != 0u ? (120u + fade_phase) : (fade_phase * 3u));
                ops[i].flags = (vn_u8)(state->vm_waiting != 0u ? 1u : 0u);
            }
        }
    }
}

static vn_u32 runtime_render_key_hash(const RenderOpCacheKey* key_data) {
    vn_u32 hash;
    vn_u32 value;

    if (key_data == (const RenderOpCacheKey*)0) {
        return 0u;
    }

    hash = 2166136261u;
#define VN_HASH_VALUE(expr)     value = (vn_u32)(expr);     hash ^= value;     hash *= 16777619u
    VN_HASH_VALUE(key_data->op_count);
    VN_HASH_VALUE(key_data->clear_gray);
    VN_HASH_VALUE(key_data->clear_flags);
    VN_HASH_VALUE(key_data->scene_id);
    VN_HASH_VALUE(key_data->sprite_flags);
    VN_HASH_VALUE(key_data->text_tex_id);
    VN_HASH_VALUE(key_data->text_flags);
    VN_HASH_VALUE(key_data->text_alpha);
    VN_HASH_VALUE(key_data->fade_flags);
    VN_HASH_VALUE(key_data->fade_mask);
#undef VN_HASH_VALUE
    return hash;
}

static int runtime_render_key_equal(const RenderOpCacheKey* a, const RenderOpCacheKey* b) {
    if (a == (const RenderOpCacheKey*)0 || b == (const RenderOpCacheKey*)0) {
        return VN_FALSE;
    }
    return (a->op_count == b->op_count &&
            a->clear_gray == b->clear_gray &&
            a->clear_flags == b->clear_flags &&
            a->scene_id == b->scene_id &&
            a->sprite_flags == b->sprite_flags &&
            a->text_tex_id == b->text_tex_id &&
            a->text_flags == b->text_flags &&
            a->text_alpha == b->text_alpha &&
            a->fade_flags == b->fade_flags &&
            a->fade_mask == b->fade_mask) ? VN_TRUE : VN_FALSE;
}

static int runtime_frame_reuse_eligible(const VNRuntimeState* state) {
    if (state == (const VNRuntimeState*)0) {
        return VN_FALSE;
    }
    if (state->vm_fade_active != 0u) {
        return VN_FALSE;
    }
    if (state->se_id != 0u) {
        return VN_FALSE;
    }
    return VN_TRUE;
}

int runtime_prepare_frame_reuse(VNRuntimeSession* session,
                                const VNRuntimeState* state,
                                RenderOpCacheKey* out_key,
                                int* out_reuse_hit) {
    if (out_reuse_hit != (int*)0) {
        *out_reuse_hit = VN_FALSE;
    }
    if (session == (VNRuntimeSession*)0 ||
        state == (const VNRuntimeState*)0 ||
        out_key == (RenderOpCacheKey*)0) {
        return VN_FALSE;
    }
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_FRAME_REUSE) == VN_FALSE) {
        session->frame_reuse_valid = 0u;
        return VN_FALSE;
    }
    if (runtime_frame_reuse_eligible(state) == VN_FALSE) {
        session->frame_reuse_valid = 0u;
        return VN_FALSE;
    }

    runtime_render_key_init(out_key, state);
    if (session->frame_reuse_valid != 0u &&
        runtime_render_key_equal(&session->frame_reuse_key, out_key) != VN_FALSE) {
        session->frame_reuse_hits += 1u;
        if (out_reuse_hit != (int*)0) {
            *out_reuse_hit = VN_TRUE;
        }
        return VN_TRUE;
    }

    session->frame_reuse_valid = 0u;
    session->frame_reuse_misses += 1u;
    return VN_TRUE;
}

void runtime_commit_frame_reuse(VNRuntimeSession* session,
                                const RenderOpCacheKey* key_data) {
    if (session == (VNRuntimeSession*)0 || key_data == (const RenderOpCacheKey*)0) {
        return;
    }
    session->frame_reuse_key = *key_data;
    session->frame_reuse_valid = 1u;
}

int runtime_build_render_ops_cached(VNRuntimeSession* session,
                                    const VNRuntimeState* state,
                                    VNRenderOp* out_ops,
                                    vn_u32* io_count,
                                    int* out_cache_hit) {
    RenderOpCacheKey render_key;
    vn_u32 key;
    vn_u32 i;
    vn_u32 victim_index;
    vn_u32 victim_stamp;
    int victim_found;

    if (session == (VNRuntimeSession*)0 ||
        state == (const VNRuntimeState*)0 ||
        out_ops == (VNRenderOp*)0 ||
        io_count == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }
    if (out_cache_hit != (int*)0) {
        *out_cache_hit = VN_FALSE;
    }
    if (runtime_perf_flag_enabled(session->perf_flags, VN_RUNTIME_PERF_OP_CACHE) == VN_FALSE) {
        return build_render_ops(state, out_ops, io_count);
    }

    runtime_render_key_init(&render_key, state);
    key = runtime_render_key_hash(&render_key);
    victim_index = 0u;
    victim_stamp = 0u;
    victim_found = VN_FALSE;

    for (i = 0u; i < VN_OP_CACHE_CAP; ++i) {
        RenderOpCacheEntry* entry;

        entry = &session->op_cache[i];
        if (entry->valid != 0u &&
            entry->key == key &&
            runtime_render_key_equal(&entry->render_key, &render_key) != VN_FALSE) {
            if (*io_count < entry->op_count) {
                *io_count = entry->op_count;
                return VN_E_NOMEM;
            }
            (void)memcpy(out_ops, entry->ops, (size_t)entry->op_count * sizeof(VNRenderOp));
            runtime_render_patch_cached_ops(state, out_ops, entry->op_count);
            *io_count = entry->op_count;
            session->op_cache_stamp += 1u;
            entry->stamp = session->op_cache_stamp;
            session->op_cache_hits += 1u;
            if (out_cache_hit != (int*)0) {
                *out_cache_hit = VN_TRUE;
            }
            return VN_OK;
        }
        if (entry->valid == 0u) {
            if (victim_found == VN_FALSE) {
                victim_index = i;
                victim_found = VN_TRUE;
            }
            continue;
        }
        if (victim_found == VN_FALSE || entry->stamp < victim_stamp) {
            victim_index = i;
            victim_stamp = entry->stamp;
            victim_found = VN_TRUE;
        }
    }

    session->op_cache_misses += 1u;
    {
        int rc;
        vn_u32 built_count;
        RenderOpCacheEntry* entry;

        built_count = *io_count;
        rc = build_render_ops(state, out_ops, &built_count);
        *io_count = built_count;
        if (rc != VN_OK) {
            return rc;
        }
        if (victim_found == VN_FALSE) {
            return VN_OK;
        }

        entry = &session->op_cache[victim_index];
        entry->valid = 1u;
        entry->key = key;
        entry->render_key = render_key;
        entry->op_count = built_count;
        (void)memcpy(entry->ops, out_ops, (size_t)built_count * sizeof(VNRenderOp));
        session->op_cache_stamp += 1u;
        entry->stamp = session->op_cache_stamp;
    }
    return VN_OK;
}

int vn_runtime_session_create(const VNRunConfig* cfg, VNRuntimeSession** out_session) {
    VNRunConfig cfg_local;
    const VNRunConfig* active_cfg;
    VNRuntimeSession* session;
    const char* pack_path;
    const char* scene_name;
    vn_u32 scene_id;
    vn_u32 force_flag;
    vn_u32 i;
    int rc;

    if (out_session == (VNRuntimeSession**)0) {
        return VN_E_INVALID_ARG;
    }
    *out_session = (VNRuntimeSession*)0;
    runtime_result_reset();

    active_cfg = cfg;
    if (active_cfg == (const VNRunConfig*)0) {
        vn_run_config_init(&cfg_local);
        active_cfg = &cfg_local;
    }

    if (active_cfg->choice_seq_count > VN_MAX_CHOICE_SEQ) {
        return VN_E_INVALID_ARG;
    }
    if (active_cfg->frames == 0u || active_cfg->dt_ms > 1000u) {
        return VN_E_INVALID_ARG;
    }
    if (active_cfg->width == 0u || active_cfg->height == 0u) {
        return VN_E_INVALID_ARG;
    }

    pack_path = active_cfg->pack_path;
    if (pack_path == (const char*)0 || pack_path[0] == '\0') {
        pack_path = "assets/demo/demo.vnpak";
    }
    scene_name = active_cfg->scene_name;
    if (scene_name == (const char*)0 || scene_name[0] == '\0') {
        scene_name = "S0";
    }

    rc = parse_scene_id(scene_name, &scene_id);
    if (rc != VN_OK) {
        return rc;
    }

    session = (VNRuntimeSession*)malloc(sizeof(VNRuntimeSession));
    if (session == (VNRuntimeSession*)0) {
        return VN_E_NOMEM;
    }
    (void)memset(session, 0, sizeof(VNRuntimeSession));

    state_init_defaults(&session->state);
    fade_player_init(&session->fade_player);
    keyboard_init(&session->keyboard);
    session->choice_feed.count = 0u;
    session->choice_feed.cursor = 0u;
    session->renderer_cfg.width = active_cfg->width;
    session->renderer_cfg.height = active_cfg->height;
    session->renderer_cfg.flags = VN_RENDERER_FLAG_SIMD;
    force_flag = parse_backend_flag(active_cfg->backend_name);
    if (force_flag != 0u) {
        session->renderer_cfg.flags &= ~(VN_RENDERER_FLAG_FORCE_SCALAR |
                                         VN_RENDERER_FLAG_FORCE_AVX2 |
                                         VN_RENDERER_FLAG_FORCE_NEON |
                                         VN_RENDERER_FLAG_FORCE_RVV |
                                         VN_RENDERER_FLAG_FORCE_AVX2_ASM);
        session->renderer_cfg.flags |= force_flag;
    }

    session->frames_limit = active_cfg->frames;
    session->dt_ms = active_cfg->dt_ms;
    session->trace = active_cfg->trace;
    session->emit_logs = active_cfg->emit_logs;
    session->hold_on_end = active_cfg->hold_on_end;
    session->perf_flags = active_cfg->perf_flags & runtime_supported_perf_flags();
    vn_dynres_init(&session->dynamic_resolution, active_cfg->width, active_cfg->height);
    session->default_choice_index = active_cfg->choice_index;
    session->keyboard.enabled = (active_cfg->keyboard != 0u) ? VN_TRUE : VN_FALSE;
    session->done = VN_FALSE;
    session->exit_code = 0;
    session->summary_emitted = VN_FALSE;
    runtime_dirty_planner_reconfigure(session, session->renderer_cfg.width, session->renderer_cfg.height);
    for (i = 0u; i < active_cfg->choice_seq_count; ++i) {
        session->choice_feed.items[i] = active_cfg->choice_seq[i];
    }
    session->choice_feed.count = active_cfg->choice_seq_count;

    session->state.scene_id = scene_id;
    session->state.clear_color = (vn_u32)(200u + (scene_id * 12u));

    rc = vnpak_open(&session->pak, pack_path);
    if (rc != VN_OK) {
        free(session);
        return rc;
    }
    session->pak_opened = VN_TRUE;
    session->state.resource_count = session->pak.resource_count;

    rc = load_scene_script(&session->pak, session->state.scene_id, &session->script_buf, &session->script_size);
    if (rc != VN_OK) {
        runtime_session_cleanup(session);
        free(session);
        return rc;
    }

    if (vm_init(&session->vm, session->script_buf, session->script_size) != VN_TRUE) {
        runtime_session_cleanup(session);
        free(session);
        return VN_E_FORMAT;
    }
    session->vm_ready = VN_TRUE;
    session->last_choice_serial = vm_choice_serial(&session->vm);

    rc = renderer_init(&session->renderer_cfg);
    if (rc != VN_OK) {
        runtime_session_cleanup(session);
        free(session);
        return rc;
    }
    session->renderer_ready = VN_TRUE;

    rc = keyboard_enable(&session->keyboard);
    if (session->keyboard.enabled == VN_TRUE && rc != VN_OK) {
        runtime_session_cleanup(session);
        free(session);
        return rc;
    }
    if (session->keyboard.active == VN_TRUE && session->emit_logs != 0u) {
        (void)printf("[keyboard] enabled: press 1-9 to select choice, t to toggle trace, q to quit\n");
    }

    runtime_result_publish(session);
    *out_session = session;
    return VN_OK;
}

int vn_runtime_session_is_done(const VNRuntimeSession* session) {
    if (session == (const VNRuntimeSession*)0) {
        return VN_TRUE;
    }
    return (session->done != VN_FALSE) ? VN_TRUE : VN_FALSE;
}

int vn_runtime_session_set_choice(VNRuntimeSession* session, vn_u8 choice_index) {
    if (session == (VNRuntimeSession*)0) {
        return VN_E_INVALID_ARG;
    }
    session->default_choice_index = choice_index;
    return VN_OK;
}

int vn_runtime_session_inject_input(VNRuntimeSession* session, const VNInputEvent* event) {
    if (session == (VNRuntimeSession*)0 || event == (const VNInputEvent*)0) {
        return VN_E_INVALID_ARG;
    }

    if (event->kind == VN_INPUT_KIND_CHOICE) {
        if (event->value0 > 255u) {
            return VN_E_INVALID_ARG;
        }
        session->injected_choice_index = (vn_u8)(event->value0 & 0xFFu);
        session->injected_has_choice = VN_TRUE;
        return VN_OK;
    }
    if (event->kind == VN_INPUT_KIND_KEY) {
        return runtime_session_inject_key_code(session, event->value0);
    }
    if (event->kind == VN_INPUT_KIND_TRACE_TOGGLE) {
        session->injected_trace_toggle_count += 1u;
        return VN_OK;
    }
    if (event->kind == VN_INPUT_KIND_QUIT) {
        session->injected_quit = VN_TRUE;
        return VN_OK;
    }
    return VN_E_UNSUPPORTED;
}

int vn_runtime_session_destroy(VNRuntimeSession* session) {
    if (session == (VNRuntimeSession*)0) {
        return VN_OK;
    }
    runtime_session_cleanup(session);
    free(session);
    return VN_OK;
}
