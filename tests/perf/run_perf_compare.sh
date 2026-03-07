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
BASELINE_PERF_DYNAMIC_RESOLUTION=""
CANDIDATE_PERF_DYNAMIC_RESOLUTION=""
SCENES="S0,S1,S2,S3,S10"
OUT_DIR="tests/perf/compare_run"
DURATION_SEC=120
WARMUP_SEC=20
DT_MS=16
RESOLUTION="600x800"
FRAMES_OVERRIDE=""
KEEP_RAW=0
THRESHOLD_FILE=""
THRESHOLD_PROFILE=""
REPEAT_COUNT=1
RUNNER_BIN=""
SKIP_BUILD=0

sorted_median() {
  local values_file="$1"
  local fmt="$2"

  awk -v fmt="$fmt" '
    {
      values[++n] = $1 + 0.0;
    }
    END {
      if (n == 0) {
        printf fmt, 0.0;
        exit;
      }
      mid = int((n + 1) / 2);
      if ((n % 2) == 1) {
        printf fmt, values[mid];
      } else {
        printf fmt, (values[mid] + values[mid + 1]) / 2.0;
      }
    }
  ' "$values_file"
}

aggregate_perf_summary() {
  local label="$1"
  local dest_dir="$2"
  shift 2

  local csvs=("$@")
  local first_csv
  local summary_csv
  local repeats_csv
  local scenes_tmp
  local rows_tmp
  local values_tmp
  local scene
  local idx
  local csv
  local row
  local first_row
  local sample_count
  local warmup_sec
  local duration_sec
  local backend
  local dt_ms
  local resolution
  local pass
  local perf_frame_reuse
  local perf_op_cache
  local perf_dirty_tile
  local perf_dynamic_resolution
  local host_cpu
  local p95_median
  local avg_median
  local rss_median

  if [[ "${#csvs[@]}" -eq 0 ]]; then
    echo "aggregate_perf_summary: no input csvs for $label" >&2
    exit 2
  fi

  first_csv="${csvs[0]}"
  summary_csv="$dest_dir/perf_summary.csv"
  repeats_csv="$dest_dir/perf_summary_repeats.csv"

  mkdir -p "$dest_dir"
  head -n 1 "$first_csv" > "$summary_csv"
  echo "run,scene,sample_count,p95_frame_ms,avg_frame_ms,max_rss_mb,warmup_sec,duration_sec,backend,dt_ms,resolution,pass,perf_frame_reuse,perf_op_cache,perf_dirty_tile,perf_dynamic_resolution,host_cpu" > "$repeats_csv"

  scenes_tmp="$(mktemp)"
  tail -n +2 "$first_csv" | cut -d, -f1 | sort -u > "$scenes_tmp"

  while IFS= read -r scene; do
    if [[ -z "$scene" ]]; then
      continue
    fi

    rows_tmp="$(mktemp)"
    for idx in "${!csvs[@]}"; do
      csv="${csvs[$idx]}"
      row="$(awk -F, -v scene="$scene" 'NR > 1 && $1 == scene { print $0; exit }' "$csv")"
      if [[ -z "$row" ]]; then
        rm -f "$rows_tmp" "$scenes_tmp"
        echo "missing scene=$scene in csv=$csv during repeat aggregation" >&2
        exit 1
      fi
      printf '%s\n' "$row" >> "$rows_tmp"
      printf 'run_%02d,%s\n' "$((idx + 1))" "$row" >> "$repeats_csv"
    done

    first_row="$(sed -n '1p' "$rows_tmp")"
    IFS=, read -r _ sample_count _ _ _ warmup_sec duration_sec backend dt_ms resolution pass perf_frame_reuse perf_op_cache perf_dirty_tile perf_dynamic_resolution host_cpu <<< "$first_row"

    values_tmp="$(mktemp)"
    awk -F, '{ print $3 }' "$rows_tmp" | sort -n > "$values_tmp"
    p95_median="$(sorted_median "$values_tmp" "%.3f")"
    rm -f "$values_tmp"

    values_tmp="$(mktemp)"
    awk -F, '{ print $4 }' "$rows_tmp" | sort -n > "$values_tmp"
    avg_median="$(sorted_median "$values_tmp" "%.3f")"
    rm -f "$values_tmp"

    values_tmp="$(mktemp)"
    awk -F, '{ print $5 }' "$rows_tmp" | sort -n > "$values_tmp"
    rss_median="$(sorted_median "$values_tmp" "%.3f")"
    rm -f "$values_tmp"

    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
      "$scene" \
      "$sample_count" \
      "$p95_median" \
      "$avg_median" \
      "$rss_median" \
      "$warmup_sec" \
      "$duration_sec" \
      "$backend" \
      "$dt_ms" \
      "$resolution" \
      "$pass" \
      "$perf_frame_reuse" \
      "$perf_op_cache" \
      "$perf_dirty_tile" \
      "$perf_dynamic_resolution" \
      "$host_cpu" >> "$summary_csv"

    rm -f "$rows_tmp"
  done < "$scenes_tmp"

  rm -f "$scenes_tmp"
  cp "$ROOT_DIR/tests/perf/report_template.md" "$dest_dir/perf_report_template.md"

  {
    echo "# Perf Repeat Aggregate"
    echo
    echo "- Label: \`$label\`"
    echo "- Repeat count: $REPEAT_COUNT"
    echo "- Aggregate method: median per scene for \`p95_frame_ms\`, \`avg_frame_ms\`, and \`max_rss_mb\`"
    echo "- Raw repeat rows: \`$repeats_csv\`"
    echo "- Aggregate summary: \`$summary_csv\`"
  } > "$dest_dir/perf_repeat_aggregate.md"
}

repeat_metric_stats() {
  local csv="$1"
  local scene="$2"
  local field="$3"
  local values_tmp
  local count
  local min_value
  local max_value
  local median_value
  local range_pct

  values_tmp="$(mktemp)"
  awk -F, -v scene="$scene" -v field="$field" 'NR > 1 && $2 == scene { print $field }' "$csv" | sort -n > "$values_tmp"

  count="$(awk 'END { print NR + 0 }' "$values_tmp")"
  if [[ "$count" -eq 0 ]]; then
    rm -f "$values_tmp"
    echo "repeat_metric_stats: missing scene=$scene field=$field csv=$csv" >&2
    exit 1
  fi

  min_value="$(sed -n '1p' "$values_tmp")"
  max_value="$(sed -n '$p' "$values_tmp")"
  median_value="$(sorted_median "$values_tmp" "%.3f")"
  range_pct="$(awk -v min="$min_value" -v max="$max_value" -v median="$median_value" 'BEGIN {
    if ((median + 0.0) == 0.0) {
      printf "%.2f", 0.0;
    } else {
      printf "%.2f", ((max - min) / median) * 100.0;
    }
  }')"

  printf '%s,%s,%s,%s,%s\n' "$count" "$min_value" "$median_value" "$max_value" "$range_pct"
  rm -f "$values_tmp"
}

write_repeat_variability_report() {
  local baseline_label="$1"
  local baseline_csv="$2"
  local candidate_label="$3"
  local candidate_csv="$4"
  local out_dir="$5"
  local base_scenes_tmp
  local cand_scenes_tmp
  local scene
  local csv_path
  local md_path
  local base_p95_stats
  local cand_p95_stats
  local base_avg_stats
  local cand_avg_stats
  local base_runs
  local cand_runs
  local base_p95_min
  local base_p95_median
  local base_p95_max
  local base_p95_range
  local cand_p95_min
  local cand_p95_median
  local cand_p95_max
  local cand_p95_range
  local base_avg_min
  local base_avg_median
  local base_avg_max
  local base_avg_range
  local cand_avg_min
  local cand_avg_median
  local cand_avg_max
  local cand_avg_range
  local mean_base_p95_range
  local mean_cand_p95_range

  if [[ ! -f "$baseline_csv" || ! -f "$candidate_csv" ]]; then
    echo "write_repeat_variability_report: repeat csv missing" >&2
    exit 2
  fi

  mkdir -p "$out_dir"
  csv_path="$out_dir/perf_repeat_variability.csv"
  md_path="$out_dir/perf_repeat_variability.md"

  echo "scene,baseline_label,candidate_label,baseline_runs,candidate_runs,baseline_p95_min_ms,baseline_p95_median_ms,baseline_p95_max_ms,baseline_p95_range_pct,candidate_p95_min_ms,candidate_p95_median_ms,candidate_p95_max_ms,candidate_p95_range_pct,baseline_avg_min_ms,baseline_avg_median_ms,baseline_avg_max_ms,baseline_avg_range_pct,candidate_avg_min_ms,candidate_avg_median_ms,candidate_avg_max_ms,candidate_avg_range_pct" > "$csv_path"

  base_scenes_tmp="$(mktemp)"
  cand_scenes_tmp="$(mktemp)"
  tail -n +2 "$baseline_csv" | cut -d, -f2 | sort -u > "$base_scenes_tmp"
  tail -n +2 "$candidate_csv" | cut -d, -f2 | sort -u > "$cand_scenes_tmp"

  if ! cmp -s "$base_scenes_tmp" "$cand_scenes_tmp"; then
    echo "repeat scene sets differ between baseline and candidate" >&2
    diff -u "$base_scenes_tmp" "$cand_scenes_tmp" >&2 || true
    rm -f "$base_scenes_tmp" "$cand_scenes_tmp"
    exit 1
  fi

  while IFS= read -r scene; do
    if [[ -z "$scene" ]]; then
      continue
    fi

    base_p95_stats="$(repeat_metric_stats "$baseline_csv" "$scene" 4)"
    cand_p95_stats="$(repeat_metric_stats "$candidate_csv" "$scene" 4)"
    base_avg_stats="$(repeat_metric_stats "$baseline_csv" "$scene" 5)"
    cand_avg_stats="$(repeat_metric_stats "$candidate_csv" "$scene" 5)"

    IFS=, read -r base_runs base_p95_min base_p95_median base_p95_max base_p95_range <<< "$base_p95_stats"
    IFS=, read -r cand_runs cand_p95_min cand_p95_median cand_p95_max cand_p95_range <<< "$cand_p95_stats"
    IFS=, read -r _ base_avg_min base_avg_median base_avg_max base_avg_range <<< "$base_avg_stats"
    IFS=, read -r _ cand_avg_min cand_avg_median cand_avg_max cand_avg_range <<< "$cand_avg_stats"

    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
      "$scene" \
      "$baseline_label" \
      "$candidate_label" \
      "$base_runs" \
      "$cand_runs" \
      "$base_p95_min" \
      "$base_p95_median" \
      "$base_p95_max" \
      "$base_p95_range" \
      "$cand_p95_min" \
      "$cand_p95_median" \
      "$cand_p95_max" \
      "$cand_p95_range" \
      "$base_avg_min" \
      "$base_avg_median" \
      "$base_avg_max" \
      "$base_avg_range" \
      "$cand_avg_min" \
      "$cand_avg_median" \
      "$cand_avg_max" \
      "$cand_avg_range" >> "$csv_path"
  done < "$base_scenes_tmp"

  rm -f "$base_scenes_tmp" "$cand_scenes_tmp"

  mean_base_p95_range="$(awk -F, 'NR > 1 { sum += $9; n += 1 } END { if (n == 0) printf "%.2f", 0.0; else printf "%.2f", sum / n }' "$csv_path")"
  mean_cand_p95_range="$(awk -F, 'NR > 1 { sum += $13; n += 1 } END { if (n == 0) printf "%.2f", 0.0; else printf "%.2f", sum / n }' "$csv_path")"

  {
    echo "# Perf Repeat Variability"
    echo
    echo "- Baseline label: \`$baseline_label\`"
    echo "- Candidate label: \`$candidate_label\`"
    echo "- Baseline repeat rows: \`$baseline_csv\`"
    echo "- Candidate repeat rows: \`$candidate_csv\`"
    echo "- Repeat count: $REPEAT_COUNT"
    echo "- Mean baseline p95 range: ${mean_base_p95_range}%"
    echo "- Mean candidate p95 range: ${mean_cand_p95_range}%"
    echo
    echo "High range percentage indicates short-window jitter across repeats, even when the median compare stays stable."
    echo
    echo "| scene | ${baseline_label} p95 min / med / max | ${baseline_label} p95 range | ${candidate_label} p95 min / med / max | ${candidate_label} p95 range | ${baseline_label} avg range | ${candidate_label} avg range |"
    echo "|---|---:|---:|---:|---:|---:|---:|"
    tail -n +2 "$csv_path" | awk -F, '{
      printf "| %s | %.3f / %.3f / %.3f | %.2f%% | %.3f / %.3f / %.3f | %.2f%% | %.2f%% | %.2f%% |\n",
             $1,
             $6 + 0.0,
             $7 + 0.0,
             $8 + 0.0,
             $9 + 0.0,
             $10 + 0.0,
             $11 + 0.0,
             $12 + 0.0,
             $13 + 0.0,
             $17 + 0.0,
             $21 + 0.0;
    }'
  } > "$md_path"

  echo "[perf-compare] wrote $csv_path"
  echo "[perf-compare] wrote $md_path"
}

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
    --baseline-perf-dynamic-resolution)
      BASELINE_PERF_DYNAMIC_RESOLUTION="$2"
      shift 2
      ;;
    --candidate-perf-dynamic-resolution)
      CANDIDATE_PERF_DYNAMIC_RESOLUTION="$2"
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
    --repeat)
      REPEAT_COUNT="$2"
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
if ! [[ "$REPEAT_COUNT" =~ ^[1-9][0-9]*$ ]]; then
  echo "invalid --repeat value: $REPEAT_COUNT" >&2
  exit 2
fi

BASELINE_DIR="$OUT_DIR/$BASELINE_LABEL"
CANDIDATE_DIR="$OUT_DIR/$CANDIDATE_LABEL"
COMPARE_DIR="$OUT_DIR/compare"
REPEATS_DIR="$OUT_DIR/repeats"
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
if [[ -n "$BASELINE_PERF_DYNAMIC_RESOLUTION" ]]; then
  BASELINE_ARGS+=(--perf-dynamic-resolution "$BASELINE_PERF_DYNAMIC_RESOLUTION")
fi
if [[ -n "$CANDIDATE_PERF_DYNAMIC_RESOLUTION" ]]; then
  CANDIDATE_ARGS+=(--perf-dynamic-resolution "$CANDIDATE_PERF_DYNAMIC_RESOLUTION")
fi

BASELINE_SUMMARY_CSV="$BASELINE_DIR/perf_summary.csv"
CANDIDATE_SUMMARY_CSV="$CANDIDATE_DIR/perf_summary.csv"

if [[ "$REPEAT_COUNT" -eq 1 ]]; then
  echo "[perf-compare] baseline label=$BASELINE_LABEL backend=$BASELINE_BACKEND out=$BASELINE_DIR"
  ./tests/perf/run_perf.sh --backend "$BASELINE_BACKEND" --out-dir "$BASELINE_DIR" "${COMMON_ARGS[@]}" "${BASELINE_ARGS[@]}"

  echo "[perf-compare] candidate label=$CANDIDATE_LABEL backend=$CANDIDATE_BACKEND out=$CANDIDATE_DIR"
  ./tests/perf/run_perf.sh --backend "$CANDIDATE_BACKEND" --out-dir "$CANDIDATE_DIR" "${COMMON_ARGS[@]}" "${CANDIDATE_ARGS[@]}"
else
  BASELINE_REPEAT_CSVS=()
  CANDIDATE_REPEAT_CSVS=()
  mkdir -p "$REPEATS_DIR"

  for run_idx in $(seq 1 "$REPEAT_COUNT"); do
    run_name="run_$(printf '%02d' "$run_idx")"
    base_run_dir="$REPEATS_DIR/$run_name/$BASELINE_LABEL"
    cand_run_dir="$REPEATS_DIR/$run_name/$CANDIDATE_LABEL"

    echo "[perf-compare] baseline repeat=$run_name label=$BASELINE_LABEL backend=$BASELINE_BACKEND out=$base_run_dir"
    ./tests/perf/run_perf.sh --backend "$BASELINE_BACKEND" --out-dir "$base_run_dir" "${COMMON_ARGS[@]}" "${BASELINE_ARGS[@]}"
    BASELINE_REPEAT_CSVS+=("$base_run_dir/perf_summary.csv")

    echo "[perf-compare] candidate repeat=$run_name label=$CANDIDATE_LABEL backend=$CANDIDATE_BACKEND out=$cand_run_dir"
    ./tests/perf/run_perf.sh --backend "$CANDIDATE_BACKEND" --out-dir "$cand_run_dir" "${COMMON_ARGS[@]}" "${CANDIDATE_ARGS[@]}"
    CANDIDATE_REPEAT_CSVS+=("$cand_run_dir/perf_summary.csv")
  done

  aggregate_perf_summary "$BASELINE_LABEL" "$BASELINE_DIR" "${BASELINE_REPEAT_CSVS[@]}"
  aggregate_perf_summary "$CANDIDATE_LABEL" "$CANDIDATE_DIR" "${CANDIDATE_REPEAT_CSVS[@]}"
fi

./tests/perf/compare_perf.sh \
  --baseline "$BASELINE_LABEL:$BASELINE_SUMMARY_CSV" \
  --candidate "$CANDIDATE_LABEL:$CANDIDATE_SUMMARY_CSV" \
  --out-dir "$COMPARE_DIR"

if [[ "$REPEAT_COUNT" -gt 1 ]]; then
  BASELINE_REPEATS_CSV="$BASELINE_DIR/perf_summary_repeats.csv"
  CANDIDATE_REPEATS_CSV="$CANDIDATE_DIR/perf_summary_repeats.csv"

  {
    echo
    echo "## Repeat Aggregation"
    echo
    echo "- Repeat count: $REPEAT_COUNT"
    echo "- Aggregate method: median per scene for baseline/candidate \`p95_frame_ms\`, \`avg_frame_ms\`, and \`max_rss_mb\`"
    echo "- Raw repeat dir: \`$REPEATS_DIR\`"
    echo "- Baseline aggregate summary: \`$BASELINE_SUMMARY_CSV\`"
    echo "- Candidate aggregate summary: \`$CANDIDATE_SUMMARY_CSV\`"
  } >> "$COMPARE_DIR/perf_compare.md"

  write_repeat_variability_report \
    "$BASELINE_LABEL" \
    "$BASELINE_REPEATS_CSV" \
    "$CANDIDATE_LABEL" \
    "$CANDIDATE_REPEATS_CSV" \
    "$COMPARE_DIR"

  {
    echo
    echo "## Repeat Variability"
    echo
    sed -n '2,$p' "$COMPARE_DIR/perf_repeat_variability.md"
  } >> "$COMPARE_DIR/perf_compare.md"
fi

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
