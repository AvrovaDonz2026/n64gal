#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BASELINE_BACKEND="scalar"
CANDIDATE_BACKEND="avx2"
BASELINE_LABEL=""
CANDIDATE_LABEL=""
BASELINE_PERF_FRAME_REUSE=""
CANDIDATE_PERF_FRAME_REUSE=""
BASELINE_PERF_OP_CACHE=""
CANDIDATE_PERF_OP_CACHE=""
BASELINE_PERF_DIRTY_TILE=""
CANDIDATE_PERF_DIRTY_TILE=""
SCENES="S0,S1,S2,S3"
OUT_DIR="tests/perf/compare_run"
DURATION_SEC=120
WARMUP_SEC=20
DT_MS=16
RESOLUTION="600x800"
FRAMES_OVERRIDE=""
KEEP_RAW=0
THRESHOLD_FILE=""
THRESHOLD_PROFILE=""

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
    --baseline-perf-frame-reuse)
      BASELINE_PERF_FRAME_REUSE="$2"
      shift 2
      ;;
    --candidate-perf-frame-reuse)
      CANDIDATE_PERF_FRAME_REUSE="$2"
      shift 2
      ;;
    --baseline-perf-op-cache)
      BASELINE_PERF_OP_CACHE="$2"
      shift 2
      ;;
    --candidate-perf-op-cache)
      CANDIDATE_PERF_OP_CACHE="$2"
      shift 2
      ;;
    --baseline-perf-dirty-tile)
      BASELINE_PERF_DIRTY_TILE="$2"
      shift 2
      ;;
    --candidate-perf-dirty-tile)
      CANDIDATE_PERF_DIRTY_TILE="$2"
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
    --threshold-file)
      THRESHOLD_FILE="$2"
      shift 2
      ;;
    --threshold-profile)
      THRESHOLD_PROFILE="$2"
      shift 2
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

BASELINE_ARGS=()
CANDIDATE_ARGS=()
if [[ -n "$BASELINE_PERF_FRAME_REUSE" ]]; then
  BASELINE_ARGS+=(--perf-frame-reuse "$BASELINE_PERF_FRAME_REUSE")
fi
if [[ -n "$CANDIDATE_PERF_FRAME_REUSE" ]]; then
  CANDIDATE_ARGS+=(--perf-frame-reuse "$CANDIDATE_PERF_FRAME_REUSE")
fi
if [[ -n "$BASELINE_PERF_OP_CACHE" ]]; then
  BASELINE_ARGS+=(--perf-op-cache "$BASELINE_PERF_OP_CACHE")
fi
if [[ -n "$CANDIDATE_PERF_OP_CACHE" ]]; then
  CANDIDATE_ARGS+=(--perf-op-cache "$CANDIDATE_PERF_OP_CACHE")
fi
if [[ -n "$BASELINE_PERF_DIRTY_TILE" ]]; then
  BASELINE_ARGS+=(--perf-dirty-tile "$BASELINE_PERF_DIRTY_TILE")
fi
if [[ -n "$CANDIDATE_PERF_DIRTY_TILE" ]]; then
  CANDIDATE_ARGS+=(--perf-dirty-tile "$CANDIDATE_PERF_DIRTY_TILE")
fi

echo "[perf-compare] baseline label=$BASELINE_LABEL backend=$BASELINE_BACKEND out=$BASELINE_DIR"
./tests/perf/run_perf.sh --backend "$BASELINE_BACKEND" --out-dir "$BASELINE_DIR" "${COMMON_ARGS[@]}" "${BASELINE_ARGS[@]}"

echo "[perf-compare] candidate label=$CANDIDATE_LABEL backend=$CANDIDATE_BACKEND out=$CANDIDATE_DIR"
./tests/perf/run_perf.sh --backend "$CANDIDATE_BACKEND" --out-dir "$CANDIDATE_DIR" "${COMMON_ARGS[@]}" "${CANDIDATE_ARGS[@]}"

./tests/perf/compare_perf.sh \
  --baseline "$BASELINE_LABEL:$BASELINE_DIR/perf_summary.csv" \
  --candidate "$CANDIDATE_LABEL:$CANDIDATE_DIR/perf_summary.csv" \
  --out-dir "$COMPARE_DIR"

if [[ -n "$THRESHOLD_PROFILE" ]]; then
  if [[ -z "$THRESHOLD_FILE" ]]; then
    THRESHOLD_FILE="tests/perf/perf_thresholds.csv"
  fi
  ./tests/perf/check_perf_thresholds.sh \
    --compare-csv "$COMPARE_DIR/perf_compare.csv" \
    --threshold-file "$THRESHOLD_FILE" \
    --profile "$THRESHOLD_PROFILE" \
    --out-dir "$COMPARE_DIR"
fi

echo "[perf-compare] done baseline=$BASELINE_LABEL/$BASELINE_BACKEND candidate=$CANDIDATE_LABEL/$CANDIDATE_BACKEND out=$OUT_DIR"
