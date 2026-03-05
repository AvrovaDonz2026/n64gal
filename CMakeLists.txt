cmake_minimum_required(VERSION 3.16)
project(n64gal C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror -pedantic-errors)

add_library(vn_core
    src/core/backend_registry.c
    src/core/renderer.c
    src/core/vm.c
    src/core/pack.c
    src/frontend/render_ops.c
    src/backend/avx2/avx2_backend.c
    src/backend/scalar/scalar_backend.c
)

target_include_directories(vn_core PUBLIC include)

add_executable(vn_player
    src/main.c
)

target_link_libraries(vn_player PRIVATE vn_core)

enable_testing()
add_test(NAME smoke_scalar COMMAND vn_player --backend=scalar --scene S0)
add_test(NAME smoke_auto COMMAND vn_player --scene S0)

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

add_executable(test_vm tests/unit/test_vm.c)
target_link_libraries(test_vm PRIVATE vn_core)
add_test(NAME unit_vm COMMAND test_vm)
