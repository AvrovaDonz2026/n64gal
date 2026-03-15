#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_report}"
BUNDLE_INDEX="${BUNDLE_INDEX:-$ROOT_DIR/build_release_bundle/release_bundle_index.md}"
GATE_SUMMARY="${GATE_SUMMARY:-$ROOT_DIR/build_release_gate/release_gate_summary.md}"
SOAK_SUMMARY="${SOAK_SUMMARY:-$ROOT_DIR/build_release_soak/demo_soak_summary.md}"
CI_SUITE_SUMMARY="${CI_SUITE_SUMMARY:-$ROOT_DIR/build_ci_cc/ci_suite_summary.md}"
REPORT_OUT=""
REPORT_JSON_OUT=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_report.sh [--out-dir <dir>] [--bundle-index <path>] [--gate-summary <path>] [--soak-summary <path>] [--ci-suite-summary <path>] [--report-out <path>] [--report-json-out <path>]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out-dir)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      OUT_DIR="$1"
      shift
      ;;
    --bundle-index)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      BUNDLE_INDEX="$1"
      shift
      ;;
    --gate-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      GATE_SUMMARY="$1"
      shift
      ;;
    --soak-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      SOAK_SUMMARY="$1"
      shift
      ;;
    --ci-suite-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      CI_SUITE_SUMMARY="$1"
      shift
      ;;
    --report-out)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      REPORT_OUT="$1"
      shift
      ;;
    --report-json-out)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      REPORT_JSON_OUT="$1"
      shift
      ;;
    -h|--help)
      usage
      exit 2
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

mkdir -p "$OUT_DIR"
if [[ -z "$REPORT_OUT" ]]; then
  REPORT_OUT="$OUT_DIR/release_report.md"
fi
if [[ -z "$REPORT_JSON_OUT" ]]; then
  REPORT_JSON_OUT="$OUT_DIR/release_report.json"
fi

require_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "trace_id=release.report.missing error_code=-2 error_name=VN_E_IO path=$path message=required release report input missing" >&2
    exit 1
  fi
}

require_file "$BUNDLE_INDEX"
require_file "$GATE_SUMMARY"
require_file "$SOAK_SUMMARY"
require_file "$CI_SUITE_SUMMARY"

{
  echo "# Release Report"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Bundle index: \`$BUNDLE_INDEX\`"
  echo "- Gate summary: \`$GATE_SUMMARY\`"
  echo "- Soak summary: \`$SOAK_SUMMARY\`"
  echo "- CI suite summary: \`$CI_SUITE_SUMMARY\`"
  echo
  echo "## Core Evidence"
  echo
  echo "1. Release bundle index"
  echo "2. Release gate summary"
  echo "3. Demo soak summary"
  echo "4. CI suite summary"
  echo "5. Release note / evidence / package docs"
  echo
  echo "## Perf Evidence Docs"
  echo
  echo "1. \`docs/perf-report.md\`"
  echo "2. \`docs/perf-dirty-2026-03-07.md\`"
  echo "3. \`docs/perf-dynres-2026-03-07.md\`"
  echo "4. \`docs/perf-windows-x64-2026-03-07.md\`"
  echo "5. \`docs/perf-x64-hosts-2026-03-09.md\`"
  echo "6. \`docs/perf-rvv-2026-03-06.md\`"
  echo
  echo "## Notes"
  echo
  echo "1. This report is an index/export layer; it does not replace the underlying summaries."
  echo "2. For a formal release, pair this with \`python3 tools/toolchain.py validate-all\`, \`release-gate\`, and \`release-soak\`."
} >"$REPORT_OUT"

{
  printf '{\n'
  printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
  printf '  "branch": "%s",\n' "$(git branch --show-current)"
  printf '  "report_md": "%s",\n' "$REPORT_OUT"
  printf '  "bundle_index": "%s",\n' "$BUNDLE_INDEX"
  printf '  "gate_summary": "%s",\n' "$GATE_SUMMARY"
  printf '  "soak_summary": "%s",\n' "$SOAK_SUMMARY"
  printf '  "ci_suite_summary": "%s"\n' "$CI_SUITE_SUMMARY"
  printf '}\n'
} >"$REPORT_JSON_OUT"

echo "trace_id=release.report.ok report=$REPORT_OUT report_json=$REPORT_JSON_OUT bundle_index=$BUNDLE_INDEX gate_summary=$GATE_SUMMARY soak_summary=$SOAK_SUMMARY ci_summary=$CI_SUITE_SUMMARY"
