#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_ci_riscv64}"
TMP_BUILD_DIR="$BUILD_DIR/tmp"
mkdir -p "$BUILD_DIR" "$TMP_BUILD_DIR"
export TMPDIR="$TMP_BUILD_DIR"

COMMON_SRC=(
  src/core/error.c
  src/core/backend_registry.c
  src/core/renderer.c
  src/core/save.c
  src/core/vm.c
  src/core/pack.c
  src/core/platform.c
  src/core/runtime_cli.c
  src/core/runtime_persist.c
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
CFLAGS=(
  -std=c89
  -Wall
  -Wextra
  -Werror
  -pedantic-errors
  -Iinclude
)
RVV_FLAGS=(
  -march=rv64gcv
  -mabi=lp64d
)
TESTS=(
  test_backend_registry
  test_error_codes
  test_render_ops
  test_dirty_tiles
  test_dynamic_resolution
  test_vnpak
  test_vnsave
  test_vnsave_migrate
  test_vnsave_probe_tool
  test_renderer_fallback
  test_renderer_dirty_submit
  test_backend_consistency
  test_vm
  test_runtime_api
  test_runtime_dynamic_resolution
  test_runtime_session
  test_runtime_cli_errors
  test_runtime_golden
)

echo "[riscv64-cross] build dir: $BUILD_DIR"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" src/main.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/vn_player_riscv64"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" "${RVV_FLAGS[@]}" src/main.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/vn_player_rvv"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" "${RVV_FLAGS[@]}" -c src/backend/rvv/rvv_backend.c -o "$BUILD_DIR/rvv_backend.o"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" "${RVV_FLAGS[@]}" tests/unit/test_backend_consistency.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/test_backend_consistency_rvv"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" "${RVV_FLAGS[@]}" tests/unit/test_renderer_dirty_submit.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/test_renderer_dirty_submit_rvv"

for test_name in "${TESTS[@]}"; do
  echo "[riscv64-cross] compiling $test_name"
  if [[ "$test_name" == "test_vnsave_migrate" ]]; then
    riscv64-linux-gnu-gcc "${CFLAGS[@]}" -DVN_SAVE_MIGRATE_NO_MAIN "tests/unit/${test_name}.c" tools/migrate/vnsave_migrate.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/${test_name}_riscv64"
  elif [[ "$test_name" == "test_vnsave_probe_tool" ]]; then
    riscv64-linux-gnu-gcc "${CFLAGS[@]}" -DVN_SAVE_PROBE_NO_MAIN "tests/unit/${test_name}.c" tools/probe/vnsave_probe.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/${test_name}_riscv64"
  else
    riscv64-linux-gnu-gcc "${CFLAGS[@]}" "tests/unit/${test_name}.c" "${COMMON_SRC[@]}" -o "$BUILD_DIR/${test_name}_riscv64"
  fi
done

riscv64-linux-gnu-gcc "${CFLAGS[@]}" tools/migrate/vnsave_migrate.c src/core/error.c src/core/save.c src/core/platform.c -o "$BUILD_DIR/vnsave_migrate_riscv64"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" tools/probe/vnsave_probe.c src/core/error.c src/core/save.c src/core/platform.c -o "$BUILD_DIR/vnsave_probe_riscv64"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" examples/host-embed/session_loop.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/example_host_embed_riscv64"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" examples/host-embed/linux_tty_loop.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/example_host_embed_linux_tty_riscv64"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" examples/host-embed/windows_console_loop.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/example_host_embed_windows_console_riscv64"
