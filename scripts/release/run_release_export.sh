#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_export}"
TAG_NAME="${TAG_NAME:-v0.1.0-alpha}"
RELEASE_URL="${RELEASE_URL:-https://github.com/AvrovaDonz2026/n64gal/releases/tag/v0.1.0-alpha}"
RELEASE_NOTE="${RELEASE_NOTE:-$ROOT_DIR/docs/release-v0.1.0-alpha.md}"
ASSET_PATH="${ASSET_PATH:-$ROOT_DIR/assets/demo/demo.vnpak}"
GATE_SUMMARY="${GATE_SUMMARY:-$ROOT_DIR/build_release_gate/release_gate_summary.md}"
SOAK_SUMMARY="${SOAK_SUMMARY:-$ROOT_DIR/build_release_soak/demo_soak_summary.md}"
CI_SUITE_SUMMARY="${CI_SUITE_SUMMARY:-$ROOT_DIR/build_ci_cc/ci_suite_summary.md}"
HOST_SDK_SUMMARY="${HOST_SDK_SUMMARY:-$ROOT_DIR/build_release_host_sdk/host_sdk_smoke_summary.md}"
HOST_SDK_SUMMARY_JSON="${HOST_SDK_SUMMARY_JSON:-$ROOT_DIR/build_release_host_sdk/host_sdk_smoke_summary.json}"
PLATFORM_EVIDENCE_SUMMARY="${PLATFORM_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_platform/platform_evidence_summary.md}"
PLATFORM_EVIDENCE_SUMMARY_JSON="${PLATFORM_EVIDENCE_SUMMARY_JSON:-$ROOT_DIR/build_release_platform/platform_evidence_summary.json}"
PREVIEW_EVIDENCE_SUMMARY="${PREVIEW_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_preview/preview_evidence_summary.md}"
PREVIEW_EVIDENCE_SUMMARY_JSON="${PREVIEW_EVIDENCE_SUMMARY_JSON:-$ROOT_DIR/build_release_preview/preview_evidence_summary.json}"
SUMMARY_OUT=""
SUMMARY_JSON_OUT=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_export.sh [--out-dir <dir>] [--tag <tag>] [--release-url <url>] [--release-note <path>] [--asset <path>] [--gate-summary <path>] [--soak-summary <path>] [--ci-suite-summary <path>] [--host-sdk-summary <path>] [--host-sdk-summary-json <path>] [--platform-evidence-summary <path>] [--platform-evidence-summary-json <path>] [--preview-evidence-summary <path>] [--preview-evidence-summary-json <path>] [--summary-out <path>] [--summary-json-out <path>]
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
    --tag)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      TAG_NAME="$1"
      shift
      ;;
    --release-url)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_URL="$1"
      shift
      ;;
    --release-note)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_NOTE="$1"
      shift
      ;;
    --asset)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      ASSET_PATH="$1"
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
    --summary-out)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      SUMMARY_OUT="$1"
      shift
      ;;
    --summary-json-out)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      SUMMARY_JSON_OUT="$1"
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
BUNDLE_DIR="$OUT_DIR/bundle"
REPORT_DIR="$OUT_DIR/report"
PUBLISH_DIR="$OUT_DIR/publish"

if [[ -z "$SUMMARY_OUT" ]]; then
  SUMMARY_OUT="$OUT_DIR/release_export_summary.md"
fi
if [[ -z "$SUMMARY_JSON_OUT" ]]; then
  SUMMARY_JSON_OUT="$OUT_DIR/release_export_summary.json"
fi

run_step() {
  local name="$1"
  shift
  echo "[release-export] $name"
  "$@"
}

run_step "release-bundle" \
  bash scripts/release/run_release_bundle.sh \
  --out-dir "$BUNDLE_DIR" \
  --gate-summary "$GATE_SUMMARY" \
  --soak-summary "$SOAK_SUMMARY" \
  --ci-summary "$CI_SUITE_SUMMARY" \
  --host-sdk-summary "$HOST_SDK_SUMMARY" \
  --host-sdk-summary-json "$HOST_SDK_SUMMARY_JSON" \
  --platform-evidence-summary "$PLATFORM_EVIDENCE_SUMMARY" \
  --platform-evidence-summary-json "$PLATFORM_EVIDENCE_SUMMARY_JSON" \
  --preview-evidence-summary "$PREVIEW_EVIDENCE_SUMMARY" \
  --preview-evidence-summary-json "$PREVIEW_EVIDENCE_SUMMARY_JSON"

run_step "release-report" \
  bash scripts/release/run_release_report.sh \
  --out-dir "$REPORT_DIR" \
  --bundle-index "$BUNDLE_DIR/release_bundle_index.md" \
  --bundle-manifest "$BUNDLE_DIR/release_bundle_manifest.json" \
  --gate-summary "$GATE_SUMMARY" \
  --soak-summary "$SOAK_SUMMARY" \
  --ci-suite-summary "$CI_SUITE_SUMMARY" \
  --host-sdk-summary "$HOST_SDK_SUMMARY" \
  --platform-evidence-summary "$PLATFORM_EVIDENCE_SUMMARY" \
  --preview-evidence-summary "$PREVIEW_EVIDENCE_SUMMARY"

run_step "release-publish-map" \
  bash scripts/release/run_release_publish_map.sh \
  --out-dir "$PUBLISH_DIR" \
  --tag "$TAG_NAME" \
  --release-url "$RELEASE_URL" \
  --release-note "$RELEASE_NOTE" \
  --asset "$ASSET_PATH" \
  --bundle-index "$BUNDLE_DIR/release_bundle_index.md" \
  --bundle-manifest "$BUNDLE_DIR/release_bundle_manifest.json" \
  --report-json "$REPORT_DIR/release_report.json"

{
  echo "# Release Export Summary"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Tag: \`$TAG_NAME\`"
  echo "- Release URL: \`$RELEASE_URL\`"
  echo
  echo "## Outputs"
  echo
  echo "1. Bundle dir: \`$BUNDLE_DIR\`"
  echo "2. Report dir: \`$REPORT_DIR\`"
  echo "3. Publish dir: \`$PUBLISH_DIR\`"
  echo "4. Summary: \`$SUMMARY_OUT\`"
} >"$SUMMARY_OUT"

{
  printf '{\n'
  printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
  printf '  "branch": "%s",\n' "$(git branch --show-current)"
  printf '  "tag": "%s",\n' "$TAG_NAME"
  printf '  "release_url": "%s",\n' "$RELEASE_URL"
  printf '  "bundle_dir": "%s",\n' "$BUNDLE_DIR"
  printf '  "report_dir": "%s",\n' "$REPORT_DIR"
  printf '  "publish_dir": "%s",\n' "$PUBLISH_DIR"
  printf '  "summary_md": "%s",\n' "$SUMMARY_OUT"
  printf '  "summary_json": "%s"\n' "$SUMMARY_JSON_OUT"
  printf '}\n'
} >"$SUMMARY_JSON_OUT"

echo "trace_id=release.export.ok out_dir=$OUT_DIR summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT tag=$TAG_NAME"
