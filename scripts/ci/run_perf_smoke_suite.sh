#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_ci_perf}"
SCENES="S0,S3"
DURATION_SEC=2
WARMUP_SEC=1
DT_MS=16
RESOLUTION="600x800"
DYNRES_SCENES="S3"
DYNRES_DURATION_SEC=6
DYNRES_WARMUP_SEC=1
DYNRES_RESOLUTION="1200x1600"
THRESHOLD_FILE="tests/perf/perf_thresholds.csv"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out-dir)
      OUT_DIR="$2"
      shift 2
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
    --threshold-file)
      THRESHOLD_FILE="$2"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

SCALAR_AVX2_DIR="$OUT_DIR/scalar_vs_avx2"
DIRTY_TILE_DIR="$OUT_DIR/avx2_dirty_tile"
DYNRES_DIR="$OUT_DIR/scalar_dynamic_resolution"
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

./tests/perf/run_perf_compare.sh \
  --baseline scalar \
  --candidate avx2 \
  --scenes "$SCENES" \
  --duration-sec "$DURATION_SEC" \
  --warmup-sec "$WARMUP_SEC" \
  --dt-ms "$DT_MS" \
  --resolution "$RESOLUTION" \
  --threshold-file "$THRESHOLD_FILE" \
  --threshold-profile linux-x64-scalar-avx2-smoke \
  --out-dir "$SCALAR_AVX2_DIR"

./tests/perf/run_perf_compare.sh \
  --baseline avx2 \
  --baseline-label avx2_dirty_off \
  --baseline-perf-dirty-tile off \
  --candidate avx2 \
  --candidate-label avx2_dirty_on \
  --candidate-perf-dirty-tile on \
  --scenes "$SCENES" \
  --duration-sec "$DURATION_SEC" \
  --warmup-sec "$WARMUP_SEC" \
  --dt-ms "$DT_MS" \
  --resolution "$RESOLUTION" \
  --out-dir "$DIRTY_TILE_DIR"

./tests/perf/run_perf_compare.sh \
  --baseline scalar \
  --baseline-label scalar_dynres_off \
  --baseline-perf-dynamic-resolution off \
  --candidate scalar \
  --candidate-label scalar_dynres_on \
  --candidate-perf-dynamic-resolution on \
  --scenes "$DYNRES_SCENES" \
  --duration-sec "$DYNRES_DURATION_SEC" \
  --warmup-sec "$DYNRES_WARMUP_SEC" \
  --dt-ms "$DT_MS" \
  --resolution "$DYNRES_RESOLUTION" \
  --out-dir "$DYNRES_DIR"

cat > "$SUMMARY_MD" <<EOF_SUMMARY
# Perf Workflow Summary

- Output dir: \`$OUT_DIR\`
- Smoke scenes: \`$SCENES\`
- Smoke duration / warmup: \`${DURATION_SEC}s / ${WARMUP_SEC}s\`
- dt_ms: \`$DT_MS\`
- Smoke resolution: \`$RESOLUTION\`
- Dynres scenes: \`$DYNRES_SCENES\`
- Dynres duration / warmup: \`${DYNRES_DURATION_SEC}s / ${DYNRES_WARMUP_SEC}s\`
- Dynres resolution: \`$DYNRES_RESOLUTION\`
- Compare A: \`scalar -> avx2\` with threshold profile \`linux-x64-scalar-avx2-smoke\`
- Compare B: \`avx2 dirty-tile off -> on\`
- Compare C: \`scalar dynamic-resolution off -> on\`
- Artifact A dir: \`$SCALAR_AVX2_DIR\`
- Artifact B dir: \`$DIRTY_TILE_DIR\`
- Artifact C dir: \`$DYNRES_DIR\`

EOF_SUMMARY

append_report "Scalar vs AVX2 Compare" "$SCALAR_AVX2_DIR/compare/perf_compare.md"
append_report "Scalar vs AVX2 Threshold" "$SCALAR_AVX2_DIR/compare/perf_threshold_report.md"
append_report "AVX2 Dirty-Tile Off vs On Compare" "$DIRTY_TILE_DIR/compare/perf_compare.md"
append_report "Scalar Dynamic-Resolution Off vs On Compare" "$DYNRES_DIR/compare/perf_compare.md"

echo "[ci-perf] done out=$OUT_DIR"
