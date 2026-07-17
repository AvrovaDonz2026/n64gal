#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vn_frontend.h"
#include "vn_error.h"
#include "vn_pack.h"
#include "vn_runtime.h"
#include "vn_save.h"
#include "vn_vm.h"
#include "../../src/core/scene_catalog.h"

static int expect_same_resume_result(const VNRunResult* expected,
                                     const VNRunResult* actual) {
    if (expected == (const VNRunResult*)0 || actual == (const VNRunResult*)0) {
        return 1;
    }
    if (expected->frames_executed != actual->frames_executed ||
        expected->text_id != actual->text_id ||
        expected->vm_waiting != actual->vm_waiting ||
        expected->vm_ended != actual->vm_ended ||
        expected->vm_error != actual->vm_error ||
        expected->fade_alpha != actual->fade_alpha ||
        expected->fade_remain_ms != actual->fade_remain_ms ||
        expected->bgm_id != actual->bgm_id ||
        expected->se_id != actual->se_id ||
        expected->choice_count != actual->choice_count ||
        expected->choice_selected_index != actual->choice_selected_index ||
        expected->choice_text_id != actual->choice_text_id ||
        expected->op_count != actual->op_count ||
        expected->perf_flags_effective != actual->perf_flags_effective ||
        expected->render_width != actual->render_width ||
        expected->render_height != actual->render_height ||
        expected->dynamic_resolution_tier != actual->dynamic_resolution_tier ||
        expected->dynamic_resolution_switches != actual->dynamic_resolution_switches) {
        return 1;
    }
    if (expected->backend_name == (const char*)0 ||
        actual->backend_name == (const char*)0 ||
        strcmp(expected->backend_name, actual->backend_name) != 0) {
        return 1;
    }
    return 0;
}

#define TEST_V2_PAYLOAD_EXTENSION_SIZE \
    (VN_RUNTIME_SCENE_NAME_MAX + 4u + \
     2u + 2u + 2u + 4u + 4u + \
     (VN_RUNTIME_SNAPSHOT_VISUAL_LAYER_MAX * 8u) + \
     4u + 2u + 2u + 2u + 4u + 1u)
#define TEST_V1_PAYLOAD_AFTER_VM_FLAGS_SIZE 15u

static vn_u32 test_u32_le_read(const vn_u8* p) {
    return (vn_u32)(((vn_u32)p[0]) |
                    ((vn_u32)p[1] << 8) |
                    ((vn_u32)p[2] << 16) |
                    ((vn_u32)p[3] << 24));
}

static void test_u32_le_write(vn_u8* p, vn_u32 value) {
    p[0] = (vn_u8)(value & 0xFFu);
    p[1] = (vn_u8)((value >> 8) & 0xFFu);
    p[2] = (vn_u8)((value >> 16) & 0xFFu);
    p[3] = (vn_u8)((value >> 24) & 0xFFu);
}

static vn_u32 test_crc32(const vn_u8* data, vn_u32 size) {
    vn_u32 crc;
    vn_u32 i;

    crc = 0xFFFFFFFFu;
    for (i = 0u; i < size; ++i) {
        vn_u32 j;
        crc ^= (vn_u32)data[i];
        for (j = 0u; j < 8u; ++j) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static int make_v1_runtime_save(const char* v2_path, const char* v1_path) {
    FILE* fp;
    vn_u8* file_data;
    long file_size_long;
    vn_u32 file_size;
    vn_u32 payload_size_v2;
    vn_u32 payload_size_v1;
    vn_u32 crc;
    int result;

    fp = (FILE*)0;
    file_data = (vn_u8*)0;
    result = 1;

    fp = fopen(v2_path, "rb");
    if (fp == (FILE*)0 || fseek(fp, 0L, SEEK_END) != 0) {
        goto cleanup;
    }
    file_size_long = ftell(fp);
    if (file_size_long <= (long)VNSAVE_HEADER_SIZE_V1 ||
        (unsigned long)file_size_long > 0xFFFFFFFFUL ||
        fseek(fp, 0L, SEEK_SET) != 0) {
        goto cleanup;
    }
    file_size = (vn_u32)file_size_long;
    file_data = (vn_u8*)malloc((size_t)file_size);
    if (file_data == (vn_u8*)0 ||
        fread(file_data, 1u, file_size, fp) != (size_t)file_size) {
        goto cleanup;
    }
    (void)fclose(fp);
    fp = (FILE*)0;

    if (file_data[0] != (vn_u8)'V' || file_data[1] != (vn_u8)'N' ||
        file_data[2] != (vn_u8)'S' || file_data[3] != (vn_u8)'V' ||
        file_data[VNSAVE_HEADER_SIZE_V1 + 0u] != (vn_u8)'V' ||
        file_data[VNSAVE_HEADER_SIZE_V1 + 1u] != (vn_u8)'N' ||
        file_data[VNSAVE_HEADER_SIZE_V1 + 2u] != (vn_u8)'R' ||
        file_data[VNSAVE_HEADER_SIZE_V1 + 3u] != (vn_u8)'S' ||
        test_u32_le_read(file_data + VNSAVE_HEADER_SIZE_V1 + 4u) != VN_RUNTIME_SNAPSHOT_V2_VERSION) {
        goto cleanup;
    }

    payload_size_v2 = test_u32_le_read(file_data + VNSAVE_HEADER_SIZE_V1 + 8u);
    if (payload_size_v2 <= TEST_V2_PAYLOAD_EXTENSION_SIZE ||
        payload_size_v2 != file_size - VNSAVE_HEADER_SIZE_V1) {
        goto cleanup;
    }
    payload_size_v1 = payload_size_v2 - TEST_V2_PAYLOAD_EXTENSION_SIZE;
    test_u32_le_write(file_data + VNSAVE_HEADER_SIZE_V1 + 4u, 1u);
    test_u32_le_write(file_data + VNSAVE_HEADER_SIZE_V1 + 8u, payload_size_v1);
    crc = test_crc32(file_data + VNSAVE_HEADER_SIZE_V1, payload_size_v1);
    test_u32_le_write(file_data + 24u, crc);

    fp = fopen(v1_path, "wb");
    if (fp == (FILE*)0) {
        goto cleanup;
    }
    if (fwrite(file_data,
               1u,
               VNSAVE_HEADER_SIZE_V1 + payload_size_v1,
               fp) != (size_t)(VNSAVE_HEADER_SIZE_V1 + payload_size_v1)) {
        (void)fclose(fp);
        fp = (FILE*)0;
        goto cleanup;
    }
    if (fclose(fp) != 0) {
        fp = (FILE*)0;
        goto cleanup;
    }
    fp = (FILE*)0;
    result = 0;

cleanup:
    if (fp != (FILE*)0) {
        (void)fclose(fp);
    }
    if (file_data != (vn_u8*)0) {
        free(file_data);
    }
    if (result != 0) {
        (void)remove(v1_path);
    }
    return result;
}

static int make_vm_flags_runtime_save(const char* source_path,
                                      const char* output_path,
                                      vn_u32 vm_flags) {
    FILE* fp;
    vn_u8* file_data;
    long file_size_long;
    vn_u32 file_size;
    vn_u32 payload_size;
    vn_u32 payload_version;
    vn_u32 base_payload_size;
    vn_u32 vm_flags_offset;
    vn_u32 crc;
    int result;

    fp = (FILE*)0;
    file_data = (vn_u8*)0;
    result = 1;
    (void)remove(output_path);

    fp = fopen(source_path, "rb");
    if (fp == (FILE*)0 || fseek(fp, 0L, SEEK_END) != 0) {
        goto cleanup;
    }
    file_size_long = ftell(fp);
    if (file_size_long <= (long)VNSAVE_HEADER_SIZE_V1 ||
        (unsigned long)file_size_long > 0xFFFFFFFFUL ||
        fseek(fp, 0L, SEEK_SET) != 0) {
        goto cleanup;
    }
    file_size = (vn_u32)file_size_long;
    file_data = (vn_u8*)malloc((size_t)file_size);
    if (file_data == (vn_u8*)0 ||
        fread(file_data, 1u, file_size, fp) != (size_t)file_size) {
        goto cleanup;
    }
    (void)fclose(fp);
    fp = (FILE*)0;

    if (file_data[0] != (vn_u8)'V' || file_data[1] != (vn_u8)'N' ||
        file_data[2] != (vn_u8)'S' || file_data[3] != (vn_u8)'V' ||
        file_data[VNSAVE_HEADER_SIZE_V1 + 0u] != (vn_u8)'V' ||
        file_data[VNSAVE_HEADER_SIZE_V1 + 1u] != (vn_u8)'N' ||
        file_data[VNSAVE_HEADER_SIZE_V1 + 2u] != (vn_u8)'R' ||
        file_data[VNSAVE_HEADER_SIZE_V1 + 3u] != (vn_u8)'S') {
        goto cleanup;
    }
    payload_version = test_u32_le_read(file_data + VNSAVE_HEADER_SIZE_V1 + 4u);
    payload_size = test_u32_le_read(file_data + VNSAVE_HEADER_SIZE_V1 + 8u);
    if (payload_size != file_size - VNSAVE_HEADER_SIZE_V1) {
        goto cleanup;
    }
    if (payload_version == 1u) {
        base_payload_size = payload_size;
    } else if (payload_version == VN_RUNTIME_SNAPSHOT_V2_VERSION &&
               payload_size > TEST_V2_PAYLOAD_EXTENSION_SIZE) {
        base_payload_size = payload_size - TEST_V2_PAYLOAD_EXTENSION_SIZE;
    } else {
        goto cleanup;
    }
    if (base_payload_size < TEST_V1_PAYLOAD_AFTER_VM_FLAGS_SIZE + 4u) {
        goto cleanup;
    }
    vm_flags_offset = VNSAVE_HEADER_SIZE_V1 +
                      base_payload_size -
                      TEST_V1_PAYLOAD_AFTER_VM_FLAGS_SIZE - 4u;
    test_u32_le_write(file_data + vm_flags_offset, vm_flags);
    crc = test_crc32(file_data + VNSAVE_HEADER_SIZE_V1, payload_size);
    test_u32_le_write(file_data + 24u, crc);

    fp = fopen(output_path, "wb");
    if (fp == (FILE*)0) {
        goto cleanup;
    }
    if (fwrite(file_data, 1u, file_size, fp) != (size_t)file_size) {
        (void)fclose(fp);
        fp = (FILE*)0;
        goto cleanup;
    }
    if (fclose(fp) != 0) {
        fp = (FILE*)0;
        goto cleanup;
    }
    fp = (FILE*)0;
    result = 0;

cleanup:
    if (fp != (FILE*)0) {
        (void)fclose(fp);
    }
    if (file_data != (vn_u8*)0) {
        free(file_data);
    }
    if (result != 0) {
        (void)remove(output_path);
    }
    return result;
}

static int assert_gallery_script_terminal_tail(void) {
    VNPak pak;
    VNSceneCatalog catalog;
    const VNSceneCatalogEntry* gallery;
    const ResourceEntry* resource;
    vn_u8* script;
    vn_u32 read_size;
    int pak_opened;
    int rc;
    int result;

    (void)memset(&pak, 0, sizeof(pak));
    vn_scene_catalog_init(&catalog);
    script = (vn_u8*)0;
    pak_opened = VN_FALSE;
    rc = vnpak_open(&pak, "assets/demo/content-demo.vnpak");
    result = 1;
    if (rc != VN_OK) {
        goto cleanup;
    }
    pak_opened = VN_TRUE;
    rc = vn_scene_catalog_load(&catalog, &pak);
    gallery = vn_scene_catalog_find_name(&catalog, "Gallery");
    if (rc != VN_OK || gallery == (const VNSceneCatalogEntry*)0) {
        goto cleanup;
    }
    resource = vnpak_get(&pak, (vn_u32)gallery->script_resource_id);
    if (resource == (const ResourceEntry*)0 ||
        resource->type != VN_RESOURCE_TYPE_SCRIPT ||
        resource->data_size < 2u) {
        goto cleanup;
    }
    script = (vn_u8*)malloc((size_t)resource->data_size);
    if (script == (vn_u8*)0) {
        goto cleanup;
    }
    read_size = 0u;
    rc = vnpak_read_resource(&pak,
                             (vn_u32)gallery->script_resource_id,
                             script,
                             resource->data_size,
                             &read_size);
    if (rc != VN_OK || read_size != resource->data_size ||
        script[read_size - 2u] != VN_VM_OP_END ||
        script[read_size - 1u] != VN_VM_OP_RETURN) {
        goto cleanup;
    }
    result = 0;

cleanup:
    if (script != (vn_u8*)0) {
        free(script);
    }
    if (pak_opened != VN_FALSE) {
        vnpak_close(&pak);
    }
    if (result != 0) {
        (void)fprintf(stderr, "Gallery script must end with END,RETURN rc=%d\n", rc);
    }
    return result;
}

static int test_snapshot_v2_mid_fade(void) {
    VNRunConfig cfg;
    VNRunResult original_result;
    VNRunResult restored_result;
    VNRuntimeSession* original;
    VNRuntimeSession* restored;
    VNRuntimeSessionSnapshot legacy_snapshot;
    VNRuntimeSessionSnapshotV2 snapshot;
    VNRuntimeSessionSnapshotV2 invalid_snapshot;
    int rc;
    int result;

    original = (VNRuntimeSession*)0;
    restored = (VNRuntimeSession*)0;
    result = 1;
    (void)memset(&snapshot, 0, sizeof(snapshot));
    (void)memset(&invalid_snapshot, 0, sizeof(invalid_snapshot));
    (void)memset(&legacy_snapshot, 0, sizeof(legacy_snapshot));
    (void)memset(&original_result, 0, sizeof(original_result));
    (void)memset(&restored_result, 0, sizeof(restored_result));

    vn_run_config_init(&cfg);
    cfg.scene_name = "S3";
    cfg.frames = 12u;
    cfg.dt_ms = 16u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;
    rc = vn_runtime_session_create(&cfg, &original);
    if (rc != VN_OK || original == (VNRuntimeSession*)0) {
        goto cleanup;
    }
    rc = vn_runtime_session_step(original, &original_result);
    if (rc != VN_OK) {
        goto cleanup;
    }

    if (vn_runtime_session_capture_snapshot_v2(original, &invalid_snapshot) != VN_E_INVALID_ARG) {
        goto cleanup;
    }
    vn_runtime_session_snapshot_v2_init(&snapshot);
    rc = vn_runtime_session_capture_snapshot_v2(original, &snapshot);
    if (rc != VN_OK || snapshot.struct_size != (vn_u32)sizeof(snapshot) ||
        snapshot.version != VN_RUNTIME_SNAPSHOT_V2_VERSION ||
        strcmp(snapshot.scene_name, "S3") != 0 || snapshot.content_mode != 0u ||
        snapshot.v1.fade_active == 0u || snapshot.v1.fade_elapsed_ms == 0u ||
        snapshot.v1.fade_elapsed_ms >= (vn_u32)snapshot.v1.fade_duration_ms) {
        goto cleanup;
    }

    invalid_snapshot = snapshot;
    invalid_snapshot.version += 1u;
    if (vn_runtime_session_create_from_snapshot_v2(&invalid_snapshot, &restored) != VN_E_INVALID_ARG) {
        goto cleanup;
    }
    invalid_snapshot = snapshot;
    invalid_snapshot.struct_size -= 1u;
    if (vn_runtime_session_create_from_snapshot_v2(&invalid_snapshot, &restored) != VN_E_INVALID_ARG) {
        goto cleanup;
    }
    invalid_snapshot = snapshot;
    invalid_snapshot.v1.fade_active = 1u;
    invalid_snapshot.v1.fade_duration_ms = 0u;
    invalid_snapshot.v1.fade_elapsed_ms = 1u;
    invalid_snapshot.v1.fade_seen_serial = invalid_snapshot.v1.vm_fade_serial;
    if (vn_runtime_session_create_from_snapshot_v2(&invalid_snapshot, &restored) != VN_E_INVALID_ARG ||
        restored != (VNRuntimeSession*)0) {
        goto cleanup;
    }
    rc = vn_runtime_session_capture_snapshot(original, &legacy_snapshot);
    if (rc != VN_OK) {
        goto cleanup;
    }
    legacy_snapshot.fade_active = 1u;
    legacy_snapshot.fade_duration_ms = 0u;
    legacy_snapshot.fade_elapsed_ms = 1u;
    legacy_snapshot.fade_seen_serial = legacy_snapshot.vm_fade_serial;
    if (vn_runtime_session_create_from_snapshot(&legacy_snapshot, &restored) != VN_E_INVALID_ARG ||
        restored != (VNRuntimeSession*)0) {
        goto cleanup;
    }

    rc = vn_runtime_session_step(original, &original_result);
    if (rc != VN_OK) {
        goto cleanup;
    }
    (void)vn_runtime_session_destroy(original);
    original = (VNRuntimeSession*)0;
    rc = vn_runtime_session_create_from_snapshot_v2(&snapshot, &restored);
    if (rc != VN_OK || restored == (VNRuntimeSession*)0) {
        goto cleanup;
    }
    rc = vn_runtime_session_step(restored, &restored_result);
    if (rc != VN_OK || expect_same_resume_result(&original_result, &restored_result) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
    if (restored != (VNRuntimeSession*)0) {
        (void)vn_runtime_session_destroy(restored);
    }
    if (original != (VNRuntimeSession*)0) {
        (void)vn_runtime_session_destroy(original);
    }
    if (result != 0) {
        (void)fprintf(stderr, "snapshot v2 mid-fade compatibility failed rc=%d\n", rc);
    }
    return result;
}

static int test_v1_runtime_file_compatibility(void) {
    const char* v2_path;
    const char* v1_path;
    VNRunConfig cfg;
    VNRunResult original_result;
    VNRunResult restored_result;
    VNRuntimeSession* original;
    VNRuntimeSession* restored;
    int rc;
    int result;

    v2_path = "test_runtime_session_payload_v2.vnsave";
    v1_path = "test_runtime_session_payload_v1.vnsave";
    original = (VNRuntimeSession*)0;
    restored = (VNRuntimeSession*)0;
    result = 1;
    rc = VN_OK;
    (void)remove(v2_path);
    (void)remove(v1_path);
    (void)memset(&original_result, 0, sizeof(original_result));
    (void)memset(&restored_result, 0, sizeof(restored_result));

    vn_run_config_init(&cfg);
    cfg.scene_name = "S3";
    cfg.frames = 12u;
    cfg.dt_ms = 16u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;
    rc = vn_runtime_session_create(&cfg, &original);
    if (rc != VN_OK || original == (VNRuntimeSession*)0) {
        goto cleanup;
    }
    rc = vn_runtime_session_step(original, &original_result);
    if (rc != VN_OK || vn_runtime_session_save_to_file(original, v2_path, 3u, 77u) != VN_OK ||
        make_v1_runtime_save(v2_path, v1_path) != 0) {
        goto cleanup;
    }
    rc = vn_runtime_session_step(original, &original_result);
    if (rc != VN_OK) {
        goto cleanup;
    }
    (void)vn_runtime_session_destroy(original);
    original = (VNRuntimeSession*)0;
    rc = vn_runtime_session_load_from_file(v1_path, &restored);
    if (rc != VN_OK || restored == (VNRuntimeSession*)0) {
        goto cleanup;
    }
    rc = vn_runtime_session_step(restored, &restored_result);
    if (rc != VN_OK || expect_same_resume_result(&original_result, &restored_result) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
    if (restored != (VNRuntimeSession*)0) {
        (void)vn_runtime_session_destroy(restored);
    }
    if (original != (VNRuntimeSession*)0) {
        (void)vn_runtime_session_destroy(original);
    }
    (void)remove(v2_path);
    (void)remove(v1_path);
    if (result != 0) {
        (void)fprintf(stderr, "runtime payload v1 compatibility failed rc=%d\n", rc);
    }
    return result;
}

static int test_frozen_v1_runtime_fixture(void) {
    VNRuntimeSession* session;
    VNRunResult result;
    int rc;

    session = (VNRuntimeSession*)0;
    (void)memset(&result, 0, sizeof(result));
    rc = vn_runtime_session_load_from_file(
        "tests/fixtures/vnsave/v1/runtime-v1.0.0-s3.vnsave",
        &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "frozen v1.0 runtime fixture load failed rc=%d\n", rc);
        return 1;
    }
    rc = vn_runtime_session_step(session, &result);
    if (rc != VN_OK || result.frames_executed != 2u || result.text_id != 160u ||
        result.vm_waiting == 0u || result.fade_alpha == 0u || result.fade_alpha >= 220u ||
        result.backend_name == (const char*)0 || strcmp(result.backend_name, "scalar") != 0) {
        (void)fprintf(stderr,
                      "frozen v1.0 runtime fixture resume mismatch rc=%d frames=%u text=%u fade=%u\n",
                      rc,
                      (unsigned int)result.frames_executed,
                      (unsigned int)result.text_id,
                      (unsigned int)result.fade_alpha);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    (void)vn_runtime_session_destroy(session);
    return 0;
}

static int test_terminal_snapshot_restore_rejected(void) {
    static const vn_u32 crafted_flags[] = {
        VN_VM_FLAG_ENDED,
        VN_VM_FLAG_ERROR,
        0x80000000u
    };
    static const int expected_rc[] = {
        VN_E_UNSUPPORTED,
        VN_E_UNSUPPORTED,
        VN_E_INVALID_ARG
    };
    const char* source_path;
    const char* crafted_path;
    VNRunConfig cfg;
    VNRunResult run_result;
    VNRuntimeSession* original;
    VNRuntimeSession* restored;
    VNRuntimeSessionSnapshot snapshot_v1;
    VNRuntimeSessionSnapshot crafted_v1;
    VNRuntimeSessionSnapshotV2 snapshot_v2;
    VNRuntimeSessionSnapshotV2 crafted_v2;
    vn_u32 i;
    int rc;
    int result;

    source_path = "test_runtime_session_flags_source.vnsave";
    crafted_path = "test_runtime_session_flags_crafted.vnsave";
    original = (VNRuntimeSession*)0;
    restored = (VNRuntimeSession*)0;
    rc = VN_OK;
    result = 1;
    (void)remove(source_path);
    (void)remove(crafted_path);
    (void)memset(&run_result, 0, sizeof(run_result));
    (void)memset(&snapshot_v1, 0, sizeof(snapshot_v1));
    (void)memset(&crafted_v1, 0, sizeof(crafted_v1));
    (void)memset(&snapshot_v2, 0, sizeof(snapshot_v2));
    (void)memset(&crafted_v2, 0, sizeof(crafted_v2));

    vn_run_config_init(&cfg);
    cfg.scene_name = "S3";
    cfg.frames = 12u;
    cfg.dt_ms = 16u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;
    rc = vn_runtime_session_create(&cfg, &original);
    if (rc != VN_OK || original == (VNRuntimeSession*)0) {
        goto cleanup;
    }
    rc = vn_runtime_session_step(original, &run_result);
    if (rc != VN_OK || vn_runtime_session_capture_snapshot(original, &snapshot_v1) != VN_OK) {
        goto cleanup;
    }
    vn_runtime_session_snapshot_v2_init(&snapshot_v2);
    if (vn_runtime_session_capture_snapshot_v2(original, &snapshot_v2) != VN_OK ||
        vn_runtime_session_save_to_file(original, source_path, 13u, 789u) != VN_OK) {
        goto cleanup;
    }
    (void)vn_runtime_session_destroy(original);
    original = (VNRuntimeSession*)0;

    for (i = 0u; i < (vn_u32)(sizeof(crafted_flags) / sizeof(crafted_flags[0])); ++i) {
        crafted_v1 = snapshot_v1;
        crafted_v1.vm_flags = crafted_flags[i];
        restored = (VNRuntimeSession*)0;
        rc = vn_runtime_session_create_from_snapshot(&crafted_v1, &restored);
        if (rc != expected_rc[i] || restored != (VNRuntimeSession*)0) {
            goto cleanup;
        }

        crafted_v2 = snapshot_v2;
        crafted_v2.v1.vm_flags = crafted_flags[i];
        restored = (VNRuntimeSession*)0;
        rc = vn_runtime_session_create_from_snapshot_v2(&crafted_v2, &restored);
        if (rc != expected_rc[i] || restored != (VNRuntimeSession*)0) {
            goto cleanup;
        }

        if (make_vm_flags_runtime_save(source_path,
                                       crafted_path,
                                       crafted_flags[i]) != 0) {
            goto cleanup;
        }
        restored = (VNRuntimeSession*)0;
        rc = vn_runtime_session_load_from_file(crafted_path, &restored);
        if (rc != expected_rc[i] || restored != (VNRuntimeSession*)0) {
            goto cleanup;
        }
        (void)remove(crafted_path);
    }
    crafted_v1 = snapshot_v1;
    crafted_v1.vm_pc_offset = 1u;
    rc = vn_runtime_session_create_from_snapshot(&crafted_v1, &restored);
    if (rc != VN_E_FORMAT || restored != (VNRuntimeSession*)0) {
        goto cleanup;
    }
    crafted_v2 = snapshot_v2;
    crafted_v2.v1.vm_pc_offset = 1u;
    rc = vn_runtime_session_create_from_snapshot_v2(&crafted_v2, &restored);
    if (rc != VN_E_FORMAT || restored != (VNRuntimeSession*)0) {
        goto cleanup;
    }
    crafted_v1 = snapshot_v1;
    crafted_v1.vm_call_sp = 1u;
    crafted_v1.vm_call_stack[0] = 1u;
    rc = vn_runtime_session_create_from_snapshot(&crafted_v1, &restored);
    if (rc != VN_E_FORMAT || restored != (VNRuntimeSession*)0) {
        goto cleanup;
    }
    crafted_v2 = snapshot_v2;
    crafted_v2.v1.vm_call_sp = 1u;
    crafted_v2.v1.vm_call_stack[0] = 1u;
    rc = vn_runtime_session_create_from_snapshot_v2(&crafted_v2, &restored);
    if (rc != VN_E_FORMAT || restored != (VNRuntimeSession*)0) {
        goto cleanup;
    }
    result = 0;

cleanup:
    if (restored != (VNRuntimeSession*)0) {
        (void)vn_runtime_session_destroy(restored);
    }
    if (original != (VNRuntimeSession*)0) {
        (void)vn_runtime_session_destroy(original);
    }
    (void)remove(source_path);
    (void)remove(crafted_path);
    if (result != 0) {
        (void)fprintf(stderr, "terminal snapshot restore validation failed rc=%d\n", rc);
    }
    return result;
}

static int test_terminal_content_snapshot_rejected(void) {
    const char* save_path;
    VNRunConfig cfg;
    VNRunResult run_result;
    VNRuntimeSession* session;
    VNRuntimeSessionSnapshotV2 snapshot;
    vn_u32 guard;
    int capture_rc;
    int rc;
    int save_rc;

    save_path = "test_runtime_session_terminal.vnsave";
    session = (VNRuntimeSession*)0;
    guard = 0u;
    capture_rc = VN_OK;
    save_rc = VN_OK;
    (void)remove(save_path);
    (void)memset(&run_result, 0, sizeof(run_result));
    (void)memset(&snapshot, 0, sizeof(snapshot));
    if (assert_gallery_script_terminal_tail() != 0) {
        return 1;
    }
    vn_run_config_init(&cfg);
    cfg.pack_path = "assets/demo/content-demo.vnpak";
    cfg.scene_name = "Gallery";
    cfg.backend_name = "scalar";
    cfg.width = 64u;
    cfg.height = 48u;
    cfg.frames = 64u;
    cfg.dt_ms = 40u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "terminal snapshot session create failed rc=%d\n", rc);
        return 1;
    }
    while (vn_runtime_session_is_done(session) == VN_FALSE && guard < 64u) {
        rc = vn_runtime_session_step(session, &run_result);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "terminal snapshot session step failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
        guard += 1u;
    }
    vn_runtime_session_snapshot_v2_init(&snapshot);
    capture_rc = vn_runtime_session_capture_snapshot_v2(session, &snapshot);
    save_rc = vn_runtime_session_save_to_file(session, save_path, 14u, 987u);
    (void)vn_runtime_session_destroy(session);
    (void)remove(save_path);
    if (guard == 64u || run_result.vm_ended == 0u ||
        capture_rc != VN_E_UNSUPPORTED || save_rc != VN_E_UNSUPPORTED) {
        (void)fprintf(stderr,
                      "terminal snapshot should be unsupported capture_rc=%d save_rc=%d guard=%u ended=%u\n",
                      capture_rc,
                      save_rc,
                      (unsigned int)guard,
                      (unsigned int)run_result.vm_ended);
        return 1;
    }
    return 0;
}

static int test_content_snapshot_and_frame_view(void) {
    const char* save_path;
    VNRunConfig cfg;
    VNRunResult reference_result;
    VNRunResult restored_result;
    VNRuntimeSession* original;
    VNRuntimeSession* restored;
    VNRuntimeSession* file_restored;
    VNRuntimeSessionSnapshot legacy_snapshot;
    VNRuntimeSessionSnapshotV2 snapshot;
    VNRuntimeSessionSnapshotV2 invalid_snapshot;
    VNRuntimeFrameView frame;
    VNSaveProbe probe;
    vn_u32 reference_crc;
    vn_u32 restored_crc;
    vn_u32 i;
    int rc;
    int result;

    save_path = "test_runtime_session_content.vnsave";
    original = (VNRuntimeSession*)0;
    restored = (VNRuntimeSession*)0;
    file_restored = (VNRuntimeSession*)0;
    reference_crc = 0u;
    restored_crc = 0u;
    rc = VN_OK;
    result = 1;
    (void)remove(save_path);
    (void)memset(&reference_result, 0, sizeof(reference_result));
    (void)memset(&restored_result, 0, sizeof(restored_result));
    (void)memset(&legacy_snapshot, 0, sizeof(legacy_snapshot));
    (void)memset(&snapshot, 0, sizeof(snapshot));
    (void)memset(&invalid_snapshot, 0, sizeof(invalid_snapshot));
    (void)memset(&frame, 0, sizeof(frame));
    (void)memset(&probe, 0, sizeof(probe));

    vn_run_config_init(&cfg);
    cfg.pack_path = "assets/demo/content-demo.vnpak";
    cfg.backend_name = "scalar";
    cfg.width = 64u;
    cfg.height = 48u;
    cfg.frames = 20u;
    cfg.dt_ms = 40u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;
    cfg.perf_flags = 0u;
    rc = vn_runtime_session_create(&cfg, &original);
    if (rc != VN_OK || original == (VNRuntimeSession*)0) {
        goto cleanup;
    }
    if (vn_runtime_session_get_frame_view(original, &frame) != VN_E_INVALID_ARG) {
        goto cleanup;
    }
    vn_runtime_frame_view_init(&frame);
    if (vn_runtime_session_get_frame_view(original, &frame) != VN_E_RENDER_STATE ||
        frame.pixels != (const vn_u32*)0 ||
        frame.struct_size != (vn_u32)sizeof(frame) ||
        frame.version != VN_RUNTIME_FRAME_VIEW_VERSION ||
        vn_runtime_session_get_frame_view((const VNRuntimeSession*)0, &frame) != VN_E_INVALID_ARG ||
        vn_runtime_session_get_frame_view(original, (VNRuntimeFrameView*)0) != VN_E_INVALID_ARG) {
        goto cleanup;
    }
    for (i = 0u; i < 4u; ++i) {
        rc = vn_runtime_session_step(original, &reference_result);
        if (rc != VN_OK) {
            goto cleanup;
        }
    }
    rc = vn_runtime_session_get_frame_view(original, &frame);
    if (rc != VN_OK || frame.pixels == (const vn_u32*)0 ||
        frame.width != 64u || frame.height != 48u ||
        frame.stride_pixels != 64u || frame.pixel_count != 3072u ||
        frame.pixel_format != VN_RUNTIME_PIXEL_FORMAT_ARGB8888_U32) {
        goto cleanup;
    }
    if (vn_runtime_session_capture_snapshot(original, &legacy_snapshot) != VN_E_UNSUPPORTED) {
        goto cleanup;
    }
    vn_runtime_session_snapshot_v2_init(&snapshot);
    rc = vn_runtime_session_capture_snapshot_v2(original, &snapshot);
    if (rc != VN_OK || strcmp(snapshot.scene_name, "Opening") != 0 ||
        snapshot.content_mode != 1u || snapshot.v1.scene_id != 0x015825B7u ||
        snapshot.vm_previous_background_texture_id != 2u ||
        snapshot.vm_background_texture_id != 3u ||
        snapshot.background_previous_texture_id != 2u ||
        snapshot.background_texture_id != 3u ||
        snapshot.background_active != 1u ||
        snapshot.background_elapsed_ms == 0u ||
        snapshot.background_elapsed_ms >= (vn_u32)snapshot.background_duration_ms ||
        snapshot.visual_layers[0].active != 1u || snapshot.visual_layers[0].texture_id != 3u ||
        snapshot.visual_layers[1].active != 1u || snapshot.visual_layers[1].texture_id != 4u) {
        goto cleanup;
    }
    invalid_snapshot = snapshot;
    invalid_snapshot.version += 1u;
    restored = (VNRuntimeSession*)1;
    rc = vn_runtime_session_create_from_snapshot_v2(&invalid_snapshot, &restored);
    if (rc != VN_E_INVALID_ARG || restored != (VNRuntimeSession*)0) {
        restored = (VNRuntimeSession*)0;
        goto cleanup;
    }
    invalid_snapshot = snapshot;
    invalid_snapshot.background_active = 1u;
    invalid_snapshot.background_duration_ms = 0u;
    invalid_snapshot.background_elapsed_ms = 0u;
    restored = (VNRuntimeSession*)0;
    rc = vn_runtime_session_create_from_snapshot_v2(&invalid_snapshot, &restored);
    if (rc != VN_E_FORMAT || restored != (VNRuntimeSession*)0) {
        goto cleanup;
    }
    invalid_snapshot = snapshot;
    invalid_snapshot.visual_layers[0].active = 1u;
    invalid_snapshot.visual_layers[0].texture_id = 0xFFFFu;
    restored = (VNRuntimeSession*)0;
    rc = vn_runtime_session_create_from_snapshot_v2(&invalid_snapshot, &restored);
    if (rc != VN_E_FORMAT || restored != (VNRuntimeSession*)0) {
        goto cleanup;
    }

    rc = vn_runtime_session_save_to_file(original, save_path, 11u, 456u);
    if (rc != VN_OK) {
        goto cleanup;
    }
    rc = vnsave_probe_file(save_path, &probe);
    if (rc != VN_OK || probe.version != VNSAVE_VERSION_1 ||
        probe.slot_id != 11u || probe.timestamp_s != 456u ||
        probe.scene_id != snapshot.v1.scene_id ||
        probe.script_pc != snapshot.v1.vm_pc_offset) {
        goto cleanup;
    }

    rc = vn_runtime_session_step(original, &reference_result);
    if (rc != VN_OK || vn_runtime_session_get_frame_view(original, &frame) != VN_OK) {
        goto cleanup;
    }
    reference_crc = test_crc32((const vn_u8*)frame.pixels,
                               frame.pixel_count * (vn_u32)sizeof(vn_u32));
    (void)vn_runtime_session_destroy(original);
    original = (VNRuntimeSession*)0;

    rc = vn_runtime_session_create_from_snapshot_v2(&snapshot, &restored);
    if (rc != VN_OK || restored == (VNRuntimeSession*)0 ||
        vn_runtime_session_get_frame_view(restored, &frame) != VN_E_RENDER_STATE) {
        goto cleanup;
    }
    rc = vn_runtime_session_step(restored, &restored_result);
    if (rc != VN_OK || expect_same_resume_result(&reference_result, &restored_result) != 0 ||
        vn_runtime_session_get_frame_view(restored, &frame) != VN_OK) {
        goto cleanup;
    }
    restored_crc = test_crc32((const vn_u8*)frame.pixels,
                              frame.pixel_count * (vn_u32)sizeof(vn_u32));
    if (restored_crc != reference_crc) {
        goto cleanup;
    }
    (void)vn_runtime_session_destroy(restored);
    restored = (VNRuntimeSession*)0;

    rc = vn_runtime_session_load_from_file(save_path, &file_restored);
    if (rc != VN_OK || file_restored == (VNRuntimeSession*)0 ||
        vn_runtime_session_get_frame_view(file_restored, &frame) != VN_E_RENDER_STATE) {
        goto cleanup;
    }
    rc = vn_runtime_session_step(file_restored, &restored_result);
    if (rc != VN_OK || expect_same_resume_result(&reference_result, &restored_result) != 0 ||
        vn_runtime_session_get_frame_view(file_restored, &frame) != VN_OK) {
        goto cleanup;
    }
    restored_crc = test_crc32((const vn_u8*)frame.pixels,
                              frame.pixel_count * (vn_u32)sizeof(vn_u32));
    if (restored_crc != reference_crc) {
        goto cleanup;
    }
    result = 0;

cleanup:
    if (file_restored != (VNRuntimeSession*)0) {
        (void)vn_runtime_session_destroy(file_restored);
    }
    if (restored != (VNRuntimeSession*)0) {
        (void)vn_runtime_session_destroy(restored);
    }
    if (original != (VNRuntimeSession*)0) {
        (void)vn_runtime_session_destroy(original);
    }
    (void)remove(save_path);
    if (result != 0) {
        (void)fprintf(stderr,
                      "content snapshot/frame view compatibility failed rc=%d reference_crc=%08x restored_crc=%08x\n",
                      rc,
                      (unsigned int)reference_crc,
                      (unsigned int)restored_crc);
    }
    return result;
}

int main(void) {
    VNRunConfig cfg;
    VNRunResult res;
    VNRunResult resumed_res;
    VNRunResult restored_res;
    VNRuntimeSession* session;
    VNRuntimeSession* second_session;
    VNRuntimeSession* restored_session;
    VNRuntimeSession* file_session;
    VNRuntimeSessionSnapshot snapshot;
    VNInputEvent input;
    VNSaveProbe probe;
    int rc;
    vn_u32 guard;
    vn_u32 i;
    const char* save_path;

    session = (VNRuntimeSession*)0;
    second_session = (VNRuntimeSession*)0;
    restored_session = (VNRuntimeSession*)0;
    file_session = (VNRuntimeSession*)0;
    memset((void*)&res, 0, sizeof(res));
    memset((void*)&resumed_res, 0, sizeof(resumed_res));
    memset((void*)&restored_res, 0, sizeof(restored_res));
    memset((void*)&snapshot, 0, sizeof(snapshot));
    memset((void*)&input, 0, sizeof(input));
    memset((void*)&probe, 0, sizeof(probe));
    save_path = "test_runtime_session_snapshot.vnsave";
    (void)remove(save_path);
    if (test_snapshot_v2_mid_fade() != 0 ||
        test_v1_runtime_file_compatibility() != 0 ||
        test_frozen_v1_runtime_fixture() != 0 ||
        test_terminal_snapshot_restore_rejected() != 0 ||
        test_terminal_content_snapshot_rejected() != 0 ||
        test_content_snapshot_and_frame_view() != 0) {
        return 1;
    }
    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 8u;
    cfg.dt_ms = 16u;
    cfg.choice_index = 0u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "vn_runtime_session_create failed rc=%d\n", rc);
        return 1;
    }
    rc = vn_runtime_session_create(&cfg, &second_session);
    if (rc != VN_E_RENDER_STATE || second_session != (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "second live session should fail rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }

    rc = vn_runtime_session_set_choice(session, 1u);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vn_runtime_session_set_choice failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }

    guard = 0u;
    while (vn_runtime_session_is_done(session) == VN_FALSE) {
        rc = vn_runtime_session_step(session, &res);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "vn_runtime_session_step failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
        guard += 1u;
        if (guard > 64u) {
            (void)fprintf(stderr, "runtime session did not finish\n");
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
    }

    if (res.frames_executed == 0u) {
        (void)fprintf(stderr, "no frames executed\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (res.choice_selected_index != 1u) {
        (void)fprintf(stderr, "choice index mismatch got=%u\n", (unsigned int)res.choice_selected_index);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (res.backend_name == (const char*)0) {
        (void)fprintf(stderr, "backend name missing\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }

    rc = vn_runtime_session_destroy(session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "vn_runtime_session_destroy failed rc=%d\n", rc);
        return 1;
    }
    session = (VNRuntimeSession*)0;

    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 8u;
    cfg.dt_ms = 16u;
    cfg.choice_index = 1u;
    cfg.trace = 0u;
    cfg.keyboard = 0u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "snapshot create failed rc=%d\n", rc);
        return 1;
    }
    for (i = 0u; i < 3u; ++i) {
        rc = vn_runtime_session_step(session, &res);
        if (rc != VN_OK) {
            (void)fprintf(stderr, "snapshot warmup step failed rc=%d\n", rc);
            (void)vn_runtime_session_destroy(session);
            return 1;
        }
    }
    rc = vn_runtime_session_capture_snapshot(session, &snapshot);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "capture snapshot failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    if (vn_runtime_session_capture_snapshot((const VNRuntimeSession*)0, &snapshot) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null session snapshot\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    restored_session = (VNRuntimeSession*)1;
    if (vn_runtime_session_create_from_snapshot((const VNRuntimeSessionSnapshot*)0, &restored_session) != VN_E_INVALID_ARG ||
        restored_session != (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null snapshot create\n");
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    rc = vn_runtime_session_save_to_file(session, save_path, 7u, 123u);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "save snapshot to file failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        return 1;
    }
    rc = vnsave_probe_file(save_path, &probe);
    if (rc != VN_OK ||
        probe.version != VNSAVE_VERSION_1 ||
        probe.slot_id != 7u ||
        probe.scene_id != VN_SCENE_S2 ||
        probe.script_pc != snapshot.vm_pc_offset ||
        probe.timestamp_s != 123u) {
        (void)fprintf(stderr,
                      "saved snapshot probe mismatch rc=%d version=%u slot=%u scene=%u pc=%u timestamp=%u\n",
                      rc,
                      (unsigned int)probe.version,
                      (unsigned int)probe.slot_id,
                      (unsigned int)probe.scene_id,
                      (unsigned int)probe.script_pc,
                      (unsigned int)probe.timestamp_s);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    rc = vn_runtime_session_step(session, &resumed_res);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "resume original after snapshot failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(session);
        (void)remove(save_path);
        return 1;
    }
    (void)vn_runtime_session_destroy(session);
    session = (VNRuntimeSession*)0;

    rc = vn_runtime_session_create_from_snapshot(&snapshot, &restored_session);
    if (rc != VN_OK || restored_session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "restore snapshot failed rc=%d\n", rc);
        (void)remove(save_path);
        return 1;
    }
    rc = vn_runtime_session_step(restored_session, &restored_res);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "resume restored after snapshot failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(restored_session);
        (void)remove(save_path);
        return 1;
    }
    if (expect_same_resume_result(&resumed_res, &restored_res) != 0) {
        (void)fprintf(stderr, "snapshot resume mismatch\n");
        (void)vn_runtime_session_destroy(restored_session);
        (void)remove(save_path);
        return 1;
    }
    if (vn_runtime_session_save_to_file((const VNRuntimeSession*)0, save_path, 0u, 0u) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null save_to_file\n");
        (void)vn_runtime_session_destroy(restored_session);
        (void)remove(save_path);
        return 1;
    }
    input.kind = VN_INPUT_KIND_QUIT;
    input.value0 = 0u;
    input.value1 = 0u;
    rc = vn_runtime_session_inject_input(restored_session, &input);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "inject quit before save_to_file failed rc=%d\n", rc);
        (void)vn_runtime_session_destroy(restored_session);
        (void)remove(save_path);
        return 1;
    }
    if (vn_runtime_session_save_to_file(restored_session, save_path, 0u, 0u) != VN_E_UNSUPPORTED) {
        (void)fprintf(stderr, "expected VN_E_UNSUPPORTED for pending-input save_to_file\n");
        (void)vn_runtime_session_destroy(restored_session);
        (void)remove(save_path);
        return 1;
    }
    rc = vn_runtime_session_destroy(restored_session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "restored destroy failed rc=%d\n", rc);
        (void)remove(save_path);
        return 1;
    }
    restored_session = (VNRuntimeSession*)0;

    file_session = (VNRuntimeSession*)1;
    if (vn_runtime_session_load_from_file((const char*)0, &file_session) != VN_E_INVALID_ARG ||
        file_session != (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null load_from_file path\n");
        (void)remove(save_path);
        return 1;
    }
    if (vn_runtime_session_load_from_file("tests/fixtures/vnsave/v1/sample.vnsave", &file_session) != VN_E_UNSUPPORTED) {
        (void)fprintf(stderr, "expected VN_E_UNSUPPORTED for non-runtime vnsave payload\n");
        (void)remove(save_path);
        return 1;
    }
    rc = vn_runtime_session_load_from_file(save_path, &file_session);
    if (rc != VN_OK || file_session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "load snapshot from file failed rc=%d\n", rc);
        (void)remove(save_path);
        return 1;
    }
    rc = vn_runtime_session_step(file_session, &restored_res);
    if (rc != VN_OK || expect_same_resume_result(&resumed_res, &restored_res) != 0) {
        (void)fprintf(stderr, "file snapshot resume mismatch rc=%d\n", rc);
        (void)vn_runtime_session_destroy(file_session);
        (void)remove(save_path);
        return 1;
    }
    rc = vn_runtime_session_destroy(file_session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "file restored destroy failed rc=%d\n", rc);
        (void)remove(save_path);
        return 1;
    }
    file_session = (VNRuntimeSession*)0;
    (void)remove(save_path);
    rc = vn_runtime_session_create((const VNRunConfig*)0, &session);
    if (rc != VN_OK || session == (VNRuntimeSession*)0) {
        (void)fprintf(stderr, "default create failed rc=%d\n", rc);
        return 1;
    }
    rc = vn_runtime_session_destroy(session);
    if (rc != VN_OK) {
        (void)fprintf(stderr, "default destroy failed rc=%d\n", rc);
        return 1;
    }

    if (vn_runtime_session_create(&cfg, (VNRuntimeSession**)0) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null out_session\n");
        return 1;
    }
    if (vn_runtime_session_step((VNRuntimeSession*)0, &res) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null session step\n");
        return 1;
    }
    if (vn_runtime_session_set_choice((VNRuntimeSession*)0, 0u) != VN_E_INVALID_ARG) {
        (void)fprintf(stderr, "expected VN_E_INVALID_ARG for null session set_choice\n");
        return 1;
    }
    if (vn_runtime_session_is_done((const VNRuntimeSession*)0) != VN_TRUE) {
        (void)fprintf(stderr, "expected null session as done\n");
        return 1;
    }
    if (vn_runtime_session_destroy((VNRuntimeSession*)0) != VN_OK) {
        (void)fprintf(stderr, "null destroy should be ok\n");
        return 1;
    }

    (void)printf("test_runtime_session ok\n");
    return 0;
}
