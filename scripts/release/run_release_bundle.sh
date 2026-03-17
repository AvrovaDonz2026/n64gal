#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_bundle}"
RELEASE_GATE_SUMMARY="${RELEASE_GATE_SUMMARY:-$ROOT_DIR/build_release_gate/release_gate_summary.md}"
DEMO_SOAK_SUMMARY="${DEMO_SOAK_SUMMARY:-$ROOT_DIR/build_release_gate/demo_soak_summary.md}"
CI_SUITE_SUMMARY="${CI_SUITE_SUMMARY:-$ROOT_DIR/build_ci_cc/ci_suite_summary.md}"
HOST_SDK_SUMMARY="${HOST_SDK_SUMMARY:-$ROOT_DIR/build_release_host_sdk/host_sdk_smoke_summary.md}"
HOST_SDK_SUMMARY_JSON="${HOST_SDK_SUMMARY_JSON:-$ROOT_DIR/build_release_host_sdk/host_sdk_smoke_summary.json}"
PLATFORM_EVIDENCE_SUMMARY="${PLATFORM_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_platform/platform_evidence_summary.md}"
PLATFORM_EVIDENCE_SUMMARY_JSON="${PLATFORM_EVIDENCE_SUMMARY_JSON:-$ROOT_DIR/build_release_platform/platform_evidence_summary.json}"
PREVIEW_EVIDENCE_SUMMARY="${PREVIEW_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_preview/preview_evidence_summary.md}"
PREVIEW_EVIDENCE_SUMMARY_JSON="${PREVIEW_EVIDENCE_SUMMARY_JSON:-$ROOT_DIR/build_release_preview/preview_evidence_summary.json}"

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_bundle.sh [--out-dir <dir>] [--release-gate-summary <path>|--gate-summary <path>] [--demo-soak-summary <path>|--soak-summary <path>] [--ci-suite-summary <path>|--ci-summary <path>] [--host-sdk-summary <path>] [--host-sdk-summary-json <path>] [--platform-evidence-summary <path>] [--platform-evidence-summary-json <path>] [--preview-evidence-summary <path>] [--preview-evidence-summary-json <path>]
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
    --release-gate-summary|--gate-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_GATE_SUMMARY="$1"
      shift
      ;;
    --demo-soak-summary|--soak-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      DEMO_SOAK_SUMMARY="$1"
      shift
      ;;
    --ci-suite-summary|--ci-summary)
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
    --host-sdk-summary-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      HOST_SDK_SUMMARY_JSON="$1"
      shift
      ;;
    --platform-evidence-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PLATFORM_EVIDENCE_SUMMARY="$1"
      shift
      ;;
    --platform-evidence-summary-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PLATFORM_EVIDENCE_SUMMARY_JSON="$1"
      shift
      ;;
    --preview-evidence-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PREVIEW_EVIDENCE_SUMMARY="$1"
      shift
      ;;
    --preview-evidence-summary-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PREVIEW_EVIDENCE_SUMMARY_JSON="$1"
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
DOC_DIR="$OUT_DIR/docs"
SUM_DIR="$OUT_DIR/summaries"
mkdir -p "$DOC_DIR" "$SUM_DIR"

copy_required() {
  local src="$1"
  local dst="$2"
  if [[ ! -f "$src" ]]; then
    echo "trace_id=release.bundle.missing error_code=-2 error_name=VN_E_IO path=$src message=required bundle input missing" >&2
    exit 1
  fi
  cp "$src" "$dst"
}

copy_required "docs/release-v0.1.0-alpha.md" "$DOC_DIR/release-v0.1.0-alpha.md"
copy_required "docs/release-evidence-v0.1.0-alpha.md" "$DOC_DIR/release-evidence-v0.1.0-alpha.md"
copy_required "docs/release-package-v0.1.0-alpha.md" "$DOC_DIR/release-package-v0.1.0-alpha.md"
copy_required "docs/release-checklist-v1.0.0.md" "$DOC_DIR/release-checklist-v1.0.0.md"
copy_required "README.md" "$OUT_DIR/README.md"
copy_required "CHANGELOG.md" "$OUT_DIR/CHANGELOG.md"
copy_required "assets/demo/demo.vnpak" "$OUT_DIR/demo.vnpak"
copy_required "$RELEASE_GATE_SUMMARY" "$SUM_DIR/release_gate_summary.md"
copy_required "$DEMO_SOAK_SUMMARY" "$SUM_DIR/demo_soak_summary.md"
copy_required "$CI_SUITE_SUMMARY" "$SUM_DIR/ci_suite_summary.md"
copy_required "$HOST_SDK_SUMMARY" "$SUM_DIR/host_sdk_smoke_summary.md"
copy_required "$HOST_SDK_SUMMARY_JSON" "$SUM_DIR/host_sdk_smoke_summary.json"
copy_required "$PLATFORM_EVIDENCE_SUMMARY" "$SUM_DIR/platform_evidence_summary.md"
copy_required "$PLATFORM_EVIDENCE_SUMMARY_JSON" "$SUM_DIR/platform_evidence_summary.json"
copy_required "$PREVIEW_EVIDENCE_SUMMARY" "$SUM_DIR/preview_evidence_summary.md"
copy_required "$PREVIEW_EVIDENCE_SUMMARY_JSON" "$SUM_DIR/preview_evidence_summary.json"

INDEX_MD="$OUT_DIR/release_bundle_index.md"
INDEX_JSON="$OUT_DIR/release_bundle_index.json"
{
  echo "# Release Bundle"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Out dir: \`$OUT_DIR\`"
  echo
  echo "## Docs"
  echo
  echo "1. \`docs/release-v0.1.0-alpha.md\`"
  echo "2. \`docs/release-evidence-v0.1.0-alpha.md\`"
  echo "3. \`docs/release-package-v0.1.0-alpha.md\`"
  echo "4. \`docs/release-checklist-v1.0.0.md\`"
  echo "5. \`README.md\`"
  echo "6. \`CHANGELOG.md\`"
  echo
  echo "## Summaries"
  echo
  echo "1. \`summaries/release_gate_summary.md\`"
  echo "2. \`summaries/demo_soak_summary.md\`"
  echo "3. \`summaries/ci_suite_summary.md\`"
  echo "4. \`summaries/host_sdk_smoke_summary.md\`"
  echo "5. \`summaries/host_sdk_smoke_summary.json\`"
  echo "6. \`summaries/platform_evidence_summary.md\`"
  echo "7. \`summaries/platform_evidence_summary.json\`"
  echo "8. \`summaries/preview_evidence_summary.md\`"
  echo "9. \`summaries/preview_evidence_summary.json\`"
  echo
  echo "## Assets"
  echo
  echo "1. \`demo.vnpak\`"
} >"$INDEX_MD"

{
  printf '{\n'
  printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
  printf '  "branch": "%s",\n' "$(git branch --show-current)"
  printf '  "out_dir": "%s",\n' "$OUT_DIR"
  printf '  "index_md": "%s",\n' "$INDEX_MD"
  printf '  "docs": [\n'
  printf '    "%s",\n' "docs/release-v0.1.0-alpha.md"
  printf '    "%s",\n' "docs/release-evidence-v0.1.0-alpha.md"
  printf '    "%s",\n' "docs/release-package-v0.1.0-alpha.md"
  printf '    "%s",\n' "docs/release-checklist-v1.0.0.md"
  printf '    "%s",\n' "README.md"
  printf '    "%s"\n' "CHANGELOG.md"
  printf '  ],\n'
  printf '  "summaries": [\n'
  printf '    "%s",\n' "summaries/release_gate_summary.md"
  printf '    "%s",\n' "summaries/demo_soak_summary.md"
  printf '    "%s",\n' "summaries/ci_suite_summary.md"
  printf '    "%s",\n' "summaries/host_sdk_smoke_summary.md"
  printf '    "%s",\n' "summaries/host_sdk_smoke_summary.json"
  printf '    "%s",\n' "summaries/platform_evidence_summary.md"
  printf '    "%s",\n' "summaries/platform_evidence_summary.json"
  printf '    "%s",\n' "summaries/preview_evidence_summary.md"
  printf '    "%s"\n' "summaries/preview_evidence_summary.json"
  printf '  ],\n'
  printf '  "assets": [\n'
  printf '    "%s"\n' "demo.vnpak"
  printf '  ]\n'
  printf '}\n'
} >"$INDEX_JSON"

echo "trace_id=release.bundle.ok out_dir=$OUT_DIR index=$INDEX_MD index_json=$INDEX_JSON"
