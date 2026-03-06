#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

COMPARE_CSV=""
THRESHOLD_FILE="tests/perf/perf_thresholds.csv"
PROFILE=""
OUT_DIR=""

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
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$COMPARE_CSV" || -z "$PROFILE" ]]; then
  echo "usage: ./tests/perf/check_perf_thresholds.sh --compare-csv path/to/perf_compare.csv --profile PROFILE [--threshold-file FILE] [--out-dir DIR]" >&2
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

METRICS_CSV="$OUT_DIR/perf_threshold_metrics.csv"
RESULTS_CSV="$OUT_DIR/perf_threshold_results.csv"
REPORT_MD="$OUT_DIR/perf_threshold_report.md"
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
    scene = $idx["scene"];
    base_samples = $idx["baseline_samples"] + 0;
    cand_samples = $idx["candidate_samples"] + 0;
    base_p95 = $idx["baseline_p95_ms"] + 0.0;
    cand_p95 = $idx["candidate_p95_ms"] + 0.0;
    p95_speedup = $idx["p95_speedup"] + 0.0;
    p95_gain = $idx["p95_gain_pct"] + 0.0;
    base_avg = $idx["baseline_avg_ms"] + 0.0;
    cand_avg = $idx["candidate_avg_ms"] + 0.0;
    avg_speedup = $idx["avg_speedup"] + 0.0;
    avg_gain = $idx["avg_gain_pct"] + 0.0;
    base_rss = $idx["baseline_max_rss_mb"] + 0.0;
    cand_rss = $idx["candidate_max_rss_mb"] + 0.0;
    rss_delta = $idx["rss_delta_mb"] + 0.0;

    printf "%s,%s,%.6f\n", scene, "baseline_samples", base_samples;
    printf "%s,%s,%.6f\n", scene, "candidate_samples", cand_samples;
    printf "%s,%s,%.6f\n", scene, "baseline_p95_ms", base_p95;
    printf "%s,%s,%.6f\n", scene, "candidate_p95_ms", cand_p95;
    printf "%s,%s,%.6f\n", scene, "p95_speedup", p95_speedup;
    printf "%s,%s,%.6f\n", scene, "p95_gain_pct", p95_gain;
    printf "%s,%s,%.6f\n", scene, "baseline_avg_ms", base_avg;
    printf "%s,%s,%.6f\n", scene, "candidate_avg_ms", cand_avg;
    printf "%s,%s,%.6f\n", scene, "avg_speedup", avg_speedup;
    printf "%s,%s,%.6f\n", scene, "avg_gain_pct", avg_gain;
    printf "%s,%s,%.6f\n", scene, "baseline_max_rss_mb", base_rss;
    printf "%s,%s,%.6f\n", scene, "candidate_max_rss_mb", cand_rss;
    printf "%s,%s,%.6f\n", scene, "rss_delta_mb", rss_delta;

    scene_count += 1;
    sum_p95_gain += p95_gain;
    sum_avg_gain += avg_gain;
    sum_p95_speedup += p95_speedup;
    sum_avg_speedup += avg_speedup;
    if (scene_count == 1 || p95_gain < min_p95_gain) {
      min_p95_gain = p95_gain;
    }
    if (scene_count == 1 || avg_gain < min_avg_gain) {
      min_avg_gain = avg_gain;
    }
    if (scene_count == 1 || cand_p95 > max_candidate_p95) {
      max_candidate_p95 = cand_p95;
    }
    if (scene_count == 1 || cand_avg > max_candidate_avg) {
      max_candidate_avg = cand_avg;
    }
    if (scene_count == 1 || rss_delta > max_rss_delta) {
      max_rss_delta = rss_delta;
    }
    if (scene_count == 1 || cand_rss > max_candidate_rss) {
      max_candidate_rss = cand_rss;
    }
  }
  END {
    if (scene_count == 0) {
      exit 4;
    }
    printf "%s,%s,%.6f\n", "__all__", "scene_count", scene_count;
    printf "%s,%s,%.6f\n", "__all__", "mean_p95_gain_pct", sum_p95_gain / scene_count;
    printf "%s,%s,%.6f\n", "__all__", "mean_avg_gain_pct", sum_avg_gain / scene_count;
    printf "%s,%s,%.6f\n", "__all__", "mean_p95_speedup", sum_p95_speedup / scene_count;
    printf "%s,%s,%.6f\n", "__all__", "mean_avg_speedup", sum_avg_speedup / scene_count;
    printf "%s,%s,%.6f\n", "__all__", "min_p95_gain_pct", min_p95_gain;
    printf "%s,%s,%.6f\n", "__all__", "min_avg_gain_pct", min_avg_gain;
    printf "%s,%s,%.6f\n", "__all__", "max_candidate_p95_ms", max_candidate_p95;
    printf "%s,%s,%.6f\n", "__all__", "max_candidate_avg_ms", max_candidate_avg;
    printf "%s,%s,%.6f\n", "__all__", "max_rss_delta_mb", max_rss_delta;
    printf "%s,%s,%.6f\n", "__all__", "max_candidate_max_rss_mb", max_candidate_rss;
  }
' "$COMPARE_CSV" > "$METRICS_CSV"

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
fail_count = 0
with open(thresholds_path, newline='', encoding='utf-8') as f:
    reader = csv.reader(f)
    for row in reader:
        if not row:
            continue
        if row[0].startswith('#'):
            continue
        if len(row) < 6:
            raise SystemExit(f'invalid threshold row: {row!r}')
        profile_name, scope, metric, op, expected_text, description = row[:6]
        expected = float(expected_text)
        actual = metrics.get((scope, metric))
        status = 'PASS'
        if actual is None:
            status = 'MISSING'
        elif op == '<=':
            status = 'PASS' if actual <= expected else 'FAIL'
        elif op == '>=':
            status = 'PASS' if actual >= expected else 'FAIL'
        elif op == '==':
            status = 'PASS' if actual == expected else 'FAIL'
        else:
            raise SystemExit(f'unsupported operator: {op}')
        if status != 'PASS':
            fail_count += 1
        results.append({
            'profile': profile_name,
            'scope': scope,
            'metric': metric,
            'op': op,
            'expected': expected,
            'actual': actual,
            'status': status,
            'description': description,
        })

with open(results_path, 'w', newline='', encoding='utf-8') as f:
    writer = csv.writer(f)
    writer.writerow(['profile', 'scope', 'metric', 'op', 'expected', 'actual', 'status', 'description'])
    for item in results:
        actual_text = '' if item['actual'] is None else f"{item['actual']:.6f}"
        writer.writerow([
            item['profile'],
            item['scope'],
            item['metric'],
            item['op'],
            f"{item['expected']:.6f}",
            actual_text,
            item['status'],
            item['description'],
        ])

pass_count = sum(1 for item in results if item['status'] == 'PASS')
with open(report_path, 'w', encoding='utf-8') as f:
    f.write('# N64GAL Perf Threshold Report\n\n')
    f.write(f'- Profile: `{profile}`\n')
    f.write(f'- Checks: {len(results)}\n')
    f.write(f'- Passed: {pass_count}\n')
    f.write(f'- Failed: {fail_count}\n\n')
    f.write('| status | scope | metric | op | expected | actual | description |\n')
    f.write('|---|---|---|---:|---:|---:|---|\n')
    for item in results:
        actual_text = 'missing' if item['actual'] is None else f"{item['actual']:.3f}"
        f.write(
            f"| {item['status']} | {item['scope']} | {item['metric']} | {item['op']} | {item['expected']:.3f} | {actual_text} | {item['description']} |\n"
        )

if fail_count != 0:
    raise SystemExit(1)
PY

rc=$?
if [[ "$rc" -ne 0 ]]; then
  echo "[perf-threshold] profile=$PROFILE FAILED report=$REPORT_MD" >&2
  exit "$rc"
fi

echo "[perf-threshold] profile=$PROFILE passed results=$RESULTS_CSV"
echo "[perf-threshold] report=$REPORT_MD"
