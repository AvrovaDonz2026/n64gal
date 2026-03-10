#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BASELINE_SPEC=""
CANDIDATE_SPEC=""
OUT_DIR="tests/perf/kernel_compare"

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
  echo "usage: ./tests/perf/compare_kernel_bench.sh --baseline label:path/to/kernel.csv --candidate label:path/to/kernel.csv [--out-dir DIR]" >&2
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

BASE_KERNELS_TMP="$(mktemp)"
CAND_KERNELS_TMP="$(mktemp)"
trap 'rm -f "$BASE_KERNELS_TMP" "$CAND_KERNELS_TMP"' EXIT

kernel_sets_equal() {
  local left_file="$1"
  local right_file="$2"

  awk 'NR == FNR {
    left[FNR] = $0;
    left_count = FNR;
    next;
  }
  {
    if (FNR > left_count || $0 != left[FNR]) {
      exit 1;
    }
  }
  END {
    if (FNR != left_count) {
      exit 1;
    }
    exit 0;
  }' "$left_file" "$right_file"
}

tail -n +2 "$BASELINE_CSV" | cut -d, -f1 | sort -u > "$BASE_KERNELS_TMP"
tail -n +2 "$CANDIDATE_CSV" | cut -d, -f1 | sort -u > "$CAND_KERNELS_TMP"

if ! kernel_sets_equal "$BASE_KERNELS_TMP" "$CAND_KERNELS_TMP"; then
  echo "kernel sets differ between baseline and candidate" >&2
  echo "[baseline kernels]" >&2
  cat "$BASE_KERNELS_TMP" >&2
  echo "[candidate kernels]" >&2
  cat "$CAND_KERNELS_TMP" >&2
  exit 1
fi

COMPARE_CSV="$OUT_DIR/kernel_compare.csv"
COMPARE_MD="$OUT_DIR/kernel_compare.md"

csv_field() {
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

BASELINE_HOST_CPU="$(csv_field "$BASELINE_CSV" "host_cpu")"
CANDIDATE_HOST_CPU="$(csv_field "$CANDIDATE_CSV" "host_cpu")"
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
     print "kernel,baseline_label,candidate_label,baseline_samples,candidate_samples,baseline_avg_ms,candidate_avg_ms,avg_speedup,avg_gain_pct,baseline_p95_ms,candidate_p95_ms,p95_speedup,p95_gain_pct,baseline_mpix_per_s,candidate_mpix_per_s,throughput_gain_pct";
   }
   NR == FNR {
     if (FNR == 1) {
       next;
     }
     base_kernel[$1] = $0;
     next;
   }
   FNR == 1 {
     next;
   }
   {
     split(base_kernel[$1], base_fields, ",");
     base_samples = base_fields[3] + 0;
     base_avg = base_fields[8] + 0.0;
     base_p95 = base_fields[9] + 0.0;
     base_mpix = base_fields[12] + 0.0;
     cand_samples = $3 + 0;
     cand_avg = $8 + 0.0;
     cand_p95 = $9 + 0.0;
     cand_mpix = $12 + 0.0;
     avg_speedup = (cand_avg > 0.0 ? base_avg / cand_avg : 0.0);
     avg_gain = (base_avg > 0.0 ? ((base_avg - cand_avg) / base_avg) * 100.0 : 0.0);
     p95_speedup = (cand_p95 > 0.0 ? base_p95 / cand_p95 : 0.0);
     p95_gain = (base_p95 > 0.0 ? ((base_p95 - cand_p95) / base_p95) * 100.0 : 0.0);
     throughput_gain = (base_mpix > 0.0 ? ((cand_mpix - base_mpix) / base_mpix) * 100.0 : 0.0);
     printf "%s,%s,%s,%d,%d,%.6f,%.6f,%.3f,%.2f,%.6f,%.6f,%.3f,%.2f,%.6f,%.6f,%.2f\n",
            $1,
            base_label,
            cand_label,
            base_samples,
            cand_samples,
            base_avg,
            cand_avg,
            avg_speedup,
            avg_gain,
            base_p95,
            cand_p95,
            p95_speedup,
            p95_gain,
            base_mpix,
            cand_mpix,
            throughput_gain;
   }' "$BASELINE_CSV" "$CANDIDATE_CSV" > "$COMPARE_CSV"

KERNEL_COUNT="$(awk -F, 'NR > 1 { n += 1 } END { print n + 0 }' "$COMPARE_CSV")"
MEAN_AVG_SPEEDUP="$(awk -F, 'NR > 1 { sum += $8; n += 1 } END { if (n == 0) printf "0.000"; else printf "%.3f", sum / n }' "$COMPARE_CSV")"
MEAN_AVG_GAIN="$(awk -F, 'NR > 1 { sum += $9; n += 1 } END { if (n == 0) printf "0.00"; else printf "%.2f", sum / n }' "$COMPARE_CSV")"
MEAN_P95_SPEEDUP="$(awk -F, 'NR > 1 { sum += $12; n += 1 } END { if (n == 0) printf "0.000"; else printf "%.3f", sum / n }' "$COMPARE_CSV")"
MEAN_P95_GAIN="$(awk -F, 'NR > 1 { sum += $13; n += 1 } END { if (n == 0) printf "0.00"; else printf "%.2f", sum / n }' "$COMPARE_CSV")"
MEAN_THROUGHPUT_GAIN="$(awk -F, 'NR > 1 { sum += $16; n += 1 } END { if (n == 0) printf "0.00"; else printf "%.2f", sum / n }' "$COMPARE_CSV")"

{
  echo "# N64GAL Kernel Bench Compare"
  echo
  echo "- Baseline: \`$BASELINE_LABEL\`"
  echo "- Candidate: \`$CANDIDATE_LABEL\`"
  echo "- Baseline kernel csv: \`$BASELINE_CSV\`"
  echo "- Candidate kernel csv: \`$CANDIDATE_CSV\`"
  echo "- Baseline host CPU: \`$BASELINE_HOST_CPU\`"
  echo "- Candidate host CPU: \`$CANDIDATE_HOST_CPU\`"
  echo "- Kernels compared: $KERNEL_COUNT"
  echo "- Mean avg speedup: ${MEAN_AVG_SPEEDUP}x"
  echo "- Mean avg gain: ${MEAN_AVG_GAIN}%"
  echo "- Mean p95 speedup: ${MEAN_P95_SPEEDUP}x"
  echo "- Mean p95 gain: ${MEAN_P95_GAIN}%"
  echo "- Mean throughput gain: ${MEAN_THROUGHPUT_GAIN}%"
  echo
  echo "Positive avg/p95 gain means the candidate is faster than the baseline. Positive throughput gain means the candidate produced more megapixels per second."
  echo
  echo "| kernel | ${BASELINE_LABEL} avg ms | ${CANDIDATE_LABEL} avg ms | avg speedup | avg gain | ${BASELINE_LABEL} p95 ms | ${CANDIDATE_LABEL} p95 ms | p95 speedup | p95 gain | ${BASELINE_LABEL} mpix/s | ${CANDIDATE_LABEL} mpix/s | throughput gain |"
  echo "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|"
  tail -n +2 "$COMPARE_CSV" | awk -F, '{
    printf "| %s | %.6f | %.6f | %.3fx | %.2f%% | %.6f | %.6f | %.3fx | %.2f%% | %.6f | %.6f | %.2f%% |\n",
           $1,
           $6 + 0.0,
           $7 + 0.0,
           $8 + 0.0,
           $9 + 0.0,
           $10 + 0.0,
           $11 + 0.0,
           $12 + 0.0,
           $13 + 0.0,
           $14 + 0.0,
           $15 + 0.0,
           $16 + 0.0;
  }'
} > "$COMPARE_MD"

echo "[kernel-compare] wrote $COMPARE_CSV"
echo "[kernel-compare] wrote $COMPARE_MD"
