#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vn_renderer.h"
#include "vn_frontend.h"
#include "vn_pack.h"
#include "vn_vm.h"
#include "vn_error.h"

#define VN_MAX_CHOICE_SEQ 64u

typedef struct {
    vn_u8 items[VN_MAX_CHOICE_SEQ];
    vn_u32 count;
    vn_u32 cursor;
} ChoiceFeed;

static int parse_u32_range(const char* text, long min_value, long max_value, vn_u32* out_value) {
    long value;
    char* end_ptr;

    if (text == (const char*)0 || out_value == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }

    value = strtol(text, &end_ptr, 10);
    if (end_ptr == text || *end_ptr != '\0' || value < min_value || value > max_value) {
        return VN_E_FORMAT;
    }

    *out_value = (vn_u32)value;
    return VN_OK;
}

static int parse_resolution(const char* text, vn_u16* out_w, vn_u16* out_h) {
    const char* x_ptr;
    long w;
    long h;
    char* end_ptr;

    if (text == (const char*)0 || out_w == (vn_u16*)0 || out_h == (vn_u16*)0) {
        return VN_E_INVALID_ARG;
    }
    x_ptr = strchr(text, 'x');
    if (x_ptr == (const char*)0) {
        return VN_E_FORMAT;
    }

    w = strtol(text, &end_ptr, 10);
    if (end_ptr != x_ptr || w <= 0 || w > 65535) {
        return VN_E_FORMAT;
    }
    h = strtol(x_ptr + 1, &end_ptr, 10);
    if (*end_ptr != '\0' || h <= 0 || h > 65535) {
        return VN_E_FORMAT;
    }

    *out_w = (vn_u16)w;
    *out_h = (vn_u16)h;
    return VN_OK;
}

static vn_u32 parse_backend_flag(const char* value) {
    if (value == (const char*)0) {
        return 0u;
    }
    if (strcmp(value, "scalar") == 0) {
        return VN_RENDERER_FLAG_FORCE_SCALAR;
    }
    if (strcmp(value, "avx2") == 0) {
        return VN_RENDERER_FLAG_FORCE_AVX2;
    }
    if (strcmp(value, "neon") == 0) {
        return VN_RENDERER_FLAG_FORCE_NEON;
    }
    if (strcmp(value, "rvv") == 0) {
        return VN_RENDERER_FLAG_FORCE_RVV;
    }
    return 0u;
}

static int parse_scene_id(const char* value, vn_u32* out_scene_id) {
    if (value == (const char*)0 || out_scene_id == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }
    if (strcmp(value, "S0") == 0) {
        *out_scene_id = VN_SCENE_S0;
        return VN_OK;
    }
    if (strcmp(value, "S1") == 0) {
        *out_scene_id = VN_SCENE_S1;
        return VN_OK;
    }
    if (strcmp(value, "S2") == 0) {
        *out_scene_id = VN_SCENE_S2;
        return VN_OK;
    }
    if (strcmp(value, "S3") == 0) {
        *out_scene_id = VN_SCENE_S3;
        return VN_OK;
    }
    return VN_E_FORMAT;
}

static vn_u32 scene_script_res_id(vn_u32 scene_id) {
    if (scene_id == VN_SCENE_S1) {
        return 1u;
    }
    if (scene_id == VN_SCENE_S2) {
        return 2u;
    }
    if (scene_id == VN_SCENE_S3) {
        return 3u;
    }
    return 0u;
}

static int parse_choice_seq(const char* text, ChoiceFeed* out_feed) {
    const char* p;

    if (text == (const char*)0 || out_feed == (ChoiceFeed*)0) {
        return VN_E_INVALID_ARG;
    }

    out_feed->count = 0u;
    out_feed->cursor = 0u;
    p = text;

    while (*p != '\0') {
        const char* token_start;
        const char* token_end;
        long value;
        char* parse_end;
        char tmp[16];
        vn_u32 len;

        while (*p == ' ' || *p == ',') {
            p += 1;
        }
        if (*p == '\0') {
            break;
        }

        token_start = p;
        while (*p != '\0' && *p != ',') {
            p += 1;
        }
        token_end = p;

        len = (vn_u32)(token_end - token_start);
        if (len == 0u || len >= (vn_u32)sizeof(tmp)) {
            return VN_E_FORMAT;
        }

        memcpy(tmp, token_start, len);
        tmp[len] = '\0';
        value = strtol(tmp, &parse_end, 10);
        if (parse_end == tmp || *parse_end != '\0' || value < 0l || value > 255l) {
            return VN_E_FORMAT;
        }

        if (out_feed->count >= VN_MAX_CHOICE_SEQ) {
            return VN_E_NOMEM;
        }
        out_feed->items[out_feed->count] = (vn_u8)value;
        out_feed->count += 1u;

        if (*p == ',') {
            p += 1;
        }
    }

    return VN_OK;
}

static int load_scene_script(const VNPak* pak, vn_u32 scene_id, vn_u8** out_buf, vn_u32* out_size) {
    vn_u32 res_id;
    const ResourceEntry* entry;
    vn_u8* script_buf;
    vn_u32 read_size;
    int rc;

    if (pak == (const VNPak*)0 || out_buf == (vn_u8**)0 || out_size == (vn_u32*)0) {
        return VN_E_INVALID_ARG;
    }

    *out_buf = (vn_u8*)0;
    *out_size = 0u;

    res_id = scene_script_res_id(scene_id);
    entry = vnpak_get(pak, res_id);
    if (entry == (const ResourceEntry*)0 || entry->type != 2u || entry->data_size == 0u) {
        return VN_E_FORMAT;
    }

    script_buf = (vn_u8*)malloc((size_t)entry->data_size);
    if (script_buf == (vn_u8*)0) {
        return VN_E_NOMEM;
    }

    rc = vnpak_read_resource(pak, res_id, script_buf, entry->data_size, &read_size);
    if (rc != VN_OK) {
        free(script_buf);
        return rc;
    }
    if (read_size != entry->data_size) {
        free(script_buf);
        return VN_E_IO;
    }

    *out_buf = script_buf;
    *out_size = read_size;
    return VN_OK;
}

static void state_reset_frame_events(VNRuntimeState* state) {
    state->se_id = 0u;
    state->choice_count = 0u;
    state->choice_text_id = 0u;
}

static void state_from_vm(VNRuntimeState* state, VNState* vm) {
    state->text_id = vm_current_text_id(vm);
    state->text_speed_ms = vm_current_text_speed_ms(vm);
    state->vm_waiting = (vn_u32)vm_is_waiting(vm);
    state->vm_ended = (vn_u32)vm_is_ended(vm);
    state->vm_error = (vn_u32)vm_has_error(vm);
    state->fade_layer_mask = (vn_u32)vm_fade_layer_mask(vm);
    state->fade_alpha = (vn_u32)vm_fade_target_alpha(vm);
    state->fade_duration_ms = (vn_u32)vm_fade_duration_ms(vm);
    state->vm_fade_active = (state->fade_duration_ms > 0u || state->fade_alpha > 0u) ? 1u : 0u;
    state->bgm_id = (vn_u32)vm_current_bgm_id(vm);
    state->bgm_loop = (vn_u32)vm_current_bgm_loop(vm);
    state->se_id = (vn_u32)vm_take_se_id(vm);
    state->choice_count = (vn_u32)vm_last_choice_count(vm);
    state->choice_text_id = (vn_u32)vm_last_choice_text_id(vm);
    state->choice_selected_index = (vn_u32)vm_last_choice_selected_index(vm);
}

static void state_init_defaults(VNRuntimeState* state) {
    state->frame_index = 0u;
    state->clear_color = 200u;
    state->scene_id = VN_SCENE_S0;
    state->resource_count = 0u;
    state->text_id = 0u;
    state->text_speed_ms = 0u;
    state->vm_waiting = 0u;
    state->vm_ended = 0u;
    state->vm_error = 0u;
    state->vm_fade_active = 0u;
    state->fade_layer_mask = 0u;
    state->fade_alpha = 0u;
    state->fade_duration_ms = 0u;
    state->bgm_id = 0u;
    state->bgm_loop = 0u;
    state->se_id = 0u;
    state->choice_count = 0u;
    state->choice_text_id = 0u;
    state->choice_selected_index = 0u;
}

int main(int argc, char** argv) {
    RendererConfig cfg;
    VNRuntimeState state;
    VNRenderOp ops[16];
    VNPak pak;
    VNState vm;
    ChoiceFeed choice_feed;
    vn_u8* script_buf;
    vn_u32 script_size;
    const char* pack_path;
    const char* scene_name;
    vn_u32 frames;
    vn_u32 dt_ms;
    vn_u32 frame;
    vn_u32 frames_executed;
    vn_u32 trace;
    vn_u32 op_count;
    vn_u32 last_choice_serial;
    vn_u32 rc_u32;
    int rc;
    int i;
    int pak_opened;
    int vm_ready;

    cfg.width = 600;
    cfg.height = 800;
    cfg.flags = VN_RENDERER_FLAG_SIMD;

    state_init_defaults(&state);

    choice_feed.count = 0u;
    choice_feed.cursor = 0u;

    script_buf = (vn_u8*)0;
    script_size = 0u;
    pack_path = "assets/demo/demo.vnpak";
    scene_name = "S0";
    frames = 1u;
    dt_ms = 16u;
    trace = 0u;

    pak.path = (const char*)0;
    pak.version = 0u;
    pak.resource_count = 0u;
    pak.entries = (ResourceEntry*)0;
    pak_opened = VN_FALSE;
    vm_ready = VN_FALSE;
    last_choice_serial = 0u;
    frames_executed = 0u;

    for (i = 1; i < argc; ++i) {
        const char* arg;
        arg = argv[i];

        if (strcmp(arg, "--backend") == 0) {
            vn_u32 force_flag;
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --backend\n");
                return 2;
            }
            i += 1;
            force_flag = parse_backend_flag(argv[i]);
            if (force_flag != 0u) {
                cfg.flags &= ~(VN_RENDERER_FLAG_FORCE_SCALAR |
                               VN_RENDERER_FLAG_FORCE_AVX2 |
                               VN_RENDERER_FLAG_FORCE_NEON |
                               VN_RENDERER_FLAG_FORCE_RVV);
                cfg.flags |= force_flag;
            }
        } else if (strncmp(arg, "--backend=", 10) == 0) {
            vn_u32 force_flag;
            force_flag = parse_backend_flag(arg + 10);
            if (force_flag != 0u) {
                cfg.flags &= ~(VN_RENDERER_FLAG_FORCE_SCALAR |
                               VN_RENDERER_FLAG_FORCE_AVX2 |
                               VN_RENDERER_FLAG_FORCE_NEON |
                               VN_RENDERER_FLAG_FORCE_RVV);
                cfg.flags |= force_flag;
            }
        } else if (strcmp(arg, "--resolution") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --resolution\n");
                return 2;
            }
            i += 1;
            rc = parse_resolution(argv[i], &cfg.width, &cfg.height);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid resolution: %s\n", argv[i]);
                return 2;
            }
        } else if (strncmp(arg, "--resolution=", 13) == 0) {
            rc = parse_resolution(arg + 13, &cfg.width, &cfg.height);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid resolution: %s\n", arg + 13);
                return 2;
            }
        } else if (strcmp(arg, "--scene") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --scene\n");
                return 2;
            }
            i += 1;
            scene_name = argv[i];
        } else if (strncmp(arg, "--scene=", 8) == 0) {
            scene_name = arg + 8;
        } else if (strcmp(arg, "--pack") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --pack\n");
                return 2;
            }
            i += 1;
            pack_path = argv[i];
        } else if (strncmp(arg, "--pack=", 7) == 0) {
            pack_path = arg + 7;
        } else if (strcmp(arg, "--choice-index") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --choice-index\n");
                return 2;
            }
            i += 1;
            rc = parse_u32_range(argv[i], 0l, 255l, &rc_u32);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --choice-index: %s\n", argv[i]);
                return 2;
            }
            state.choice_selected_index = rc_u32;
        } else if (strncmp(arg, "--choice-index=", 15) == 0) {
            rc = parse_u32_range(arg + 15, 0l, 255l, &rc_u32);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --choice-index: %s\n", arg + 15);
                return 2;
            }
            state.choice_selected_index = rc_u32;
        } else if (strcmp(arg, "--choice-seq") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --choice-seq\n");
                return 2;
            }
            i += 1;
            rc = parse_choice_seq(argv[i], &choice_feed);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --choice-seq: %s\n", argv[i]);
                return 2;
            }
        } else if (strncmp(arg, "--choice-seq=", 13) == 0) {
            rc = parse_choice_seq(arg + 13, &choice_feed);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --choice-seq: %s\n", arg + 13);
                return 2;
            }
        } else if (strcmp(arg, "--frames") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --frames\n");
                return 2;
            }
            i += 1;
            rc = parse_u32_range(argv[i], 1l, 1000000l, &frames);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --frames: %s\n", argv[i]);
                return 2;
            }
        } else if (strncmp(arg, "--frames=", 9) == 0) {
            rc = parse_u32_range(arg + 9, 1l, 1000000l, &frames);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --frames: %s\n", arg + 9);
                return 2;
            }
        } else if (strcmp(arg, "--dt-ms") == 0) {
            if ((i + 1) >= argc) {
                (void)fprintf(stderr, "missing value for --dt-ms\n");
                return 2;
            }
            i += 1;
            rc = parse_u32_range(argv[i], 0l, 1000l, &dt_ms);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --dt-ms: %s\n", argv[i]);
                return 2;
            }
        } else if (strncmp(arg, "--dt-ms=", 8) == 0) {
            rc = parse_u32_range(arg + 8, 0l, 1000l, &dt_ms);
            if (rc != VN_OK) {
                (void)fprintf(stderr, "invalid --dt-ms: %s\n", arg + 8);
                return 2;
            }
        } else if (strcmp(arg, "--trace") == 0) {
            trace = 1u;
        }
    }

    rc = parse_scene_id(scene_name, &state.scene_id);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "invalid scene: %s\n", scene_name);
        return 2;
    }

    state.clear_color = (vn_u32)(200u + (state.scene_id * 12u));

    rc = vnpak_open(&pak, pack_path);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vnpak_open failed rc=%d path=%s\n", rc, pack_path);
        return 1;
    }
    pak_opened = VN_TRUE;
    state.resource_count = pak.resource_count;

    rc = load_scene_script(&pak, state.scene_id, &script_buf, &script_size);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "load_scene_script failed rc=%d scene=%s\n", rc, scene_name);
        vnpak_close(&pak);
        return 1;
    }

    if (vm_init(&vm, script_buf, script_size) != VN_TRUE) {
        (void)fprintf(stderr, "vm_init failed scene=%s\n", scene_name);
        free(script_buf);
        vnpak_close(&pak);
        return 1;
    }
    vm_ready = VN_TRUE;
    last_choice_serial = vm_choice_serial(&vm);

    rc = renderer_init(&cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "renderer_init failed rc=%d\n", rc);
        free(script_buf);
        vnpak_close(&pak);
        return 1;
    }

    for (frame = 0u; frame < frames; ++frame) {
        vn_u32 choice_serial_now;
        vn_u8 applied_choice;

        state.frame_index = frame;
        state_reset_frame_events(&state);

        applied_choice = (vn_u8)(state.choice_selected_index & 0xFFu);
        if (choice_feed.count > 0u && choice_feed.cursor < choice_feed.count) {
            applied_choice = choice_feed.items[choice_feed.cursor];
        }
        vm_set_choice_index(&vm, applied_choice);

        vm_step(&vm, dt_ms);
        state_from_vm(&state, &vm);

        op_count = 16u;
        rc = build_render_ops(&state, ops, &op_count);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "build_render_ops failed rc=%d frame=%u\n", rc, (unsigned int)frame);
            renderer_shutdown();
            free(script_buf);
            vnpak_close(&pak);
            return 1;
        }

        renderer_begin_frame();
        renderer_submit(ops, op_count);
        renderer_end_frame();

        choice_serial_now = vm_choice_serial(&vm);
        if (choice_serial_now != last_choice_serial) {
            last_choice_serial = choice_serial_now;
            if (choice_feed.cursor < choice_feed.count) {
                choice_feed.cursor += 1u;
            }
        }

        frames_executed = frame + 1u;

        if (trace != 0u) {
            (void)printf("frame=%u text=%u wait=%u end=%u fade=%u bgm=%u se=%u choice_count=%u choice_sel=%u choice_text=%u ops=%u\n",
                         (unsigned int)state.frame_index,
                         (unsigned int)state.text_id,
                         (unsigned int)state.vm_waiting,
                         (unsigned int)state.vm_ended,
                         (unsigned int)state.fade_alpha,
                         (unsigned int)state.bgm_id,
                         (unsigned int)state.se_id,
                         (unsigned int)state.choice_count,
                         (unsigned int)state.choice_selected_index,
                         (unsigned int)state.choice_text_id,
                         (unsigned int)op_count);
        }

        if (state.vm_ended != 0u || state.vm_error != 0u) {
            break;
        }
    }

    (void)printf("vn_player ok backend=%s resolution=%ux%u scene=%s frames=%u dt=%u resources=%u text=%u wait=%u end=%u fade=%u bgm=%u se=%u choice=%u choice_sel=%u choice_text=%u err=%u ops=%u\n",
                 renderer_backend_name(),
                 (unsigned int)cfg.width,
                 (unsigned int)cfg.height,
                 scene_name,
                 (unsigned int)frames_executed,
                 (unsigned int)dt_ms,
                 (unsigned int)state.resource_count,
                 (unsigned int)state.text_id,
                 (unsigned int)state.vm_waiting,
                 (unsigned int)state.vm_ended,
                 (unsigned int)state.fade_alpha,
                 (unsigned int)state.bgm_id,
                 (unsigned int)state.se_id,
                 (unsigned int)state.choice_count,
                 (unsigned int)state.choice_selected_index,
                 (unsigned int)state.choice_text_id,
                 (unsigned int)state.vm_error,
                 (unsigned int)op_count);

    renderer_shutdown();
    if (vm_ready != VN_FALSE) {
        free(script_buf);
    }
    if (pak_opened == VN_TRUE) {
        vnpak_close(&pak);
    }
    return 0;
}
