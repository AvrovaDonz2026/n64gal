#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BASELINE_BACKEND="scalar"
CANDIDATE_BACKEND="avx2"
BASELINE_LABEL=""
CANDIDATE_LABEL=""
RESOLUTION="600x800"
ITERATIONS=24
WARMUP=6
OUT_DIR="tests/perf/kernel_compare_run"
RUNNER_BIN=""
SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --baseline)
      BASELINE_BACKEND="$2"
      shift 2
      ;;
    --candidate)
      CANDIDATE_BACKEND="$2"
      shift 2
      ;;
    --baseline-label)
      BASELINE_LABEL="$2"
      shift 2
      ;;
    --candidate-label)
      CANDIDATE_LABEL="$2"
      shift 2
      ;;
    --resolution)
      RESOLUTION="$2"
      shift 2
      ;;
    --iterations)
      ITERATIONS="$2"
      shift 2
      ;;
    --warmup)
      WARMUP="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    --runner-bin)
      RUNNER_BIN="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift 1
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$BASELINE_LABEL" ]]; then
  BASELINE_LABEL="$BASELINE_BACKEND"
fi
if [[ -z "$CANDIDATE_LABEL" ]]; then
  CANDIDATE_LABEL="$CANDIDATE_BACKEND"
fi

BASELINE_DIR="$OUT_DIR/$BASELINE_LABEL"
CANDIDATE_DIR="$OUT_DIR/$CANDIDATE_LABEL"
COMPARE_DIR="$OUT_DIR/compare"
BASELINE_CSV="$BASELINE_DIR/kernel_bench.csv"
CANDIDATE_CSV="$CANDIDATE_DIR/kernel_bench.csv"
COMMON_ARGS=(
  --resolution "$RESOLUTION"
  --iterations "$ITERATIONS"
  --warmup "$WARMUP"
)

if [[ -n "$RUNNER_BIN" ]]; then
  COMMON_ARGS+=(--runner-bin "$RUNNER_BIN")
fi
if [[ "$SKIP_BUILD" -eq 1 ]]; then
  COMMON_ARGS+=(--skip-build)
fi

mkdir -p "$BASELINE_DIR" "$CANDIDATE_DIR"

echo "[kernel-compare] baseline label=$BASELINE_LABEL backend=$BASELINE_BACKEND out=$BASELINE_CSV"
./tests/perf/run_kernel_bench.sh --backend "$BASELINE_BACKEND" --out-csv "$BASELINE_CSV" "${COMMON_ARGS[@]}"

echo "[kernel-compare] candidate label=$CANDIDATE_LABEL backend=$CANDIDATE_BACKEND out=$CANDIDATE_CSV"
./tests/perf/run_kernel_bench.sh --backend "$CANDIDATE_BACKEND" --out-csv "$CANDIDATE_CSV" "${COMMON_ARGS[@]}"

./tests/perf/compare_kernel_bench.sh \
  --baseline "$BASELINE_LABEL:$BASELINE_CSV" \
  --candidate "$CANDIDATE_LABEL:$CANDIDATE_CSV" \
  --out-dir "$COMPARE_DIR"

echo "[kernel-compare] done baseline=$BASELINE_LABEL/$BASELINE_BACKEND candidate=$CANDIDATE_LABEL/$CANDIDATE_BACKEND out=$OUT_DIR"
