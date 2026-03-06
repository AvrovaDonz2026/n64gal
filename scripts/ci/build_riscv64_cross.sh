#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_ci_riscv64}"
mkdir -p "$BUILD_DIR"

COMMON_SRC=(
  src/core/backend_registry.c
  src/core/renderer.c
  src/core/vm.c
  src/core/pack.c
  src/core/platform.c
  src/core/runtime_cli.c
  src/frontend/render_ops.c
  src/backend/common/pixel_pipeline.c
  src/backend/avx2/avx2_backend.c
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
  test_render_ops
  test_vnpak
  test_renderer_fallback
  test_backend_consistency
  test_vm
  test_runtime_api
  test_runtime_session
  test_runtime_golden
)

echo "[riscv64-cross] build dir: $BUILD_DIR"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" src/main.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/vn_player_riscv64"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" "${RVV_FLAGS[@]}" src/main.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/vn_player_rvv"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" "${RVV_FLAGS[@]}" -c src/backend/rvv/rvv_backend.c -o "$BUILD_DIR/rvv_backend.o"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" "${RVV_FLAGS[@]}" tests/unit/test_backend_consistency.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/test_backend_consistency_rvv"

for test_name in "${TESTS[@]}"; do
  echo "[riscv64-cross] compiling $test_name"
  riscv64-linux-gnu-gcc "${CFLAGS[@]}" "tests/unit/${test_name}.c" "${COMMON_SRC[@]}" -o "$BUILD_DIR/${test_name}_riscv64"
done
