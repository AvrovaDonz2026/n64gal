#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_bundle}"
RELEASE_SPEC="${RELEASE_SPEC:-$ROOT_DIR/docs/release-publish-v1.0.0.json}"
RELEASE_GATE_SUMMARY="${RELEASE_GATE_SUMMARY:-$ROOT_DIR/build_release_gate/release_gate_summary.md}"
DEMO_SOAK_SUMMARY="${DEMO_SOAK_SUMMARY:-$ROOT_DIR/build_release_gate/demo_soak_summary.md}"
CI_SUITE_SUMMARY="${CI_SUITE_SUMMARY:-$ROOT_DIR/build_ci_cc/ci_suite_summary.md}"
HOST_SDK_SUMMARY="${HOST_SDK_SUMMARY:-$ROOT_DIR/build_release_host_sdk/host_sdk_smoke_summary.md}"
HOST_SDK_SUMMARY_JSON="${HOST_SDK_SUMMARY_JSON:-$ROOT_DIR/build_release_host_sdk/host_sdk_smoke_summary.json}"
PLATFORM_EVIDENCE_SUMMARY="${PLATFORM_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_platform/platform_evidence_summary.md}"
PLATFORM_EVIDENCE_SUMMARY_JSON="${PLATFORM_EVIDENCE_SUMMARY_JSON:-$ROOT_DIR/build_release_platform/platform_evidence_summary.json}"
PREVIEW_EVIDENCE_SUMMARY="${PREVIEW_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_preview/preview_evidence_summary.md}"
PREVIEW_EVIDENCE_SUMMARY_JSON="${PREVIEW_EVIDENCE_SUMMARY_JSON:-$ROOT_DIR/build_release_preview/preview_evidence_summary.json}"
RELEASE_REPORT_MD=""
RELEASE_REPORT_JSON=""
RELEASE_PUBLISH_MAP_MD=""
RELEASE_PUBLISH_MAP_JSON=""
RELEASE_REMOTE_SUMMARY_MD=""
RELEASE_REMOTE_SUMMARY_JSON=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_bundle.sh [--out-dir <dir>] [--release-spec <path>] [--release-gate-summary <path>|--gate-summary <path>] [--demo-soak-summary <path>|--soak-summary <path>] [--ci-suite-summary <path>|--ci-summary <path>] [--host-sdk-summary <path>] [--host-sdk-summary-json <path>] [--platform-evidence-summary <path>] [--platform-evidence-summary-json <path>] [--preview-evidence-summary <path>] [--preview-evidence-summary-json <path>] [--report-md <path>] [--report-json <path>] [--publish-map-md <path>] [--publish-map-json <path>] [--remote-summary-md <path>] [--remote-summary-json <path>]
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
    --report-md)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_REPORT_MD="$1"
      shift
      ;;
    --report-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_REPORT_JSON="$1"
      shift
      ;;
    --publish-map-md)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_PUBLISH_MAP_MD="$1"
      shift
      ;;
    --publish-map-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_PUBLISH_MAP_JSON="$1"
      shift
      ;;
    --remote-summary-md)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_REMOTE_SUMMARY_MD="$1"
      shift
      ;;
    --remote-summary-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_REMOTE_SUMMARY_JSON="$1"
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

eval "$(
python3 - "$RELEASE_SPEC" <<'PY'
import json
import os
import shlex
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    payload = json.load(handle)

version = str(payload.get("version", ""))
release_note = str(payload.get("release_note", ""))
asset = payload.get("asset", {})
asset_path = str(asset.get("path", ""))

note_dir = os.path.dirname(release_note)
note_base = os.path.basename(release_note)
suffix = ""
if note_base.startswith("release-") and note_base.endswith(".md"):
    suffix = note_base[len("release-"):-3]
elif version:
    suffix = version

fields = {
    "SPEC_VERSION": version,
    "SPEC_RELEASE_NOTE": release_note,
    "SPEC_RELEASE_EVIDENCE": os.path.join(note_dir, f"release-evidence-{suffix}.md") if suffix else "",
    "SPEC_RELEASE_PACKAGE": os.path.join(note_dir, f"release-package-{suffix}.md") if suffix else "",
    "SPEC_ASSET_PATH": asset_path,
}

for key, value in fields.items():
    print(f"{key}={shlex.quote(value)}")
PY
)"

copy_required() {
  local src="$1"
  local dst="$2"
  if [[ ! -f "$src" ]]; then
    echo "trace_id=release.bundle.missing error_code=-2 error_name=VN_E_IO path=$src message=required bundle input missing" >&2
    exit 1
  fi
  cp "$src" "$dst"
}

copy_required "$SPEC_RELEASE_NOTE" "$DOC_DIR/$(basename "$SPEC_RELEASE_NOTE")"
copy_required "$SPEC_RELEASE_EVIDENCE" "$DOC_DIR/$(basename "$SPEC_RELEASE_EVIDENCE")"
copy_required "$SPEC_RELEASE_PACKAGE" "$DOC_DIR/$(basename "$SPEC_RELEASE_PACKAGE")"
copy_required "docs/release-checklist-v1.0.0.md" "$DOC_DIR/release-checklist-v1.0.0.md"
copy_required "README.md" "$OUT_DIR/README.md"
copy_required "CHANGELOG.md" "$OUT_DIR/CHANGELOG.md"
copy_required "$SPEC_ASSET_PATH" "$OUT_DIR/$(basename "$SPEC_ASSET_PATH")"
copy_required "$RELEASE_GATE_SUMMARY" "$SUM_DIR/release_gate_summary.md"
copy_required "$DEMO_SOAK_SUMMARY" "$SUM_DIR/demo_soak_summary.md"
copy_required "$CI_SUITE_SUMMARY" "$SUM_DIR/ci_suite_summary.md"
copy_required "$HOST_SDK_SUMMARY" "$SUM_DIR/host_sdk_smoke_summary.md"
copy_required "$HOST_SDK_SUMMARY_JSON" "$SUM_DIR/host_sdk_smoke_summary.json"
copy_required "$PLATFORM_EVIDENCE_SUMMARY" "$SUM_DIR/platform_evidence_summary.md"
copy_required "$PLATFORM_EVIDENCE_SUMMARY_JSON" "$SUM_DIR/platform_evidence_summary.json"
copy_required "$PREVIEW_EVIDENCE_SUMMARY" "$SUM_DIR/preview_evidence_summary.md"
copy_required "$PREVIEW_EVIDENCE_SUMMARY_JSON" "$SUM_DIR/preview_evidence_summary.json"
if [[ -n "$RELEASE_REPORT_MD" ]]; then
  copy_required "$RELEASE_REPORT_MD" "$SUM_DIR/release_report.md"
fi
if [[ -n "$RELEASE_REPORT_JSON" ]]; then
  copy_required "$RELEASE_REPORT_JSON" "$SUM_DIR/release_report.json"
fi
if [[ -n "$RELEASE_PUBLISH_MAP_MD" ]]; then
  copy_required "$RELEASE_PUBLISH_MAP_MD" "$SUM_DIR/release_publish_map.md"
fi
if [[ -n "$RELEASE_PUBLISH_MAP_JSON" ]]; then
  copy_required "$RELEASE_PUBLISH_MAP_JSON" "$SUM_DIR/release_publish_map.json"
fi
if [[ -n "$RELEASE_REMOTE_SUMMARY_MD" ]]; then
  copy_required "$RELEASE_REMOTE_SUMMARY_MD" "$SUM_DIR/release_remote_summary.md"
fi
if [[ -n "$RELEASE_REMOTE_SUMMARY_JSON" ]]; then
  copy_required "$RELEASE_REMOTE_SUMMARY_JSON" "$SUM_DIR/release_remote_summary.json"
fi

INDEX_MD="$OUT_DIR/release_bundle_index.md"
INDEX_JSON="$OUT_DIR/release_bundle_index.json"
MANIFEST_MD="$OUT_DIR/release_bundle_manifest.md"
MANIFEST_JSON="$OUT_DIR/release_bundle_manifest.json"
BUNDLE_FILES=(
  "docs/$(basename "$SPEC_RELEASE_NOTE")"
  "docs/$(basename "$SPEC_RELEASE_EVIDENCE")"
  "docs/$(basename "$SPEC_RELEASE_PACKAGE")"
  "docs/release-checklist-v1.0.0.md"
  "README.md"
  "CHANGELOG.md"
  "summaries/release_gate_summary.md"
  "summaries/demo_soak_summary.md"
  "summaries/ci_suite_summary.md"
  "summaries/host_sdk_smoke_summary.md"
  "summaries/host_sdk_smoke_summary.json"
  "summaries/platform_evidence_summary.md"
  "summaries/platform_evidence_summary.json"
  "summaries/preview_evidence_summary.md"
  "summaries/preview_evidence_summary.json"
  "$(basename "$SPEC_ASSET_PATH")"
)
if [[ -n "$RELEASE_REPORT_MD" ]]; then
  BUNDLE_FILES+=("summaries/release_report.md")
fi
if [[ -n "$RELEASE_REPORT_JSON" ]]; then
  BUNDLE_FILES+=("summaries/release_report.json")
fi
if [[ -n "$RELEASE_PUBLISH_MAP_MD" ]]; then
  BUNDLE_FILES+=("summaries/release_publish_map.md")
fi
if [[ -n "$RELEASE_PUBLISH_MAP_JSON" ]]; then
  BUNDLE_FILES+=("summaries/release_publish_map.json")
fi
if [[ -n "$RELEASE_REMOTE_SUMMARY_MD" ]]; then
  BUNDLE_FILES+=("summaries/release_remote_summary.md")
fi
if [[ -n "$RELEASE_REMOTE_SUMMARY_JSON" ]]; then
  BUNDLE_FILES+=("summaries/release_remote_summary.json")
fi
{
  echo "# Release Bundle"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Out dir: \`$OUT_DIR\`"
  echo
  echo "## Docs"
  echo
  echo "1. \`docs/$(basename "$SPEC_RELEASE_NOTE")\`"
  echo "2. \`docs/$(basename "$SPEC_RELEASE_EVIDENCE")\`"
  echo "3. \`docs/$(basename "$SPEC_RELEASE_PACKAGE")\`"
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
  if [[ -n "$RELEASE_REPORT_MD" ]]; then
    echo "10. \`summaries/release_report.md\`"
  fi
  if [[ -n "$RELEASE_REPORT_JSON" ]]; then
    echo "11. \`summaries/release_report.json\`"
  fi
  if [[ -n "$RELEASE_PUBLISH_MAP_MD" ]]; then
    echo "12. \`summaries/release_publish_map.md\`"
  fi
  if [[ -n "$RELEASE_PUBLISH_MAP_JSON" ]]; then
    echo "13. \`summaries/release_publish_map.json\`"
  fi
  if [[ -n "$RELEASE_REMOTE_SUMMARY_MD" ]]; then
    echo "14. \`summaries/release_remote_summary.md\`"
  fi
  if [[ -n "$RELEASE_REMOTE_SUMMARY_JSON" ]]; then
    echo "15. \`summaries/release_remote_summary.json\`"
  fi
  echo
  echo "## Assets"
  echo
  echo "1. \`$(basename "$SPEC_ASSET_PATH")\`"
  echo
  echo "## Manifests"
  echo
  echo "1. \`release_bundle_manifest.md\`"
  echo "2. \`release_bundle_manifest.json\`"
} >"$INDEX_MD"

{
  printf '{\n'
  printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
  printf '  "branch": "%s",\n' "$(git branch --show-current)"
  printf '  "out_dir": "%s",\n' "$OUT_DIR"
  printf '  "index_md": "%s",\n' "$INDEX_MD"
  printf '  "manifest_md": "%s",\n' "$MANIFEST_MD"
  printf '  "manifest_json": "%s",\n' "$MANIFEST_JSON"
  printf '  "external_reference_ready": true,\n'
  printf '  "docs": [\n'
  printf '    "%s",\n' "docs/$(basename "$SPEC_RELEASE_NOTE")"
  printf '    "%s",\n' "docs/$(basename "$SPEC_RELEASE_EVIDENCE")"
  printf '    "%s",\n' "docs/$(basename "$SPEC_RELEASE_PACKAGE")"
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
  printf '    "%s"' "summaries/preview_evidence_summary.json"
  if [[ -n "$RELEASE_REPORT_MD" ]]; then
    printf ',\n    "%s"' "summaries/release_report.md"
  fi
  if [[ -n "$RELEASE_REPORT_JSON" ]]; then
    printf ',\n    "%s"' "summaries/release_report.json"
  fi
  if [[ -n "$RELEASE_PUBLISH_MAP_MD" ]]; then
    printf ',\n    "%s"' "summaries/release_publish_map.md"
  fi
  if [[ -n "$RELEASE_PUBLISH_MAP_JSON" ]]; then
    printf ',\n    "%s"' "summaries/release_publish_map.json"
  fi
  if [[ -n "$RELEASE_REMOTE_SUMMARY_MD" ]]; then
    printf ',\n    "%s"' "summaries/release_remote_summary.md"
  fi
  if [[ -n "$RELEASE_REMOTE_SUMMARY_JSON" ]]; then
    printf ',\n    "%s"' "summaries/release_remote_summary.json"
  fi
  printf '\n'
  printf '  ],\n'
  printf '  "assets": [\n'
  printf '    "%s"\n' "$(basename "$SPEC_ASSET_PATH")"
  printf '  ]\n'
  printf '}\n'
} >"$INDEX_JSON"

{
  echo "# Release Bundle Manifest"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Out dir: \`$OUT_DIR\`"
  echo
  echo "| Path | SHA256 | Bytes |"
  echo "|---|---|---:|"
  for rel in "${BUNDLE_FILES[@]}"; do
    abs="$OUT_DIR/$rel"
    sha="$(sha256sum "$abs" | awk '{print $1}')"
    size="$(wc -c <"$abs" | tr -d '[:space:]')"
    echo "| \`$rel\` | \`$sha\` | $size |"
  done
} >"$MANIFEST_MD"

{
  printf '{\n'
  printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
  printf '  "branch": "%s",\n' "$(git branch --show-current)"
  printf '  "out_dir": "%s",\n' "$OUT_DIR"
  printf '  "files": [\n'
  for i in "${!BUNDLE_FILES[@]}"; do
    rel="${BUNDLE_FILES[$i]}"
    abs="$OUT_DIR/$rel"
    sha="$(sha256sum "$abs" | awk '{print $1}')"
    size="$(wc -c <"$abs" | tr -d '[:space:]')"
    printf '    {"path":"%s","sha256":"%s","bytes":%s}' "$rel" "$sha" "$size"
    if [[ $i -lt $((${#BUNDLE_FILES[@]} - 1)) ]]; then
      printf ','
    fi
    printf '\n'
  done
  printf '  ]\n'
  printf '}\n'
} >"$MANIFEST_JSON"

echo "trace_id=release.bundle.ok out_dir=$OUT_DIR index=$INDEX_MD index_json=$INDEX_JSON manifest=$MANIFEST_MD manifest_json=$MANIFEST_JSON"
