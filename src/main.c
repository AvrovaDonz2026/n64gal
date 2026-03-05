#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vn_renderer.h"
#include "vn_frontend.h"
#include "vn_pack.h"
#include "vn_vm.h"
#include "vn_error.h"

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

static int run_vm_from_pack(const VNPak* pak, vn_u32 scene_id, VNRuntimeState* io_state) {
    vn_u32 res_id;
    const ResourceEntry* entry;
    vn_u8* script_buf;
    vn_u32 read_size;
    int rc;
    VNState vm;
    int inited;

    if (pak == (const VNPak*)0 || io_state == (VNRuntimeState*)0) {
        return VN_E_INVALID_ARG;
    }

    res_id = scene_script_res_id(scene_id);
    entry = vnpak_get(pak, res_id);
    if (entry == (const ResourceEntry*)0) {
        return VN_E_FORMAT;
    }
    if (entry->type != 2u) {
        return VN_E_FORMAT;
    }
    if (entry->data_size == 0u) {
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

    inited = vm_init(&vm, script_buf, read_size);
    if (inited != VN_TRUE) {
        free(script_buf);
        return VN_E_FORMAT;
    }

    vm_step(&vm, 16u);

    io_state->text_id = vm_current_text_id(&vm);
    io_state->text_speed_ms = vm_current_text_speed_ms(&vm);
    io_state->vm_waiting = (vn_u32)vm_is_waiting(&vm);
    io_state->vm_ended = (vn_u32)vm_is_ended(&vm);

    free(script_buf);
    return VN_OK;
}

int main(int argc, char** argv) {
    RendererConfig cfg;
    VNRuntimeState state;
    VNRenderOp ops[16];
    VNPak pak;
    const char* pack_path;
    const char* scene_name;
    vn_u32 op_count;
    int rc;
    int i;
    int pak_opened;

    cfg.width = 600;
    cfg.height = 800;
    cfg.flags = VN_RENDERER_FLAG_SIMD;

    state.frame_index = 0u;
    state.clear_color = 200u;
    state.scene_id = VN_SCENE_S0;
    state.resource_count = 0u;
    state.text_id = 0u;
    state.text_speed_ms = 0u;
    state.vm_waiting = 0u;
    state.vm_ended = 0u;

    pack_path = "assets/demo/demo.vnpak";
    scene_name = "S0";

    pak.path = (const char*)0;
    pak.version = 0u;
    pak.resource_count = 0u;
    pak.entries = (ResourceEntry*)0;
    pak_opened = VN_FALSE;

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
        }
    }

    rc = parse_scene_id(scene_name, &state.scene_id);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "invalid scene: %s\n", scene_name);
        return 2;
    }

    state.clear_color = (vn_u32)(200u + (state.scene_id * 12u));

    rc = vnpak_open(&pak, pack_path);
    if (rc == VN_OK) {
        pak_opened = VN_TRUE;
        state.resource_count = pak.resource_count;
        rc = run_vm_from_pack(&pak, state.scene_id, &state);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "warning: vm from pack failed rc=%d scene=%s\n", rc, scene_name);
        }
    } else {
        (void)fprintf(stderr, "warning: vnpak_open failed rc=%d path=%s\n", rc, pack_path);
    }

    rc = renderer_init(&cfg);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "renderer_init failed rc=%d\n", rc);
        if (pak_opened == VN_TRUE) {
            vnpak_close(&pak);
        }
        return 1;
    }

    op_count = 16u;
    rc = build_render_ops(&state, ops, &op_count);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "build_render_ops failed rc=%d\n", rc);
        renderer_shutdown();
        if (pak_opened == VN_TRUE) {
            vnpak_close(&pak);
        }
        return 1;
    }

    renderer_begin_frame();
    renderer_submit(ops, op_count);
    renderer_end_frame();

    (void)printf("vn_player ok backend=%s resolution=%ux%u scene=%s resources=%u text=%u wait=%u end=%u ops=%u\n",
                 renderer_backend_name(),
                 (unsigned int)cfg.width,
                 (unsigned int)cfg.height,
                 scene_name,
                 (unsigned int)state.resource_count,
                 (unsigned int)state.text_id,
                 (unsigned int)state.vm_waiting,
                 (unsigned int)state.vm_ended,
                 (unsigned int)op_count);

    renderer_shutdown();
    if (pak_opened == VN_TRUE) {
        vnpak_close(&pak);
    }
    return 0;
}
