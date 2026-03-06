#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BASELINE_BACKEND="scalar"
CANDIDATE_BACKEND="avx2"
SCENES="S0,S1,S2,S3"
OUT_DIR="tests/perf/compare_run"
DURATION_SEC=120
WARMUP_SEC=20
DT_MS=16
RESOLUTION="600x800"
FRAMES_OVERRIDE=""
KEEP_RAW=0

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
    --scenes)
      SCENES="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    --duration-sec)
      DURATION_SEC="$2"
      shift 2
      ;;
    --warmup-sec)
      WARMUP_SEC="$2"
      shift 2
      ;;
    --dt-ms)
      DT_MS="$2"
      shift 2
      ;;
    --resolution)
      RESOLUTION="$2"
      shift 2
      ;;
    --frames)
      FRAMES_OVERRIDE="$2"
      shift 2
      ;;
    --keep-raw)
      KEEP_RAW=1
      shift 1
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

BASELINE_DIR="$OUT_DIR/$BASELINE_BACKEND"
CANDIDATE_DIR="$OUT_DIR/$CANDIDATE_BACKEND"
COMPARE_DIR="$OUT_DIR/compare"
COMMON_ARGS=(
  --scenes "$SCENES"
  --duration-sec "$DURATION_SEC"
  --warmup-sec "$WARMUP_SEC"
  --dt-ms "$DT_MS"
  --resolution "$RESOLUTION"
)

if [[ -n "$FRAMES_OVERRIDE" ]]; then
  COMMON_ARGS+=(--frames "$FRAMES_OVERRIDE")
fi
if [[ "$KEEP_RAW" -ne 0 ]]; then
  COMMON_ARGS+=(--keep-raw)
fi

mkdir -p "$OUT_DIR"

echo "[perf-compare] baseline backend=$BASELINE_BACKEND out=$BASELINE_DIR"
./tests/perf/run_perf.sh --backend "$BASELINE_BACKEND" --out-dir "$BASELINE_DIR" "${COMMON_ARGS[@]}"

echo "[perf-compare] candidate backend=$CANDIDATE_BACKEND out=$CANDIDATE_DIR"
./tests/perf/run_perf.sh --backend "$CANDIDATE_BACKEND" --out-dir "$CANDIDATE_DIR" "${COMMON_ARGS[@]}"

./tests/perf/compare_perf.sh \
  --baseline "$BASELINE_BACKEND:$BASELINE_DIR/perf_summary.csv" \
  --candidate "$CANDIDATE_BACKEND:$CANDIDATE_DIR/perf_summary.csv" \
  --out-dir "$COMPARE_DIR"

echo "[perf-compare] done baseline=$BASELINE_BACKEND candidate=$CANDIDATE_BACKEND out=$OUT_DIR"
