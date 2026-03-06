#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_ci_riscv64}"
QEMU_BIN="${QEMU_BIN:-qemu-riscv64}"
QEMU_SYSROOT="${QEMU_SYSROOT:-/usr/riscv64-linux-gnu}"
QEMU_RVV_CPU="${QEMU_RVV_CPU:-max,v=true}"
RVV_MODE="warn"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --require-rvv)
      RVV_MODE="require"
      ;;
    --skip-rvv)
      RVV_MODE="skip"
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 2
      ;;
  esac
  shift
done

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing required tool: $1" >&2
    exit 1
  fi
}

run_capture() {
  local log_path
  log_path="$1"
  shift
  "$@" >"$log_path" 2>&1
  cat "$log_path"
}

assert_log_has() {
  local log_path
  local pattern
  log_path="$1"
  pattern="$2"
  if ! grep -q "$pattern" "$log_path"; then
    echo "expected pattern not found: $pattern" >&2
    return 1
  fi
  return 0
}

require_tool riscv64-linux-gnu-gcc
require_tool "$QEMU_BIN"

if [ ! -d "$QEMU_SYSROOT" ]; then
  echo "missing qemu sysroot: $QEMU_SYSROOT" >&2
  exit 1
fi

./scripts/ci/build_riscv64_cross.sh

echo "[riscv64-qemu] running blocking smoke suite"
run_capture "$BUILD_DIR/test_vnpak.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/test_vnpak_riscv64"
run_capture "$BUILD_DIR/test_runtime_api.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/test_runtime_api_riscv64"
assert_log_has "$BUILD_DIR/test_runtime_api.log" "backend=scalar"
run_capture "$BUILD_DIR/test_runtime_session.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/test_runtime_session_riscv64"
run_capture "$BUILD_DIR/test_renderer_fallback.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/test_renderer_fallback_riscv64"
run_capture "$BUILD_DIR/player_scalar.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/vn_player_riscv64" \
  --backend=scalar --scene=S0 --frames=2 --dt-ms=16
assert_log_has "$BUILD_DIR/player_scalar.log" "backend=scalar"
run_capture "$BUILD_DIR/player_auto.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/vn_player_riscv64" \
  --scene=S0 --frames=2 --dt-ms=16
assert_log_has "$BUILD_DIR/player_auto.log" "backend=scalar"

echo "[riscv64-qemu] blocking smoke suite passed"

if [ "$RVV_MODE" = "skip" ]; then
  echo "[riscv64-qemu] rvv smoke skipped by request"
  exit 0
fi

rvv_smoke() {
  run_capture "$BUILD_DIR/player_rvv_forced.log" \
    "$QEMU_BIN" -cpu "$QEMU_RVV_CPU" -L "$QEMU_SYSROOT" "$BUILD_DIR/vn_player_rvv" \
    --backend=rvv --scene=S0 --frames=2 --dt-ms=16
  assert_log_has "$BUILD_DIR/player_rvv_forced.log" "backend=rvv"
  run_capture "$BUILD_DIR/player_rvv_auto.log" \
    "$QEMU_BIN" -cpu "$QEMU_RVV_CPU" -L "$QEMU_SYSROOT" "$BUILD_DIR/vn_player_rvv" \
    --scene=S0 --frames=2 --dt-ms=16
  assert_log_has "$BUILD_DIR/player_rvv_auto.log" "backend=rvv"
}

if rvv_smoke; then
  echo "[riscv64-qemu] rvv smoke suite passed"
  exit 0
fi

if [ "$RVV_MODE" = "require" ]; then
  echo "[riscv64-qemu] rvv smoke suite failed in required mode" >&2
  exit 1
fi

echo "[riscv64-qemu] rvv smoke suite failed but is non-blocking in warn mode" >&2
exit 0
