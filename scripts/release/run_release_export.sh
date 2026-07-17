#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_export}"
RELEASE_SPEC="${RELEASE_SPEC:-$ROOT_DIR/docs/release-publish-v1.1.0.json}"
TAG_NAME=""
RELEASE_URL=""
RELEASE_NOTE=""
ASSET_PATHS=()
GATE_SUMMARY="${GATE_SUMMARY:-$ROOT_DIR/build_release_gate/release_gate_summary.md}"
SOAK_SUMMARY="${SOAK_SUMMARY:-$ROOT_DIR/build_release_soak/demo_soak_summary.md}"
CONTENT_SOAK_SUMMARY=""
CONTENT_SOAK_SUMMARY_JSON=""
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
usage: scripts/release/run_release_export.sh [--out-dir <dir>] [--release-spec <path>] [--tag <tag>] [--release-url <url>] [--release-note <path>] [--asset <path>] [--gate-summary <path>] [--soak-summary <path>] [--content-soak-summary <path> --content-soak-summary-json <path>] [--ci-suite-summary <path>] [--host-sdk-summary <path>] [--host-sdk-summary-json <path>] [--platform-evidence-summary <path>] [--platform-evidence-summary-json <path>] [--preview-evidence-summary <path>] [--preview-evidence-summary-json <path>] [--remote-release-json <path>] [--remote-release-json-url <url>] [--remote-github-repo <owner/repo>] [--remote-tag <tag>] [--remote-api-root <url>] [--remote-token-env <env>] [--summary-out <path>] [--summary-json-out <path>]
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
      ASSET_PATHS+=("$1")
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

if [[ -n "$CONTENT_SOAK_SUMMARY" && -z "$CONTENT_SOAK_SUMMARY_JSON" ]] ||
   [[ -z "$CONTENT_SOAK_SUMMARY" && -n "$CONTENT_SOAK_SUMMARY_JSON" ]]; then
  echo "trace_id=release.export.content_soak.pair_required error_code=-1 error_name=VN_E_INVALID_ARG message=content soak markdown and JSON summaries must be provided together" >&2
  exit 2
fi
CONTENT_SOAK_ARGS=()
if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
  CONTENT_SOAK_ARGS+=(--content-soak-summary "$CONTENT_SOAK_SUMMARY")
  CONTENT_SOAK_ARGS+=(--content-soak-summary-json "$CONTENT_SOAK_SUMMARY_JSON")
fi

mkdir -p "$OUT_DIR"
resolve_spec_path() {
  local value="$1"
  if [[ -z "$value" ]]; then
    printf '%s' ""
  elif [[ "$value" = /* ]]; then
    printf '%s' "$value"
  else
    printf '%s' "$ROOT_DIR/$value"
  fi
}

eval "$(python3 tools/validate/release_spec.py --shell "$RELEASE_SPEC")"
if [[ "$SPEC_VERSION" == "v1.1.0" && -z "$CONTENT_SOAK_SUMMARY" ]]; then
  echo "trace_id=release.export.content_soak.required error_code=-1 error_name=VN_E_INVALID_ARG message=v1.1.0 export requires real-content soak markdown and JSON summaries" >&2
  exit 2
fi
if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
  CONTENT_PACK_PATH="$(python3 tools/validate/release_spec.py "$RELEASE_SPEC" --asset-path-for-name content-demo.vnpak)"
  python3 tools/validate/validate_content_soak_summary.py \
    "$CONTENT_SOAK_SUMMARY_JSON" --summary-md "$CONTENT_SOAK_SUMMARY" --pack "$CONTENT_PACK_PATH"
fi
if [[ -z "$TAG_NAME" ]]; then
  TAG_NAME="$SPEC_TAG"
fi
if [[ -z "$RELEASE_URL" ]]; then
  RELEASE_URL="$SPEC_RELEASE_URL"
fi
if [[ -z "$RELEASE_NOTE" ]]; then
  RELEASE_NOTE="$(resolve_spec_path "$SPEC_RELEASE_NOTE")"
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
  "${CONTENT_SOAK_ARGS[@]}" \
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
  "${CONTENT_SOAK_ARGS[@]}" \
  --ci-suite-summary "$CI_SUITE_SUMMARY" \
  --host-sdk-summary "$HOST_SDK_SUMMARY" \
  --platform-evidence-summary "$PLATFORM_EVIDENCE_SUMMARY" \
  --preview-evidence-summary "$PREVIEW_EVIDENCE_SUMMARY"

publish_cmd=(bash scripts/release/run_release_publish_map.sh \
  --out-dir "$PUBLISH_DIR" \
  --release-spec "$RELEASE_SPEC" \
  --tag "$TAG_NAME" \
  --release-url "$RELEASE_URL" \
  --release-note "$RELEASE_NOTE" \
  --bundle-index "$BUNDLE_DIR/release_bundle_index.md" \
  --bundle-manifest "$BUNDLE_DIR/release_bundle_manifest.json" \
  --report-json "$REPORT_DIR/release_report.json")
for asset_path in "${ASSET_PATHS[@]}"; do
  publish_cmd+=(--asset "$(resolve_spec_path "$asset_path")")
done
run_step "release-publish-map" "${publish_cmd[@]}"

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
if [[ ${#CONTENT_SOAK_ARGS[@]} -gt 0 ]]; then
  final_bundle_cmd+=("${CONTENT_SOAK_ARGS[@]}")
fi

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
  if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
    echo "- Content soak summary: \`$CONTENT_SOAK_SUMMARY\`"
    echo "- Content soak summary JSON: \`$CONTENT_SOAK_SUMMARY_JSON\`"
  fi
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

python3 - "$SUMMARY_JSON_OUT" "$(git rev-parse --short HEAD)" "$(git branch --show-current)" \
  "$RELEASE_SPEC" "$TAG_NAME" "$RELEASE_URL" "$CONTENT_SOAK_SUMMARY" \
  "$CONTENT_SOAK_SUMMARY_JSON" "$BUNDLE_DIR" "$REPORT_DIR" "$PUBLISH_DIR" \
  "$REMOTE_DIR" "$SUMMARY_OUT" <<'PY'
import json
import sys
from pathlib import Path

(
    output_path, head, branch, release_spec, tag, release_url,
    content_soak_summary, content_soak_summary_json, bundle_dir, report_dir,
    publish_dir, remote_dir, summary_md,
) = sys.argv[1:]
payload = {
    "head": head,
    "branch": branch,
    "release_spec": release_spec,
    "tag": tag,
    "release_url": release_url,
    "content_soak_summary": content_soak_summary,
    "content_soak_summary_json": content_soak_summary_json,
    "bundle_dir": bundle_dir,
    "report_dir": report_dir,
    "publish_dir": publish_dir,
    "remote_dir": remote_dir,
    "summary_md": summary_md,
    "summary_json": output_path,
}
Path(output_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY

echo "trace_id=release.export.ok out_dir=$OUT_DIR summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT tag=$TAG_NAME"
