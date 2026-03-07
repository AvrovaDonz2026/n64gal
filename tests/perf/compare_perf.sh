#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BASELINE_SPEC=""
CANDIDATE_SPEC=""
OUT_DIR="tests/perf/compare"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --baseline)
      BASELINE_SPEC="$2"
      shift 2
      ;;
    --candidate)
      CANDIDATE_SPEC="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$BASELINE_SPEC" || -z "$CANDIDATE_SPEC" ]]; then
  echo "usage: ./tests/perf/compare_perf.sh --baseline label:path/to/perf_summary.csv --candidate label:path/to/perf_summary.csv [--out-dir DIR]" >&2
  exit 2
fi

BASELINE_LABEL="${BASELINE_SPEC%%:*}"
BASELINE_CSV="${BASELINE_SPEC#*:}"
CANDIDATE_LABEL="${CANDIDATE_SPEC%%:*}"
CANDIDATE_CSV="${CANDIDATE_SPEC#*:}"

if [[ -z "$BASELINE_LABEL" || -z "$BASELINE_CSV" || "$BASELINE_LABEL" == "$BASELINE_CSV" ]]; then
  echo "invalid baseline spec: $BASELINE_SPEC" >&2
  exit 2
fi
if [[ -z "$CANDIDATE_LABEL" || -z "$CANDIDATE_CSV" || "$CANDIDATE_LABEL" == "$CANDIDATE_CSV" ]]; then
  echo "invalid candidate spec: $CANDIDATE_SPEC" >&2
  exit 2
fi
if [[ ! -f "$BASELINE_CSV" ]]; then
  echo "baseline csv not found: $BASELINE_CSV" >&2
  exit 2
fi
if [[ ! -f "$CANDIDATE_CSV" ]]; then
  echo "candidate csv not found: $CANDIDATE_CSV" >&2
  exit 2
fi

mkdir -p "$OUT_DIR"

BASE_SCENES_TMP="$(mktemp)"
CAND_SCENES_TMP="$(mktemp)"
trap 'rm -f "$BASE_SCENES_TMP" "$CAND_SCENES_TMP"' EXIT

tail -n +2 "$BASELINE_CSV" | cut -d, -f1 | sort -u > "$BASE_SCENES_TMP"
tail -n +2 "$CANDIDATE_CSV" | cut -d, -f1 | sort -u > "$CAND_SCENES_TMP"

if ! cmp -s "$BASE_SCENES_TMP" "$CAND_SCENES_TMP"; then
  echo "scene sets differ between baseline and candidate" >&2
  diff -u "$BASE_SCENES_TMP" "$CAND_SCENES_TMP" >&2 || true
  exit 1
fi

COMPARE_CSV="$OUT_DIR/perf_compare.csv"
COMPARE_MD="$OUT_DIR/perf_compare.md"

summary_field() {
  local csv_path="$1"
  local field_name="$2"

  awk -F, -v field_name="$field_name" '
    NR == 1 {
      for (i = 1; i <= NF; ++i) {
        idx[$i] = i;
      }
      next;
    }
    NR > 1 {
      if (idx[field_name] > 0) {
        print $idx[field_name];
      }
      exit;
    }
  ' "$csv_path"
}

BASELINE_HOST_CPU="$(summary_field "$BASELINE_CSV" "host_cpu")"
CANDIDATE_HOST_CPU="$(summary_field "$CANDIDATE_CSV" "host_cpu")"
if [[ -z "$BASELINE_HOST_CPU" ]]; then
  BASELINE_HOST_CPU="unknown"
fi
if [[ -z "$CANDIDATE_HOST_CPU" ]]; then
  CANDIDATE_HOST_CPU="unknown"
fi

awk -F, \
  -v base_label="$BASELINE_LABEL" \
  -v cand_label="$CANDIDATE_LABEL" \
  'BEGIN {
     OFS = ",";
     print "scene,baseline_label,candidate_label,baseline_samples,candidate_samples,baseline_p95_ms,candidate_p95_ms,p95_speedup,p95_gain_pct,baseline_avg_ms,candidate_avg_ms,avg_speedup,avg_gain_pct,baseline_max_rss_mb,candidate_max_rss_mb,rss_delta_mb";
   }
   NR == FNR {
     if (FNR == 1) {
       next;
     }
     base_scene[$1] = $0;
     next;
   }
   FNR == 1 {
     next;
   }
   {
     split(base_scene[$1], base_fields, ",");
     base_samples = base_fields[2] + 0;
     base_p95 = base_fields[3] + 0.0;
     base_avg = base_fields[4] + 0.0;
     base_rss = base_fields[5] + 0.0;
     cand_samples = $2 + 0;
     cand_p95 = $3 + 0.0;
     cand_avg = $4 + 0.0;
     cand_rss = $5 + 0.0;
     p95_speedup = (cand_p95 > 0.0 ? base_p95 / cand_p95 : 0.0);
     avg_speedup = (cand_avg > 0.0 ? base_avg / cand_avg : 0.0);
     p95_gain = (base_p95 > 0.0 ? ((base_p95 - cand_p95) / base_p95) * 100.0 : 0.0);
     avg_gain = (base_avg > 0.0 ? ((base_avg - cand_avg) / base_avg) * 100.0 : 0.0);
     rss_delta = cand_rss - base_rss;
     printf "%s,%s,%s,%d,%d,%.3f,%.3f,%.3f,%.2f,%.3f,%.3f,%.3f,%.2f,%.3f,%.3f,%.3f\n",
            $1,
            base_label,
            cand_label,
            base_samples,
            cand_samples,
            base_p95,
            cand_p95,
            p95_speedup,
            p95_gain,
            base_avg,
            cand_avg,
            avg_speedup,
            avg_gain,
            base_rss,
            cand_rss,
            rss_delta;
   }' "$BASELINE_CSV" "$CANDIDATE_CSV" > "$COMPARE_CSV"

SCENE_COUNT="$(awk -F, 'NR > 1 { n += 1 } END { print n + 0 }' "$COMPARE_CSV")"
MEAN_P95_SPEEDUP="$(awk -F, 'NR > 1 { sum += $8; n += 1 } END { if (n == 0) printf "0.000"; else printf "%.3f", sum / n }' "$COMPARE_CSV")"
MEAN_AVG_SPEEDUP="$(awk -F, 'NR > 1 { sum += $12; n += 1 } END { if (n == 0) printf "0.000"; else printf "%.3f", sum / n }' "$COMPARE_CSV")"
MEAN_P95_GAIN="$(awk -F, 'NR > 1 { sum += $9; n += 1 } END { if (n == 0) printf "0.00"; else printf "%.2f", sum / n }' "$COMPARE_CSV")"
MEAN_AVG_GAIN="$(awk -F, 'NR > 1 { sum += $13; n += 1 } END { if (n == 0) printf "0.00"; else printf "%.2f", sum / n }' "$COMPARE_CSV")"

{
  echo "# N64GAL Perf Compare"
  echo
  echo "- Baseline: \`$BASELINE_LABEL\`"
  echo "- Candidate: \`$CANDIDATE_LABEL\`"
  echo "- Baseline summary: \`$BASELINE_CSV\`"
  echo "- Candidate summary: \`$CANDIDATE_CSV\`"
  echo "- Baseline host CPU: \`$BASELINE_HOST_CPU\`"
  echo "- Candidate host CPU: \`$CANDIDATE_HOST_CPU\`"
  echo "- Scenes compared: $SCENE_COUNT"
  echo "- Mean p95 speedup: ${MEAN_P95_SPEEDUP}x"
  echo "- Mean avg speedup: ${MEAN_AVG_SPEEDUP}x"
  echo "- Mean p95 gain: ${MEAN_P95_GAIN}%"
  echo "- Mean avg gain: ${MEAN_AVG_GAIN}%"
  echo
  echo "Positive gain means the candidate is faster than the baseline. Negative rss delta means the candidate used less peak RSS."
  echo
  echo "| scene | ${BASELINE_LABEL} p95 ms | ${CANDIDATE_LABEL} p95 ms | p95 speedup | p95 gain | ${BASELINE_LABEL} avg ms | ${CANDIDATE_LABEL} avg ms | avg speedup | avg gain | rss delta mb |"
  echo "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|"
  tail -n +2 "$COMPARE_CSV" | awk -F, '{
    printf "| %s | %.3f | %.3f | %.3fx | %.2f%% | %.3f | %.3f | %.3fx | %.2f%% | %.3f |\n",
           $1,
           $6 + 0.0,
           $7 + 0.0,
           $8 + 0.0,
           $9 + 0.0,
           $10 + 0.0,
           $11 + 0.0,
           $12 + 0.0,
           $13 + 0.0,
           $16 + 0.0;
  }'
} > "$COMPARE_MD"

echo "[perf-compare] wrote $COMPARE_CSV"
echo "[perf-compare] wrote $COMPARE_MD"
