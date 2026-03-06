cmake_minimum_required(VERSION 3.16)
project(n64gal C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic-errors)
endif()

add_library(vn_core
    src/core/backend_registry.c
    src/core/renderer.c
    src/core/vm.c
    src/core/pack.c
    src/core/runtime_cli.c
    src/frontend/render_ops.c
    src/backend/common/pixel_pipeline.c
    src/backend/avx2/avx2_backend.c
    src/backend/neon/neon_backend.c
    src/backend/rvv/rvv_backend.c
    src/backend/scalar/scalar_backend.c
)

target_include_directories(vn_core PUBLIC include)

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

add_executable(test_backend_registry tests/unit/test_backend_registry.c)
target_link_libraries(test_backend_registry PRIVATE vn_core)
add_test(NAME unit_backend_registry COMMAND test_backend_registry)

add_executable(test_render_ops tests/unit/test_render_ops.c)
target_link_libraries(test_render_ops PRIVATE vn_core)
add_test(NAME unit_render_ops COMMAND test_render_ops)

add_executable(test_vnpak tests/unit/test_vnpak.c)
target_link_libraries(test_vnpak PRIVATE vn_core)
add_test(NAME unit_vnpak COMMAND test_vnpak)

add_executable(test_renderer_fallback tests/unit/test_renderer_fallback.c)
target_link_libraries(test_renderer_fallback PRIVATE vn_core)
add_test(NAME unit_renderer_fallback COMMAND test_renderer_fallback)

add_executable(test_backend_consistency tests/unit/test_backend_consistency.c)
target_link_libraries(test_backend_consistency PRIVATE vn_core)
add_test(NAME unit_backend_consistency COMMAND test_backend_consistency)

add_executable(test_vm tests/unit/test_vm.c)
target_link_libraries(test_vm PRIVATE vn_core)
add_test(NAME unit_vm COMMAND test_vm)

add_executable(test_runtime_api tests/unit/test_runtime_api.c)
target_link_libraries(test_runtime_api PRIVATE vn_core)
add_test(NAME unit_runtime_api COMMAND test_runtime_api)

add_executable(test_runtime_session tests/unit/test_runtime_session.c)
target_link_libraries(test_runtime_session PRIVATE vn_core)
add_test(NAME unit_runtime_session COMMAND test_runtime_session)
