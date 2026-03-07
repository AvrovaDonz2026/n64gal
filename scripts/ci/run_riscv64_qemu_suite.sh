#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_ci_riscv64}"
LOG_DIR="$BUILD_DIR/ci_logs"
GOLDEN_ARTIFACT_DIR="$BUILD_DIR/golden_artifacts"
SUMMARY_MD="$BUILD_DIR/ci_suite_summary.md"
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

mkdir -p "$BUILD_DIR" "$LOG_DIR" "$GOLDEN_ARTIFACT_DIR"

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

write_summary() {
  local status
  status="$1"
  {
    echo "# RISC-V QEMU Suite Summary"
    echo
    echo "- Status: \`$status\`"
    echo "- Build dir: \`$BUILD_DIR\`"
    echo "- Log dir: \`$LOG_DIR\`"
    echo "- Golden artifact dir: \`$GOLDEN_ARTIFACT_DIR\`"
    echo "- RVV mode: \`$RVV_MODE\`"
    echo "- Fallback evidence: \`$LOG_DIR/player_auto.log\`, \`$LOG_DIR/test_runtime_api.log\`, \`$LOG_DIR/test_renderer_fallback.log\`"
    if compgen -G "$GOLDEN_ARTIFACT_DIR/*" >/dev/null; then
      echo "- Golden artifacts present: yes"
    else
      echo "- Golden artifacts present: no (exact-match run or no diff output)"
    fi
  } > "$SUMMARY_MD"
}

trap 'rc=$?; if [[ $rc -eq 0 ]]; then write_summary success; else write_summary failed; fi; exit $rc' EXIT

require_tool riscv64-linux-gnu-gcc
require_tool "$QEMU_BIN"

if [ ! -d "$QEMU_SYSROOT" ]; then
  echo "missing qemu sysroot: $QEMU_SYSROOT" >&2
  exit 1
fi

./scripts/ci/build_riscv64_cross.sh

echo "[riscv64-qemu] running blocking smoke suite"
run_capture "$LOG_DIR/test_vnpak.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/test_vnpak_riscv64"
run_capture "$LOG_DIR/test_runtime_api.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/test_runtime_api_riscv64"
assert_log_has "$LOG_DIR/test_runtime_api.log" "backend=scalar"
run_capture "$LOG_DIR/test_runtime_session.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/test_runtime_session_riscv64"
run_capture "$LOG_DIR/test_runtime_golden.log" \
  env VN_GOLDEN_ARTIFACT_DIR="$GOLDEN_ARTIFACT_DIR" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/test_runtime_golden_riscv64"
assert_log_has "$LOG_DIR/test_runtime_golden.log" "test_runtime_golden ok"
run_capture "$LOG_DIR/test_renderer_fallback.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/test_renderer_fallback_riscv64"
run_capture "$LOG_DIR/player_scalar.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/vn_player_riscv64" \
  --backend=scalar --scene=S0 --frames=2 --dt-ms=16
assert_log_has "$LOG_DIR/player_scalar.log" "backend=scalar"
run_capture "$LOG_DIR/player_auto.log" \
  "$QEMU_BIN" -L "$QEMU_SYSROOT" "$BUILD_DIR/vn_player_riscv64" \
  --scene=S0 --frames=2 --dt-ms=16
assert_log_has "$LOG_DIR/player_auto.log" "backend=scalar"

echo "[riscv64-qemu] blocking smoke suite passed"

if [ "$RVV_MODE" = "skip" ]; then
  echo "[riscv64-qemu] rvv smoke skipped by request"
  exit 0
fi

rvv_smoke() {
  run_capture "$LOG_DIR/player_rvv_forced.log" \
    "$QEMU_BIN" -cpu "$QEMU_RVV_CPU" -L "$QEMU_SYSROOT" "$BUILD_DIR/vn_player_rvv" \
    --backend=rvv --scene=S0 --frames=2 --dt-ms=16
  assert_log_has "$LOG_DIR/player_rvv_forced.log" "backend=rvv"
  run_capture "$LOG_DIR/player_rvv_auto.log" \
    "$QEMU_BIN" -cpu "$QEMU_RVV_CPU" -L "$QEMU_SYSROOT" "$BUILD_DIR/vn_player_rvv" \
    --scene=S0 --frames=2 --dt-ms=16
  assert_log_has "$LOG_DIR/player_rvv_auto.log" "backend=rvv"
  run_capture "$LOG_DIR/test_backend_consistency_rvv.log" \
    "$QEMU_BIN" -cpu "$QEMU_RVV_CPU" -L "$QEMU_SYSROOT" "$BUILD_DIR/test_backend_consistency_rvv"
  assert_log_has "$LOG_DIR/test_backend_consistency_rvv.log" "test_backend_consistency ok"
  run_capture "$LOG_DIR/test_renderer_dirty_submit_rvv.log" \
    "$QEMU_BIN" -cpu "$QEMU_RVV_CPU" -L "$QEMU_SYSROOT" "$BUILD_DIR/test_renderer_dirty_submit_rvv"
  assert_log_has "$LOG_DIR/test_renderer_dirty_submit_rvv.log" "matched backend=rvv"
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
