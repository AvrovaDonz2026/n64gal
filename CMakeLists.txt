cmake_minimum_required(VERSION 3.16)
project(n64gal C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic-errors)
endif()

if(MSVC)
    if(CMAKE_VS_PLATFORM_NAME STREQUAL "x64" OR CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
        set_source_files_properties(src/backend/avx2/avx2_backend.c src/backend/avx2/avx2_fill_fade.c src/backend/avx2/avx2_textured.c PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    endif()
endif()

add_library(vn_core
    src/core/error.c
    src/core/backend_registry.c
    src/core/renderer.c
    src/core/save.c
    src/core/vm.c
    src/core/pack.c
    src/core/platform.c
    src/core/runtime_cli.c
    src/core/runtime_input.c
    src/core/runtime_parse.c
    src/core/runtime_persist.c
    src/core/runtime_session_support.c
    src/core/dynamic_resolution.c
    src/frontend/render_ops.c
    src/frontend/dirty_tiles.c
    src/backend/common/pixel_pipeline.c
    src/backend/avx2/avx2_backend.c
    src/backend/avx2/avx2_fill_fade.c
    src/backend/avx2/avx2_textured.c
    src/backend/neon/neon_backend.c
    src/backend/rvv/rvv_backend.c
    src/backend/scalar/scalar_backend.c
)

target_include_directories(vn_core PUBLIC include)

add_library(vn_preview
    src/tools/preview_parse.c
    src/tools/preview_cli.c
    src/tools/preview_report.c
)
target_link_libraries(vn_preview PUBLIC vn_core)
target_include_directories(vn_preview PUBLIC include)

enable_testing()

option(VN_BUILD_PLAYER "Build vn_player demo executable" OFF)
if(VN_BUILD_PLAYER)
    add_executable(vn_player
        src/main.c
    )
    target_link_libraries(vn_player PRIVATE vn_core)
    add_test(NAME smoke_scalar COMMAND vn_player --backend=scalar --scene S0)
    add_test(NAME smoke_auto COMMAND vn_player --scene S0)
endif()

add_executable(vn_previewd
    src/tools/previewd_main.c
)
target_link_libraries(vn_previewd PRIVATE vn_preview)

add_executable(vnsave_migrate
    tools/migrate/vnsave_migrate.c
)
target_link_libraries(vnsave_migrate PRIVATE vn_core)

add_executable(vnsave_probe
    tools/probe/vnsave_probe.c
)
target_link_libraries(vnsave_probe PRIVATE vn_core)

add_executable(vn_backend_kernel_bench
    tests/perf/backend_kernel_bench.c
)
target_link_libraries(vn_backend_kernel_bench PRIVATE vn_core)

add_executable(test_backend_registry tests/unit/test_backend_registry.c)
target_link_libraries(test_backend_registry PRIVATE vn_core)
add_test(NAME unit_backend_registry COMMAND test_backend_registry)

add_executable(test_error_codes tests/unit/test_error_codes.c)
target_link_libraries(test_error_codes PRIVATE vn_core)
add_test(NAME unit_error_codes COMMAND test_error_codes)

add_executable(test_render_ops tests/unit/test_render_ops.c)
target_link_libraries(test_render_ops PRIVATE vn_core)
add_test(NAME unit_render_ops COMMAND test_render_ops)

add_executable(test_dirty_tiles tests/unit/test_dirty_tiles.c)
target_link_libraries(test_dirty_tiles PRIVATE vn_core)
add_test(NAME unit_dirty_tiles COMMAND test_dirty_tiles)

add_executable(test_dynamic_resolution tests/unit/test_dynamic_resolution.c)
target_link_libraries(test_dynamic_resolution PRIVATE vn_core)
add_test(NAME unit_dynamic_resolution COMMAND test_dynamic_resolution)

add_executable(test_vnpak tests/unit/test_vnpak.c)
target_link_libraries(test_vnpak PRIVATE vn_core)
add_test(NAME unit_vnpak COMMAND test_vnpak)

add_executable(test_vnsave tests/unit/test_vnsave.c)
target_link_libraries(test_vnsave PRIVATE vn_core)
add_test(NAME unit_vnsave COMMAND test_vnsave)

add_executable(test_vnsave_migrate
    tests/unit/test_vnsave_migrate.c
    tools/migrate/vnsave_migrate.c
)
target_compile_definitions(test_vnsave_migrate PRIVATE VN_SAVE_MIGRATE_NO_MAIN)
target_link_libraries(test_vnsave_migrate PRIVATE vn_core)
add_test(NAME unit_vnsave_migrate COMMAND test_vnsave_migrate)

add_executable(test_vnsave_probe_tool
    tests/unit/test_vnsave_probe_tool.c
    tools/probe/vnsave_probe.c
)
target_compile_definitions(test_vnsave_probe_tool PRIVATE VN_SAVE_PROBE_NO_MAIN)
target_link_libraries(test_vnsave_probe_tool PRIVATE vn_core)
add_test(NAME unit_vnsave_probe_tool COMMAND test_vnsave_probe_tool)

add_executable(test_renderer_fallback tests/unit/test_renderer_fallback.c)
target_link_libraries(test_renderer_fallback PRIVATE vn_core)
add_test(NAME unit_renderer_fallback COMMAND test_renderer_fallback)

add_executable(test_renderer_dirty_submit tests/unit/test_renderer_dirty_submit.c)
target_link_libraries(test_renderer_dirty_submit PRIVATE vn_core)
add_test(NAME unit_renderer_dirty_submit COMMAND test_renderer_dirty_submit)

add_executable(test_backend_consistency tests/unit/test_backend_consistency.c)
target_link_libraries(test_backend_consistency PRIVATE vn_core)
add_test(NAME unit_backend_consistency COMMAND test_backend_consistency)

add_executable(test_backend_priority tests/unit/test_backend_priority.c)
target_link_libraries(test_backend_priority PRIVATE vn_core)
add_test(NAME unit_backend_priority COMMAND test_backend_priority)

add_executable(test_avx2_fastpath_parity tests/unit/test_avx2_fastpath_parity.c)
target_link_libraries(test_avx2_fastpath_parity PRIVATE vn_core)
add_test(NAME unit_avx2_fastpath_parity COMMAND test_avx2_fastpath_parity)

add_executable(test_vm tests/unit/test_vm.c)
target_link_libraries(test_vm PRIVATE vn_core)
add_test(NAME unit_vm COMMAND test_vm)

add_executable(test_runtime_api tests/unit/test_runtime_api.c)
target_link_libraries(test_runtime_api PRIVATE vn_core)
add_test(NAME unit_runtime_api COMMAND test_runtime_api)

add_executable(test_runtime_dynamic_resolution tests/unit/test_runtime_dynamic_resolution.c)
target_link_libraries(test_runtime_dynamic_resolution PRIVATE vn_core)
add_test(NAME unit_runtime_dynamic_resolution COMMAND test_runtime_dynamic_resolution)

add_executable(test_runtime_session tests/unit/test_runtime_session.c)
target_link_libraries(test_runtime_session PRIVATE vn_core)
add_test(NAME unit_runtime_session COMMAND test_runtime_session)

add_executable(test_runtime_input tests/unit/test_runtime_input.c)
target_link_libraries(test_runtime_input PRIVATE vn_core)
add_test(NAME unit_runtime_input COMMAND test_runtime_input)

add_executable(test_runtime_cli_errors tests/unit/test_runtime_cli_errors.c)
target_link_libraries(test_runtime_cli_errors PRIVATE vn_core)
add_test(NAME unit_runtime_cli_errors COMMAND test_runtime_cli_errors)

add_executable(test_runtime_golden tests/unit/test_runtime_golden.c)
target_link_libraries(test_runtime_golden PRIVATE vn_core)
add_test(NAME unit_runtime_golden COMMAND test_runtime_golden)

add_executable(test_platform_paths tests/unit/test_platform_paths.c)
target_link_libraries(test_platform_paths PRIVATE vn_core)
add_test(NAME unit_platform_paths COMMAND test_platform_paths)

add_executable(test_preview_protocol tests/integration/test_preview_protocol.c)
target_link_libraries(test_preview_protocol PRIVATE vn_preview)
add_test(NAME integration_preview_protocol COMMAND test_preview_protocol)

add_executable(example_host_embed
    examples/host-embed/session_loop.c
)
target_link_libraries(example_host_embed PRIVATE vn_core)
add_test(NAME example_host_embed COMMAND example_host_embed)

add_executable(example_host_embed_linux_tty
    examples/host-embed/linux_tty_loop.c
)
target_link_libraries(example_host_embed_linux_tty PRIVATE vn_core)
add_test(NAME example_host_embed_linux_tty COMMAND example_host_embed_linux_tty)

add_executable(example_host_embed_windows_console
    examples/host-embed/windows_console_loop.c
)
target_link_libraries(example_host_embed_windows_console PRIVATE vn_core)
add_test(NAME example_host_embed_windows_console COMMAND example_host_embed_windows_console)

if(VN_BUILD_PLAYER)
    set_tests_properties(smoke_scalar smoke_auto PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
set_tests_properties(
    unit_backend_registry
    unit_error_codes
    unit_render_ops
    unit_dirty_tiles
    unit_dynamic_resolution
    unit_vnpak
    unit_vnsave
    unit_vnsave_migrate
    unit_vnsave_probe_tool
    unit_renderer_fallback
    unit_renderer_dirty_submit
    unit_backend_consistency
    unit_backend_priority
    unit_avx2_fastpath_parity
    unit_vm
    unit_runtime_api
    unit_runtime_dynamic_resolution
    unit_runtime_session
    unit_runtime_input
    unit_runtime_cli_errors
    unit_runtime_golden
    unit_platform_paths
    integration_preview_protocol
    example_host_embed
    example_host_embed_linux_tty
    example_host_embed_windows_console
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
