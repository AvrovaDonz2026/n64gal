#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

COMPARE_CSV=""
THRESHOLD_FILE="tests/perf/perf_thresholds.csv"
PROFILE=""
OUT_DIR=""
SOFT_FAIL=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --compare-csv)
      COMPARE_CSV="$2"
      shift 2
      ;;
    --threshold-file)
      THRESHOLD_FILE="$2"
      shift 2
      ;;
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    --soft-fail)
      SOFT_FAIL=1
      shift 1
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$COMPARE_CSV" || -z "$PROFILE" ]]; then
  echo "usage: ./tests/perf/check_kernel_thresholds.sh --compare-csv path/to/kernel_compare.csv --profile PROFILE [--threshold-file FILE] [--out-dir DIR] [--soft-fail]" >&2
  exit 2
fi
if [[ ! -f "$COMPARE_CSV" ]]; then
  echo "compare csv not found: $COMPARE_CSV" >&2
  exit 2
fi
if [[ ! -f "$THRESHOLD_FILE" ]]; then
  echo "threshold file not found: $THRESHOLD_FILE" >&2
  exit 2
fi

if [[ -z "$OUT_DIR" ]]; then
  OUT_DIR="$(dirname "$COMPARE_CSV")"
fi
mkdir -p "$OUT_DIR"

METRICS_CSV="$OUT_DIR/kernel_threshold_metrics.csv"
RESULTS_CSV="$OUT_DIR/kernel_threshold_results.csv"
REPORT_MD="$OUT_DIR/kernel_threshold_report.md"
PROFILE_TMP="$(mktemp)"
trap 'rm -f "$PROFILE_TMP"' EXIT

awk -F, -v profile="$PROFILE" '
  BEGIN { found = 0; }
  /^[[:space:]]*$/ { next; }
  /^#/ { next; }
  NR == 1 && $1 == "profile" { next; }
  $1 == profile {
    print $0;
    found = 1;
  }
  END {
    if (found == 0) {
      exit 3;
    }
  }
' "$THRESHOLD_FILE" > "$PROFILE_TMP" || {
  rc=$?
  if [[ "$rc" -eq 3 ]]; then
    echo "profile not found in threshold file: $PROFILE" >&2
    exit 2
  fi
  exit "$rc"
}

awk -F, '
  BEGIN {
    OFS = ",";
    print "scope,metric,value";
  }
  NR == 1 {
    for (i = 1; i <= NF; ++i) {
      idx[$i] = i;
    }
    next;
  }
  {
    kernel = $idx["kernel"];
    base_samples = $idx["baseline_samples"] + 0;
    cand_samples = $idx["candidate_samples"] + 0;
    base_avg = $idx["baseline_avg_ms"] + 0.0;
    cand_avg = $idx["candidate_avg_ms"] + 0.0;
    avg_speedup = $idx["avg_speedup"] + 0.0;
    avg_gain = $idx["avg_gain_pct"] + 0.0;
    base_p95 = $idx["baseline_p95_ms"] + 0.0;
    cand_p95 = $idx["candidate_p95_ms"] + 0.0;
    p95_speedup = $idx["p95_speedup"] + 0.0;
    p95_gain = $idx["p95_gain_pct"] + 0.0;
    base_mpix = $idx["baseline_mpix_per_s"] + 0.0;
    cand_mpix = $idx["candidate_mpix_per_s"] + 0.0;
    throughput_gain = $idx["throughput_gain_pct"] + 0.0;

    printf "%s,%s,%.6f\n", kernel, "baseline_samples", base_samples;
    printf "%s,%s,%.6f\n", kernel, "candidate_samples", cand_samples;
    printf "%s,%s,%.6f\n", kernel, "baseline_avg_ms", base_avg;
    printf "%s,%s,%.6f\n", kernel, "candidate_avg_ms", cand_avg;
    printf "%s,%s,%.6f\n", kernel, "avg_speedup", avg_speedup;
    printf "%s,%s,%.6f\n", kernel, "avg_gain_pct", avg_gain;
    printf "%s,%s,%.6f\n", kernel, "baseline_p95_ms", base_p95;
    printf "%s,%s,%.6f\n", kernel, "candidate_p95_ms", cand_p95;
    printf "%s,%s,%.6f\n", kernel, "p95_speedup", p95_speedup;
    printf "%s,%s,%.6f\n", kernel, "p95_gain_pct", p95_gain;
    printf "%s,%s,%.6f\n", kernel, "baseline_mpix_per_s", base_mpix;
    printf "%s,%s,%.6f\n", kernel, "candidate_mpix_per_s", cand_mpix;
    printf "%s,%s,%.6f\n", kernel, "throughput_gain_pct", throughput_gain;

    kernel_count += 1;
    sum_p95_gain += p95_gain;
    sum_avg_gain += avg_gain;
    if (kernel_count == 1 || p95_gain < min_p95_gain) {
      min_p95_gain = p95_gain;
    }
    if (kernel_count == 1 || cand_p95 > max_candidate_p95) {
      max_candidate_p95 = cand_p95;
    }
  }
  END {
    if (kernel_count == 0) {
      exit 4;
    }
    printf "%s,%s,%.6f\n", "__all__", "kernel_count", kernel_count;
    printf "%s,%s,%.6f\n", "__all__", "mean_p95_gain_pct", sum_p95_gain / kernel_count;
    printf "%s,%s,%.6f\n", "__all__", "mean_avg_gain_pct", sum_avg_gain / kernel_count;
    printf "%s,%s,%.6f\n", "__all__", "min_p95_gain_pct", min_p95_gain;
    printf "%s,%s,%.6f\n", "__all__", "max_candidate_p95_ms", max_candidate_p95;
  }
' "$COMPARE_CSV" > "$METRICS_CSV"

set +e
python3 - "$PROFILE" "$METRICS_CSV" "$PROFILE_TMP" "$RESULTS_CSV" "$REPORT_MD" <<'PY'
import csv
import sys
from pathlib import Path

profile, metrics_path, thresholds_path, results_path, report_path = sys.argv[1:]

metrics = {}
with open(metrics_path, newline='', encoding='utf-8') as f:
    reader = csv.DictReader(f)
    for row in reader:
        metrics[(row['scope'], row['metric'])] = float(row['value'])

results = []
with open(thresholds_path, newline='', encoding='utf-8') as f:
    reader = csv.reader(f)
    for row in reader:
        if not row:
            continue
        threshold_profile, scope, metric, op, value, description = row
        key = (scope, metric)
        actual = metrics.get(key)
        passed = False
        status = 'missing'
        if actual is not None:
            target = float(value)
            if op == '<=':
                passed = actual <= target
            elif op == '>=':
                passed = actual >= target
            elif op == '==':
                passed = actual == target
            else:
                raise SystemExit(f'unsupported op: {op}')
            status = 'pass' if passed else 'fail'
        results.append({
            'profile': threshold_profile,
            'scope': scope,
            'metric': metric,
            'op': op,
            'target': value,
            'actual': '' if actual is None else f'{actual:.6f}',
            'status': status,
            'description': description,
        })

with open(results_path, 'w', newline='', encoding='utf-8') as f:
    writer = csv.DictWriter(f, fieldnames=['profile','scope','metric','op','target','actual','status','description'])
    writer.writeheader()
    writer.writerows(results)

lines = [
    '# Kernel Threshold Report',
    '',
    f'- Profile: `{profile}`',
    f'- Metrics CSV: `{Path(metrics_path)}`',
    f'- Results CSV: `{Path(results_path)}`',
    '',
    '| scope | metric | op | target | actual | status | description |',
    '|---|---|---:|---:|---:|---|---|',
]
for row in results:
    actual = row['actual'] if row['actual'] else 'missing'
    lines.append(f"| {row['scope']} | {row['metric']} | {row['op']} | {row['target']} | {actual} | {row['status']} | {row['description']} |")
Path(report_path).write_text('\n'.join(lines) + '\n', encoding='utf-8')

bad = [row for row in results if row['status'] != 'pass']
raise SystemExit(1 if bad else 0)
PY
rc=$?
set -e
if [[ "$rc" -ne 0 && "$SOFT_FAIL" -eq 1 ]]; then
  echo "[kernel-threshold] soft-failed profile=$PROFILE compare=$COMPARE_CSV report=$REPORT_MD" >&2
  exit 0
fi
exit "$rc"
