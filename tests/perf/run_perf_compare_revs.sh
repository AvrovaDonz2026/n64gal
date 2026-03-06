#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BASELINE_REV=""
CANDIDATE_REV=""
BACKEND="rvv"
SCENES="S0,S3"
OUT_DIR="tests/perf/rev_compare"
DURATION_SEC=2
WARMUP_SEC=1
DT_MS=16
RESOLUTION="600x800"
FRAMES_OVERRIDE=""
KEEP_RAW=0
THRESHOLD_FILE=""
THRESHOLD_PROFILE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --baseline-rev)
      BASELINE_REV="$2"
      shift 2
      ;;
    --candidate-rev)
      CANDIDATE_REV="$2"
      shift 2
      ;;
    --backend)
      BACKEND="$2"
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

if [[ -z "$BASELINE_REV" || -z "$CANDIDATE_REV" ]]; then
  echo "usage: ./tests/perf/run_perf_compare_revs.sh --baseline-rev REV --candidate-rev REV [--backend rvv] [--out-dir DIR]" >&2
  exit 2
fi

BASELINE_REV="$(git rev-parse --verify "$BASELINE_REV")"
CANDIDATE_REV="$(git rev-parse --verify "$CANDIDATE_REV")"

if [[ "$OUT_DIR" != /* ]]; then
  OUT_DIR="$ROOT_DIR/$OUT_DIR"
fi
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
BASELINE_SRC="$WORK_DIR/baseline_src"
CANDIDATE_SRC="$WORK_DIR/candidate_src"
BASELINE_OUT="$OUT_DIR/baseline"
CANDIDATE_OUT="$OUT_DIR/candidate"
COMPARE_OUT="$OUT_DIR/compare"
META_MD="$COMPARE_OUT/perf_compare_revs.md"

mkdir -p "$BASELINE_SRC" "$CANDIDATE_SRC" "$BASELINE_OUT" "$CANDIDATE_OUT" "$COMPARE_OUT"

git archive "$BASELINE_REV" | tar -x -C "$BASELINE_SRC"
git archive "$CANDIDATE_REV" | tar -x -C "$CANDIDATE_SRC"

COMMON_ARGS=(
  --backend "$BACKEND"
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

echo "[perf-revs] baseline rev=$BASELINE_REV backend=$BACKEND"
env \
  CC="${CC:-cc}" \
  VN_PERF_CFLAGS="${VN_PERF_CFLAGS:-}" \
  VN_PERF_LDFLAGS="${VN_PERF_LDFLAGS:-}" \
  VN_PERF_RUNNER_PREFIX="${VN_PERF_RUNNER_PREFIX:-}" \
  VN_PERF_RUNNER_BIN="$WORK_DIR/baseline_runner" \
  "$ROOT_DIR/tests/perf/run_perf.sh" \
    --source-root "$BASELINE_SRC" \
    --out-dir "$BASELINE_OUT" \
    "${COMMON_ARGS[@]}"

echo "[perf-revs] candidate rev=$CANDIDATE_REV backend=$BACKEND"
env \
  CC="${CC:-cc}" \
  VN_PERF_CFLAGS="${VN_PERF_CFLAGS:-}" \
  VN_PERF_LDFLAGS="${VN_PERF_LDFLAGS:-}" \
  VN_PERF_RUNNER_PREFIX="${VN_PERF_RUNNER_PREFIX:-}" \
  VN_PERF_RUNNER_BIN="$WORK_DIR/candidate_runner" \
  "$ROOT_DIR/tests/perf/run_perf.sh" \
    --source-root "$CANDIDATE_SRC" \
    --out-dir "$CANDIDATE_OUT" \
    "${COMMON_ARGS[@]}"

./tests/perf/compare_perf.sh \
  --baseline "${BACKEND}@${BASELINE_REV:0:7}:$BASELINE_OUT/perf_summary.csv" \
  --candidate "${BACKEND}@${CANDIDATE_REV:0:7}:$CANDIDATE_OUT/perf_summary.csv" \
  --out-dir "$COMPARE_OUT"

if [[ -n "$THRESHOLD_PROFILE" ]]; then
  if [[ -z "$THRESHOLD_FILE" ]]; then
    THRESHOLD_FILE="tests/perf/perf_thresholds.csv"
  fi
  ./tests/perf/check_perf_thresholds.sh \
    --compare-csv "$COMPARE_OUT/perf_compare.csv" \
    --threshold-file "$THRESHOLD_FILE" \
    --profile "$THRESHOLD_PROFILE" \
    --out-dir "$COMPARE_OUT"
fi

HOST_UNAME="$(uname -a)"
CC_VERSION="$("${CC:-cc}" --version | head -n 1)"
RUNNER_VERSION=""
if [[ -n "${VN_PERF_RUNNER_PREFIX:-}" ]]; then
  RUNNER_BIN_NAME="${VN_PERF_RUNNER_PREFIX%% *}"
  if command -v "$RUNNER_BIN_NAME" >/dev/null 2>&1; then
    RUNNER_VERSION="$($RUNNER_BIN_NAME --version 2>/dev/null | head -n 1 || true)"
  fi
fi
{
  echo "# N64GAL Revision Perf Compare"
  echo
  echo "- Baseline revision: \`$BASELINE_REV\`"
  echo "- Candidate revision: \`$CANDIDATE_REV\`"
  echo "- Backend: \`$BACKEND\`"
  echo "- Scenes: \`$SCENES\`"
  echo "- Resolution: \`$RESOLUTION\`"
  echo "- Duration / warmup: ${DURATION_SEC}s / ${WARMUP_SEC}s"
  echo "- dt_ms: ${DT_MS}"
  echo "- Host: \`$HOST_UNAME\`"
  echo "- Compiler: \`$CC_VERSION\`"
  if [[ -n "${VN_PERF_RUNNER_PREFIX:-}" ]]; then
    echo "- Runner prefix: \`${VN_PERF_RUNNER_PREFIX}\`"
  fi
  if [[ -n "$RUNNER_VERSION" ]]; then
    echo "- Runner version: \`$RUNNER_VERSION\`"
  fi
  echo
  cat "$COMPARE_OUT/perf_compare.md"
} > "$META_MD"

echo "[perf-revs] wrote $META_MD"
