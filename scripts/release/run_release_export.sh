#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_export}"
RELEASE_SPEC="${RELEASE_SPEC:-$ROOT_DIR/docs/release-publish-v0.1.0-alpha.json}"
TAG_NAME=""
RELEASE_URL=""
RELEASE_NOTE=""
ASSET_PATH=""
GATE_SUMMARY="${GATE_SUMMARY:-$ROOT_DIR/build_release_gate/release_gate_summary.md}"
SOAK_SUMMARY="${SOAK_SUMMARY:-$ROOT_DIR/build_release_soak/demo_soak_summary.md}"
CI_SUITE_SUMMARY="${CI_SUITE_SUMMARY:-$ROOT_DIR/build_ci_cc/ci_suite_summary.md}"
HOST_SDK_SUMMARY="${HOST_SDK_SUMMARY:-$ROOT_DIR/build_release_host_sdk/host_sdk_smoke_summary.md}"
HOST_SDK_SUMMARY_JSON="${HOST_SDK_SUMMARY_JSON:-$ROOT_DIR/build_release_host_sdk/host_sdk_smoke_summary.json}"
PLATFORM_EVIDENCE_SUMMARY="${PLATFORM_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_platform/platform_evidence_summary.md}"
PLATFORM_EVIDENCE_SUMMARY_JSON="${PLATFORM_EVIDENCE_SUMMARY_JSON:-$ROOT_DIR/build_release_platform/platform_evidence_summary.json}"
PREVIEW_EVIDENCE_SUMMARY="${PREVIEW_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_preview/preview_evidence_summary.md}"
PREVIEW_EVIDENCE_SUMMARY_JSON="${PREVIEW_EVIDENCE_SUMMARY_JSON:-$ROOT_DIR/build_release_preview/preview_evidence_summary.json}"
REMOTE_RELEASE_JSON=""
REMOTE_RELEASE_JSON_URL=""
REMOTE_GITHUB_REPO=""
REMOTE_TAG_NAME=""
REMOTE_API_ROOT=""
REMOTE_TOKEN_ENV=""
SUMMARY_OUT=""
SUMMARY_JSON_OUT=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_export.sh [--out-dir <dir>] [--release-spec <path>] [--tag <tag>] [--release-url <url>] [--release-note <path>] [--asset <path>] [--gate-summary <path>] [--soak-summary <path>] [--ci-suite-summary <path>] [--host-sdk-summary <path>] [--host-sdk-summary-json <path>] [--platform-evidence-summary <path>] [--platform-evidence-summary-json <path>] [--preview-evidence-summary <path>] [--preview-evidence-summary-json <path>] [--remote-release-json <path>] [--remote-release-json-url <url>] [--remote-github-repo <owner/repo>] [--remote-tag <tag>] [--remote-api-root <url>] [--remote-token-env <env>] [--summary-out <path>] [--summary-json-out <path>]
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
    --remote-release-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      REMOTE_RELEASE_JSON="$1"
      shift
      ;;
    --remote-release-json-url)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      REMOTE_RELEASE_JSON_URL="$1"
      shift
      ;;
    --remote-github-repo)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      REMOTE_GITHUB_REPO="$1"
      shift
      ;;
    --remote-tag)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      REMOTE_TAG_NAME="$1"
      shift
      ;;
    --remote-api-root)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      REMOTE_API_ROOT="$1"
      shift
      ;;
    --remote-token-env)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      REMOTE_TOKEN_ENV="$1"
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
eval "$(
python3 - "$RELEASE_SPEC" <<'PY'
import json
import shlex
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    payload = json.load(handle)

asset = payload.get("asset", {})
fields = {
    "SPEC_TAG": payload.get("tag", ""),
    "SPEC_RELEASE_URL": payload.get("release_url", ""),
    "SPEC_RELEASE_NOTE": payload.get("release_note", ""),
    "SPEC_ASSET_PATH": asset.get("path", ""),
}
for key, value in fields.items():
    print(f"{key}={shlex.quote(str(value))}")
PY
)"
if [[ -z "$TAG_NAME" ]]; then
  TAG_NAME="$SPEC_TAG"
fi
if [[ -z "$RELEASE_URL" ]]; then
  RELEASE_URL="$SPEC_RELEASE_URL"
fi
if [[ -z "$RELEASE_NOTE" ]]; then
  RELEASE_NOTE="$ROOT_DIR/$SPEC_RELEASE_NOTE"
fi
if [[ -z "$ASSET_PATH" ]]; then
  ASSET_PATH="$ROOT_DIR/$SPEC_ASSET_PATH"
fi
BUNDLE_DIR="$OUT_DIR/bundle"
REPORT_DIR="$OUT_DIR/report"
PUBLISH_DIR="$OUT_DIR/publish"
REMOTE_DIR="$OUT_DIR/remote"

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
  --release-spec "$RELEASE_SPEC" \
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
  --release-spec "$RELEASE_SPEC" \
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
  --release-spec "$RELEASE_SPEC" \
  --tag "$TAG_NAME" \
  --release-url "$RELEASE_URL" \
  --release-note "$RELEASE_NOTE" \
  --asset "$ASSET_PATH" \
  --bundle-index "$BUNDLE_DIR/release_bundle_index.md" \
  --bundle-manifest "$BUNDLE_DIR/release_bundle_manifest.json" \
  --report-json "$REPORT_DIR/release_report.json"

if [[ -n "$REMOTE_RELEASE_JSON" || -n "$REMOTE_RELEASE_JSON_URL" || -n "$REMOTE_GITHUB_REPO" ]]; then
  remote_cmd=(bash scripts/release/run_release_remote_summary.sh --out-dir "$REMOTE_DIR" --release-spec "$RELEASE_SPEC")
  if [[ -n "$REMOTE_RELEASE_JSON" ]]; then
    remote_cmd+=(--release-json "$REMOTE_RELEASE_JSON")
  elif [[ -n "$REMOTE_RELEASE_JSON_URL" ]]; then
    remote_cmd+=(--release-json-url "$REMOTE_RELEASE_JSON_URL")
  else
    remote_cmd+=(--github-repo "$REMOTE_GITHUB_REPO")
    if [[ -n "$REMOTE_TAG_NAME" ]]; then
      remote_cmd+=(--tag "$REMOTE_TAG_NAME")
    fi
    if [[ -n "$REMOTE_API_ROOT" ]]; then
      remote_cmd+=(--api-root "$REMOTE_API_ROOT")
    fi
    if [[ -n "$REMOTE_TOKEN_ENV" ]]; then
      remote_cmd+=(--token-env "$REMOTE_TOKEN_ENV")
    fi
  fi
  run_step "release-remote-summary" "${remote_cmd[@]}"
fi

final_bundle_cmd=(bash scripts/release/run_release_bundle.sh
  --out-dir "$BUNDLE_DIR"
  --release-spec "$RELEASE_SPEC"
  --gate-summary "$GATE_SUMMARY"
  --soak-summary "$SOAK_SUMMARY"
  --ci-summary "$CI_SUITE_SUMMARY"
  --host-sdk-summary "$HOST_SDK_SUMMARY"
  --host-sdk-summary-json "$HOST_SDK_SUMMARY_JSON"
  --platform-evidence-summary "$PLATFORM_EVIDENCE_SUMMARY"
  --platform-evidence-summary-json "$PLATFORM_EVIDENCE_SUMMARY_JSON"
  --preview-evidence-summary "$PREVIEW_EVIDENCE_SUMMARY"
  --preview-evidence-summary-json "$PREVIEW_EVIDENCE_SUMMARY_JSON"
  --report-md "$REPORT_DIR/release_report.md"
  --report-json "$REPORT_DIR/release_report.json"
  --publish-map-md "$PUBLISH_DIR/release_publish_map.md"
  --publish-map-json "$PUBLISH_DIR/release_publish_map.json")

if [[ -n "$REMOTE_RELEASE_JSON" || -n "$REMOTE_RELEASE_JSON_URL" || -n "$REMOTE_GITHUB_REPO" ]]; then
  final_bundle_cmd+=(--remote-summary-md "$REMOTE_DIR/release_remote_summary.md")
  final_bundle_cmd+=(--remote-summary-json "$REMOTE_DIR/release_remote_summary.json")
fi

run_step "release-bundle-final" "${final_bundle_cmd[@]}"

{
  echo "# Release Export Summary"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Tag: \`$TAG_NAME\`"
  echo "- Release URL: \`$RELEASE_URL\`"
  echo "- Release spec: \`$RELEASE_SPEC\`"
  echo
  echo "## Outputs"
  echo
  echo "1. Bundle dir: \`$BUNDLE_DIR\`"
  echo "2. Report dir: \`$REPORT_DIR\`"
  echo "3. Publish dir: \`$PUBLISH_DIR\`"
  if [[ -n "$REMOTE_RELEASE_JSON" || -n "$REMOTE_RELEASE_JSON_URL" || -n "$REMOTE_GITHUB_REPO" ]]; then
    echo "4. Remote dir: \`$REMOTE_DIR\`"
    echo "5. Bundle now also includes: \`summaries/release_report.{md,json}\`, \`summaries/release_publish_map.{md,json}\`, \`summaries/release_remote_summary.{md,json}\`"
    echo "6. Summary: \`$SUMMARY_OUT\`"
  else
    echo "4. Bundle now also includes: \`summaries/release_report.{md,json}\`, \`summaries/release_publish_map.{md,json}\`"
    echo "5. Summary: \`$SUMMARY_OUT\`"
  fi
} >"$SUMMARY_OUT"

{
  printf '{\n'
  printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
  printf '  "branch": "%s",\n' "$(git branch --show-current)"
  printf '  "release_spec": "%s",\n' "$RELEASE_SPEC"
  printf '  "tag": "%s",\n' "$TAG_NAME"
  printf '  "release_url": "%s",\n' "$RELEASE_URL"
  printf '  "bundle_dir": "%s",\n' "$BUNDLE_DIR"
  printf '  "report_dir": "%s",\n' "$REPORT_DIR"
  printf '  "publish_dir": "%s",\n' "$PUBLISH_DIR"
  printf '  "remote_dir": "%s",\n' "$REMOTE_DIR"
  printf '  "summary_md": "%s",\n' "$SUMMARY_OUT"
  printf '  "summary_json": "%s"\n' "$SUMMARY_JSON_OUT"
  printf '}\n'
} >"$SUMMARY_JSON_OUT"

echo "trace_id=release.export.ok out_dir=$OUT_DIR summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT tag=$TAG_NAME"
