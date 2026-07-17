#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_report}"
RELEASE_SPEC="${RELEASE_SPEC:-$ROOT_DIR/docs/release-publish-v1.1.0.json}"
BUNDLE_INDEX="${BUNDLE_INDEX:-$ROOT_DIR/build_release_bundle/release_bundle_index.md}"
BUNDLE_MANIFEST=""
GATE_SUMMARY="${GATE_SUMMARY:-$ROOT_DIR/build_release_gate/release_gate_summary.md}"
SOAK_SUMMARY="${SOAK_SUMMARY:-$ROOT_DIR/build_release_soak/demo_soak_summary.md}"
CONTENT_SOAK_SUMMARY=""
CONTENT_SOAK_SUMMARY_JSON=""
CI_SUITE_SUMMARY="${CI_SUITE_SUMMARY:-$ROOT_DIR/build_ci_cc/ci_suite_summary.md}"
HOST_SDK_SUMMARY="${HOST_SDK_SUMMARY:-$ROOT_DIR/build_release_host_sdk/host_sdk_smoke_summary.md}"
PLATFORM_EVIDENCE_SUMMARY="${PLATFORM_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_platform/platform_evidence_summary.md}"
PREVIEW_EVIDENCE_SUMMARY="${PREVIEW_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_preview/preview_evidence_summary.md}"
REPORT_OUT=""
REPORT_JSON_OUT=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_report.sh [--out-dir <dir>] [--release-spec <path>] [--bundle-index <path>] [--bundle-manifest <path>] [--gate-summary <path>] [--soak-summary <path>] [--content-soak-summary <path> --content-soak-summary-json <path>] [--ci-suite-summary <path>] [--host-sdk-summary <path>] [--platform-evidence-summary <path>] [--preview-evidence-summary <path>] [--report-out <path>] [--report-json-out <path>]
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
    --release-spec)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_SPEC="$1"
      shift
      ;;
    --bundle-index)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      BUNDLE_INDEX="$1"
      shift
      ;;
    --bundle-manifest)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      BUNDLE_MANIFEST="$1"
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
    --content-soak-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      CONTENT_SOAK_SUMMARY="$1"
      shift
      ;;
    --content-soak-summary-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      CONTENT_SOAK_SUMMARY_JSON="$1"
      shift
      ;;
    --ci-suite-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      CI_SUITE_SUMMARY="$1"
      shift
      ;;
    --host-sdk-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      HOST_SDK_SUMMARY="$1"
      shift
      ;;
    --platform-evidence-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PLATFORM_EVIDENCE_SUMMARY="$1"
      shift
      ;;
    --preview-evidence-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PREVIEW_EVIDENCE_SUMMARY="$1"
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

if [[ -n "$CONTENT_SOAK_SUMMARY" && -z "$CONTENT_SOAK_SUMMARY_JSON" ]] ||
   [[ -z "$CONTENT_SOAK_SUMMARY" && -n "$CONTENT_SOAK_SUMMARY_JSON" ]]; then
  echo "trace_id=release.report.content_soak.pair_required error_code=-1 error_name=VN_E_INVALID_ARG message=content soak markdown and JSON summaries must be provided together" >&2
  exit 2
fi

mkdir -p "$OUT_DIR"
if [[ -z "$BUNDLE_MANIFEST" ]]; then
  BUNDLE_MANIFEST="$(cd "$(dirname "$BUNDLE_INDEX")" && pwd)/release_bundle_manifest.json"
fi
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
require_file "$BUNDLE_MANIFEST"
require_file "$RELEASE_SPEC"
require_file "$GATE_SUMMARY"
require_file "$SOAK_SUMMARY"
if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
  require_file "$CONTENT_SOAK_SUMMARY"
  require_file "$CONTENT_SOAK_SUMMARY_JSON"
fi
require_file "$CI_SUITE_SUMMARY"
require_file "$HOST_SDK_SUMMARY"
require_file "$PLATFORM_EVIDENCE_SUMMARY"
require_file "$PREVIEW_EVIDENCE_SUMMARY"

eval "$(python3 tools/validate/release_spec.py --shell "$RELEASE_SPEC")"
if [[ "$SPEC_VERSION" == "v1.1.0" && -z "$CONTENT_SOAK_SUMMARY" ]]; then
  echo "trace_id=release.report.content_soak.required error_code=-1 error_name=VN_E_INVALID_ARG message=v1.1.0 report requires real-content soak markdown and JSON summaries" >&2
  exit 2
fi
if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
  CONTENT_PACK_PATH="$(python3 tools/validate/release_spec.py "$RELEASE_SPEC" --asset-path-for-name content-demo.vnpak)"
  python3 tools/validate/validate_content_soak_summary.py \
    "$CONTENT_SOAK_SUMMARY_JSON" --summary-md "$CONTENT_SOAK_SUMMARY" --pack "$CONTENT_PACK_PATH"
fi

require_file "$SPEC_RELEASE_NOTE"
require_file "$SPEC_RELEASE_EVIDENCE"
require_file "$SPEC_RELEASE_PACKAGE"
require_file "$SPEC_RELEASE_CHECKLIST"

{
  echo "# Release Report"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Release spec: \`$RELEASE_SPEC\`"
  echo "- Bundle index: \`$BUNDLE_INDEX\`"
  echo "- Bundle manifest: \`$BUNDLE_MANIFEST\`"
  echo "- Gate summary: \`$GATE_SUMMARY\`"
  echo "- Soak summary: \`$SOAK_SUMMARY\`"
  if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
    echo "- Content soak summary: \`$CONTENT_SOAK_SUMMARY\`"
    echo "- Content soak summary JSON: \`$CONTENT_SOAK_SUMMARY_JSON\`"
  fi
  echo "- CI suite summary: \`$CI_SUITE_SUMMARY\`"
  echo "- Host SDK summary: \`$HOST_SDK_SUMMARY\`"
  echo "- Platform evidence summary: \`$PLATFORM_EVIDENCE_SUMMARY\`"
  echo "- Preview evidence summary: \`$PREVIEW_EVIDENCE_SUMMARY\`"
  echo
  echo "## Core Evidence"
  echo
  echo "1. Release bundle index"
  echo "2. Release bundle manifest"
  echo "3. Release gate summary"
  echo "4. Demo soak summary"
  if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
    echo "5. Real-content soak markdown and JSON summaries"
  fi
  echo "6. CI suite summary"
  echo "7. Host SDK smoke summary"
  echo "8. Platform evidence summary"
  echo "9. Preview evidence summary"
  echo "10. Release note: \`$SPEC_RELEASE_NOTE\`"
  echo "11. Release evidence: \`$SPEC_RELEASE_EVIDENCE\`"
  echo "12. Release package: \`$SPEC_RELEASE_PACKAGE\`"
  echo "13. Release checklist: \`$SPEC_RELEASE_CHECKLIST\`"
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

python3 - "$REPORT_JSON_OUT" "$(git rev-parse --short HEAD)" "$(git branch --show-current)" \
  "$RELEASE_SPEC" "$REPORT_OUT" "$BUNDLE_INDEX" "$BUNDLE_MANIFEST" "$GATE_SUMMARY" \
  "$SOAK_SUMMARY" "$CONTENT_SOAK_SUMMARY" "$CONTENT_SOAK_SUMMARY_JSON" \
  "$CI_SUITE_SUMMARY" "$HOST_SDK_SUMMARY" "$PLATFORM_EVIDENCE_SUMMARY" \
  "$PREVIEW_EVIDENCE_SUMMARY" "$SPEC_RELEASE_NOTE" "$SPEC_RELEASE_EVIDENCE" \
  "$SPEC_RELEASE_PACKAGE" "$SPEC_RELEASE_CHECKLIST" <<'PY'
import json
import sys
from pathlib import Path

(
    output_path, head, branch, release_spec, report_md, bundle_index,
    bundle_manifest, gate_summary, soak_summary, content_soak_summary,
    content_soak_summary_json, ci_suite_summary, host_sdk_summary,
    platform_evidence_summary, preview_evidence_summary, release_note,
    release_evidence, release_package, release_checklist,
) = sys.argv[1:]
payload = {
    "head": head,
    "branch": branch,
    "release_spec": release_spec,
    "report_md": report_md,
    "bundle_index": bundle_index,
    "bundle_manifest": bundle_manifest,
    "gate_summary": gate_summary,
    "soak_summary": soak_summary,
    "content_soak_summary": content_soak_summary,
    "content_soak_summary_json": content_soak_summary_json,
    "ci_suite_summary": ci_suite_summary,
    "host_sdk_summary": host_sdk_summary,
    "platform_evidence_summary": platform_evidence_summary,
    "preview_evidence_summary": preview_evidence_summary,
    "release_note": release_note,
    "release_evidence": release_evidence,
    "release_package": release_package,
    "release_checklist": release_checklist,
}
Path(output_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY

echo "trace_id=release.report.ok report=$REPORT_OUT report_json=$REPORT_JSON_OUT bundle_index=$BUNDLE_INDEX bundle_manifest=$BUNDLE_MANIFEST gate_summary=$GATE_SUMMARY soak_summary=$SOAK_SUMMARY content_soak_summary=$CONTENT_SOAK_SUMMARY content_soak_summary_json=$CONTENT_SOAK_SUMMARY_JSON ci_summary=$CI_SUITE_SUMMARY host_sdk_summary=$HOST_SDK_SUMMARY platform_summary=$PLATFORM_EVIDENCE_SUMMARY preview_summary=$PREVIEW_EVIDENCE_SUMMARY"
