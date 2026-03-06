#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BASELINE_REV="${BASELINE_REV:-75ee8f9}"
CANDIDATE_REV="${CANDIDATE_REV:-HEAD}"
QEMU_BIN="${QEMU_BIN:-qemu-riscv64}"
QEMU_SYSROOT="${QEMU_SYSROOT:-/usr/riscv64-linux-gnu}"
QEMU_RVV_CPU="${QEMU_RVV_CPU:-max,v=true}"
PERF_SCENES="${PERF_SCENES:-S0,S3}"
PERF_DURATION_SEC="${PERF_DURATION_SEC:-2}"
PERF_WARMUP_SEC="${PERF_WARMUP_SEC:-1}"
PERF_DT_MS="${PERF_DT_MS:-16}"
PERF_RESOLUTION="${PERF_RESOLUTION:-600x800}"
OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_ci_riscv64_perf}"

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing required tool: $1" >&2
    exit 1
  fi
}

require_tool git
require_tool riscv64-linux-gnu-gcc
require_tool "$QEMU_BIN"

if [ ! -d "$QEMU_SYSROOT" ]; then
  echo "missing qemu sysroot: $QEMU_SYSROOT" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
BASELINE_REV="$(git rev-parse --verify "$BASELINE_REV")"
CANDIDATE_REV="$(git rev-parse --verify "$CANDIDATE_REV")"

CC=riscv64-linux-gnu-gcc \
VN_PERF_CFLAGS='-march=rv64gcv -mabi=lp64d' \
VN_PERF_RUNNER_PREFIX="$QEMU_BIN -cpu $QEMU_RVV_CPU -L $QEMU_SYSROOT" \
./tests/perf/run_perf_compare_revs.sh \
  --baseline-rev "$BASELINE_REV" \
  --candidate-rev "$CANDIDATE_REV" \
  --backend rvv \
  --scenes "$PERF_SCENES" \
  --duration-sec "$PERF_DURATION_SEC" \
  --warmup-sec "$PERF_WARMUP_SEC" \
  --dt-ms "$PERF_DT_MS" \
  --resolution "$PERF_RESOLUTION" \
  --out-dir "$OUT_DIR"

REPORT_MD="$OUT_DIR/compare/perf_compare_revs.md"
if [ ! -f "$REPORT_MD" ]; then
  echo "missing report: $REPORT_MD" >&2
  exit 1
fi

echo "[riscv64-qemu-perf] baseline=$BASELINE_REV candidate=$CANDIDATE_REV"
echo "[riscv64-qemu-perf] report=$REPORT_MD"
