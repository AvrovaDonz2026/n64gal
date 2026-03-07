#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_ci_perf}"
PLATFORM_LABEL="generic"
SIMD_BACKEND="avx2"
SCENES="S1,S3"
DURATION_SEC=2
WARMUP_SEC=1
DT_MS=16
RESOLUTION="600x800"
DIRTY_DURATION_SEC=6
DIRTY_WARMUP_SEC=1
DIRTY_REPEAT_COUNT=3
DYNRES_SCENES="S3"
DYNRES_DURATION_SEC=6
DYNRES_WARMUP_SEC=1
DYNRES_RESOLUTION="1200x1600"
KERNEL_RESOLUTION=""
KERNEL_ITERATIONS=24
KERNEL_WARMUP=6
THRESHOLD_FILE="tests/perf/perf_thresholds.csv"
THRESHOLD_PROFILE="linux-x64-scalar-avx2-smoke"
RUNNER_BIN=""
KERNEL_RUNNER_BIN=""
SKIP_BUILD=0

resolve_kernel_runner_bin() {
  local player_bin="$1"
  local player_dir
  local suffix

  player_dir="$(dirname "$player_bin")"
  suffix=""
  case "$player_bin" in
    *.exe)
      suffix=".exe"
      ;;
  esac
  printf '%s/vn_backend_kernel_bench%s\n' "$player_dir" "$suffix"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    --platform-label)
      PLATFORM_LABEL="$2"
      shift 2
      ;;
    --simd-backend)
      SIMD_BACKEND="$2"
      shift 2
      ;;
    --runner-bin)
      RUNNER_BIN="$2"
      shift 2
      ;;
    --kernel-runner-bin)
      KERNEL_RUNNER_BIN="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift 1
      ;;
    --scenes)
      SCENES="$2"
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
    --dirty-duration-sec)
      DIRTY_DURATION_SEC="$2"
      shift 2
      ;;
    --dirty-warmup-sec)
      DIRTY_WARMUP_SEC="$2"
      shift 2
      ;;
    --dirty-repeat-count)
      DIRTY_REPEAT_COUNT="$2"
      shift 2
      ;;
    --dynres-scenes)
      DYNRES_SCENES="$2"
      shift 2
      ;;
    --dynres-duration-sec)
      DYNRES_DURATION_SEC="$2"
      shift 2
      ;;
    --dynres-warmup-sec)
      DYNRES_WARMUP_SEC="$2"
      shift 2
      ;;
    --dynres-resolution)
      DYNRES_RESOLUTION="$2"
      shift 2
      ;;
    --kernel-resolution)
      KERNEL_RESOLUTION="$2"
      shift 2
      ;;
    --kernel-iterations)
      KERNEL_ITERATIONS="$2"
      shift 2
      ;;
    --kernel-warmup)
      KERNEL_WARMUP="$2"
      shift 2
      ;;
    --threshold-file)
      THRESHOLD_FILE="$2"
      shift 2
      ;;
    --threshold-profile)
      THRESHOLD_PROFILE="$2"
      shift 2
      ;;
    --no-threshold)
      THRESHOLD_PROFILE=""
      shift 1
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if ! [[ "$DIRTY_REPEAT_COUNT" =~ ^[1-9][0-9]*$ ]]; then
  echo "invalid --dirty-repeat-count value: $DIRTY_REPEAT_COUNT" >&2
  exit 2
fi
if ! [[ "$KERNEL_ITERATIONS" =~ ^[1-9][0-9]*$ ]]; then
  echo "invalid --kernel-iterations value: $KERNEL_ITERATIONS" >&2
  exit 2
fi
if ! [[ "$KERNEL_WARMUP" =~ ^[0-9]+$ ]]; then
  echo "invalid --kernel-warmup value: $KERNEL_WARMUP" >&2
  exit 2
fi
if ! [[ "$SKIP_BUILD" =~ ^[01]$ ]]; then
  echo "invalid --skip-build state: $SKIP_BUILD" >&2
  exit 2
fi
if [[ -z "$SIMD_BACKEND" ]]; then
  echo "simd-backend must not be empty" >&2
  exit 2
fi
if [[ -z "$KERNEL_RESOLUTION" ]]; then
  KERNEL_RESOLUTION="$RESOLUTION"
fi
if [[ -z "$KERNEL_RUNNER_BIN" && -n "$RUNNER_BIN" ]]; then
  KERNEL_RUNNER_BIN="$(resolve_kernel_runner_bin "$RUNNER_BIN")"
fi

SCALAR_SIMD_DIR="$OUT_DIR/scalar_vs_${SIMD_BACKEND}"
DIRTY_TILE_DIR="$OUT_DIR/${SIMD_BACKEND}_dirty_tile"
DYNRES_DIR="$OUT_DIR/scalar_dynamic_resolution"
KERNEL_COMPARE_DIR="$OUT_DIR/kernel_scalar_vs_${SIMD_BACKEND}"
SUMMARY_MD="$OUT_DIR/perf_workflow_summary.md"
mkdir -p "$OUT_DIR"

append_report() {
  local title
  local path

  title="$1"
  path="$2"

  {
    echo "## $title"
    echo
    if [[ -f "$path" ]]; then
      cat "$path"
    else
      echo "_missing: \`$path\`_"
    fi
    echo
  } >> "$SUMMARY_MD"
}

COMMON_ARGS=(
  --scenes "$SCENES"
  --duration-sec "$DURATION_SEC"
  --warmup-sec "$WARMUP_SEC"
  --dt-ms "$DT_MS"
  --resolution "$RESOLUTION"
)
if [[ -n "$RUNNER_BIN" ]]; then
  COMMON_ARGS+=(--runner-bin "$RUNNER_BIN")
fi
if [[ "$SKIP_BUILD" -eq 1 ]]; then
  COMMON_ARGS+=(--skip-build)
fi

COMPARE_A_ARGS=(
  --baseline scalar
  --candidate "$SIMD_BACKEND"
  --out-dir "$SCALAR_SIMD_DIR"
)
if [[ -n "$THRESHOLD_PROFILE" ]]; then
  COMPARE_A_ARGS+=(
    --threshold-file "$THRESHOLD_FILE"
    --threshold-profile "$THRESHOLD_PROFILE"
  )
fi
./tests/perf/run_perf_compare.sh "${COMPARE_A_ARGS[@]}" "${COMMON_ARGS[@]}"

DIRTY_ARGS=(
  --baseline "$SIMD_BACKEND"
  --baseline-label "${SIMD_BACKEND}_dirty_off"
  --baseline-perf-dirty-tile off
  --candidate "$SIMD_BACKEND"
  --candidate-label "${SIMD_BACKEND}_dirty_on"
  --candidate-perf-dirty-tile on
  --scenes "$SCENES"
  --duration-sec "$DIRTY_DURATION_SEC"
  --warmup-sec "$DIRTY_WARMUP_SEC"
  --dt-ms "$DT_MS"
  --resolution "$RESOLUTION"
  --repeat "$DIRTY_REPEAT_COUNT"
  --out-dir "$DIRTY_TILE_DIR"
)
if [[ -n "$RUNNER_BIN" ]]; then
  DIRTY_ARGS+=(--runner-bin "$RUNNER_BIN")
fi
if [[ "$SKIP_BUILD" -eq 1 ]]; then
  DIRTY_ARGS+=(--skip-build)
fi
./tests/perf/run_perf_compare.sh "${DIRTY_ARGS[@]}"

DYNRES_ARGS=(
  --baseline scalar
  --baseline-label scalar_dynres_off
  --baseline-perf-dynamic-resolution off
  --candidate scalar
  --candidate-label scalar_dynres_on
  --candidate-perf-dynamic-resolution on
  --scenes "$DYNRES_SCENES"
  --duration-sec "$DYNRES_DURATION_SEC"
  --warmup-sec "$DYNRES_WARMUP_SEC"
  --dt-ms "$DT_MS"
  --resolution "$DYNRES_RESOLUTION"
  --out-dir "$DYNRES_DIR"
)
if [[ -n "$RUNNER_BIN" ]]; then
  DYNRES_ARGS+=(--runner-bin "$RUNNER_BIN")
fi
if [[ "$SKIP_BUILD" -eq 1 ]]; then
  DYNRES_ARGS+=(--skip-build)
fi
./tests/perf/run_perf_compare.sh "${DYNRES_ARGS[@]}"

KERNEL_ARGS=(
  --baseline scalar
  --candidate "$SIMD_BACKEND"
  --resolution "$KERNEL_RESOLUTION"
  --iterations "$KERNEL_ITERATIONS"
  --warmup "$KERNEL_WARMUP"
  --out-dir "$KERNEL_COMPARE_DIR"
)
if [[ -n "$KERNEL_RUNNER_BIN" ]]; then
  KERNEL_ARGS+=(--runner-bin "$KERNEL_RUNNER_BIN")
fi
if [[ "$SKIP_BUILD" -eq 1 ]]; then
  KERNEL_ARGS+=(--skip-build)
fi
./tests/perf/run_kernel_compare.sh "${KERNEL_ARGS[@]}"

cat > "$SUMMARY_MD" <<EOF_SUMMARY
# Perf Workflow Summary

- Platform label: \`$PLATFORM_LABEL\`
- Output dir: \`$OUT_DIR\`
- SIMD backend: \`$SIMD_BACKEND\`
- Smoke scenes: \`$SCENES\`
- Smoke duration / warmup: \`${DURATION_SEC}s / ${WARMUP_SEC}s\`
- dt_ms: \`$DT_MS\`
- Smoke resolution: \`$RESOLUTION\`
- Dirty duration / warmup: \`${DIRTY_DURATION_SEC}s / ${DIRTY_WARMUP_SEC}s\`
- Dirty repeat count: \`$DIRTY_REPEAT_COUNT\`
- Dynres scenes: \`$DYNRES_SCENES\`
- Dynres duration / warmup: \`${DYNRES_DURATION_SEC}s / ${DYNRES_WARMUP_SEC}s\`
- Dynres resolution: \`$DYNRES_RESOLUTION\`
- Kernel resolution: \`$KERNEL_RESOLUTION\`
- Kernel iterations / warmup: \`${KERNEL_ITERATIONS} / ${KERNEL_WARMUP}\`
- Runner bin: \`${RUNNER_BIN:-auto-build}\`
- Kernel runner bin: \`${KERNEL_RUNNER_BIN:-auto-build}\`
- Skip build: \`${SKIP_BUILD}\`
- Threshold profile: \`${THRESHOLD_PROFILE:-none}\`
- Compare A: \`scalar -> $SIMD_BACKEND\`
- Compare B: \`${SIMD_BACKEND} dirty-tile off -> on\`
- Compare C: \`scalar dynamic-resolution off -> on\`
- Compare D: \`kernel scalar -> $SIMD_BACKEND\`
- Artifact A dir: \`$SCALAR_SIMD_DIR\`
- Artifact B dir: \`$DIRTY_TILE_DIR\`
- Artifact C dir: \`$DYNRES_DIR\`
- Artifact D dir: \`$KERNEL_COMPARE_DIR\`

EOF_SUMMARY

append_report "Scalar vs ${SIMD_BACKEND} Compare" "$SCALAR_SIMD_DIR/compare/perf_compare.md"
if [[ -n "$THRESHOLD_PROFILE" ]]; then
  append_report "Scalar vs ${SIMD_BACKEND} Threshold" "$SCALAR_SIMD_DIR/compare/perf_threshold_report.md"
else
  {
    echo "## Scalar vs ${SIMD_BACKEND} Threshold"
    echo
    echo "_disabled: threshold profile not configured for this platform_"
    echo
  } >> "$SUMMARY_MD"
fi
append_report "${SIMD_BACKEND} Dirty-Tile Off vs On Compare" "$DIRTY_TILE_DIR/compare/perf_compare.md"
append_report "Scalar Dynamic-Resolution Off vs On Compare" "$DYNRES_DIR/compare/perf_compare.md"
append_report "Kernel Scalar vs ${SIMD_BACKEND} Compare" "$KERNEL_COMPARE_DIR/compare/kernel_compare.md"

echo "[ci-perf] done platform=$PLATFORM_LABEL simd_backend=$SIMD_BACKEND out=$OUT_DIR"
