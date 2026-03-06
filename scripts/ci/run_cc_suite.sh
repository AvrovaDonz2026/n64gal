#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_ci_cc}"
mkdir -p "$BUILD_DIR"

./scripts/check_c89.sh
./tools/scriptc/build_demo_scripts.sh >/tmp/n64gal_ci_scriptc.log
./tools/packer/make_demo_pack.sh >/tmp/n64gal_ci_packer.log

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
TESTS=(
  test_backend_registry
  test_render_ops
  test_vnpak
  test_renderer_fallback
  test_backend_consistency
  test_vm
  test_runtime_api
  test_runtime_session
)

for test_name in "${TESTS[@]}"; do
  cc "${CFLAGS[@]}" "tests/unit/${test_name}.c" "${COMMON_SRC[@]}" -o "$BUILD_DIR/$test_name"
done

cc "${CFLAGS[@]}" src/main.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/vn_player"

for test_name in "${TESTS[@]}"; do
  "$BUILD_DIR/$test_name"
done
