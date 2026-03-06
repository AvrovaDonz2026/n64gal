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

riscv64-linux-gnu-gcc "${CFLAGS[@]}" src/main.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/vn_player_riscv64"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" "${RVV_FLAGS[@]}" src/main.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/vn_player_rvv"
riscv64-linux-gnu-gcc "${CFLAGS[@]}" "${RVV_FLAGS[@]}" -c src/backend/rvv/rvv_backend.c -o "$BUILD_DIR/rvv_backend.o"
