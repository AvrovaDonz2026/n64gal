#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

ALLOW_DIRTY=0
SKIP_CC_SUITE=0
WITH_SOAK=0
WITH_BUNDLE=0
WITH_EXPORT=0
SUMMARY_OUT=""
SUMMARY_JSON_OUT=""
LOG_DIR=""
CI_SUITE_SUMMARY=""
CONTENT_SOAK_SUMMARY=""
CONTENT_SOAK_SUMMARY_JSON=""
SOAK_ARGS=()
SOAK_SUMMARY_OUT=""
SOAK_SUMMARY_JSON_OUT=""
BUNDLE_ARGS=()
BUNDLE_OUT=""
HOST_SDK_SUMMARY_OUT=""
HOST_SDK_SUMMARY_JSON_OUT=""
PLATFORM_EVIDENCE_OUT_DIR=""
PLATFORM_EVIDENCE_SUMMARY_OUT=""
PLATFORM_EVIDENCE_SUMMARY_JSON_OUT=""
PREVIEW_EVIDENCE_OUT_DIR=""
PREVIEW_EVIDENCE_SUMMARY_OUT=""
PREVIEW_EVIDENCE_SUMMARY_JSON_OUT=""
EXPORT_OUT_DIR=""
EXPORT_SUMMARY_OUT=""
EXPORT_SUMMARY_JSON_OUT=""
EXPORT_RELEASE_SPEC=""
EXPORT_REMOTE_RELEASE_JSON=""
EXPORT_REMOTE_RELEASE_JSON_URL=""
EXPORT_REMOTE_GITHUB_REPO=""
EXPORT_REMOTE_TAG=""
EXPORT_REMOTE_API_ROOT=""
EXPORT_REMOTE_TOKEN_ENV=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_gate.sh [--allow-dirty] [--skip-cc-suite] [--with-soak] [--with-bundle] [--with-export] [--release-spec <path>] [--summary-out <path>] [--summary-json-out <path>] [--ci-suite-summary <path>] [--content-soak-summary <path> --content-soak-summary-json <path>] [--soak-...] [--bundle-...] [--export-...] [--remote-...]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --allow-dirty)
      ALLOW_DIRTY=1
      shift
      ;;
    --skip-cc-suite)
      SKIP_CC_SUITE=1
      shift
      ;;
    --with-soak)
      WITH_SOAK=1
      shift
      ;;
    --with-bundle)
      WITH_BUNDLE=1
      shift
      ;;
    --with-export)
      WITH_EXPORT=1
      shift
      ;;
    --release-spec)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      EXPORT_RELEASE_SPEC="$1"
      shift
      ;;
    --summary-out)
      shift
      if [[ $# -eq 0 ]]; then
        usage
        exit 2
      fi
      SUMMARY_OUT="$1"
      shift
      ;;
    --summary-json-out)
      shift
      if [[ $# -eq 0 ]]; then
        usage
        exit 2
      fi
      SUMMARY_JSON_OUT="$1"
      shift
      ;;
    --ci-suite-summary)
      shift
      if [[ $# -eq 0 ]]; then
        usage
        exit 2
      fi
      CI_SUITE_SUMMARY="$1"
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
    --soak-scenes|--soak-frames-per-scene|--soak-backend|--soak-pack|--soak-resolution|--soak-dt-ms|--soak-scene-duration-sec|--soak-summary-out|--soak-summary-json-out|--soak-runner-bin)
      key="$1"
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      if [[ "$key" == "--soak-summary-out" ]]; then
        SOAK_SUMMARY_OUT="$1"
      elif [[ "$key" == "--soak-summary-json-out" ]]; then
        SOAK_SUMMARY_JSON_OUT="$1"
      else
        SOAK_ARGS+=("--${key#--soak-}" "$1")
      fi
      shift
      ;;
    --soak-skip-build|--soak-skip-pack)
      SOAK_ARGS+=("--${1#--soak-}")
      shift
      ;;
    --bundle-out-dir|--bundle-gate-summary|--bundle-soak-summary|--bundle-ci-summary)
      key="$1"
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      if [[ "$key" == "--bundle-out-dir" ]]; then
        BUNDLE_OUT="$1"
      fi
      BUNDLE_ARGS+=("--${key#--bundle-}" "$1")
      shift
      ;;
    --export-out-dir|--export-summary-out|--export-summary-json-out)
      key="$1"
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      if [[ "$key" == "--export-out-dir" ]]; then
        EXPORT_OUT_DIR="$1"
      elif [[ "$key" == "--export-summary-out" ]]; then
        EXPORT_SUMMARY_OUT="$1"
      elif [[ "$key" == "--export-summary-json-out" ]]; then
        EXPORT_SUMMARY_JSON_OUT="$1"
      fi
      shift
      ;;
    --remote-release-json|--remote-release-json-url|--remote-github-repo|--remote-tag|--remote-api-root|--remote-token-env|--remote-release-spec)
      key="$1"
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      case "$key" in
        --remote-release-json) EXPORT_REMOTE_RELEASE_JSON="$1" ;;
        --remote-release-json-url) EXPORT_REMOTE_RELEASE_JSON_URL="$1" ;;
        --remote-github-repo) EXPORT_REMOTE_GITHUB_REPO="$1" ;;
        --remote-tag) EXPORT_REMOTE_TAG="$1" ;;
        --remote-api-root) EXPORT_REMOTE_API_ROOT="$1" ;;
        --remote-token-env) EXPORT_REMOTE_TOKEN_ENV="$1" ;;
        --remote-release-spec) EXPORT_RELEASE_SPEC="$1" ;;
      esac
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
  echo "trace_id=release.gate.content_soak.pair_required error_code=-1 error_name=VN_E_INVALID_ARG message=content soak markdown and JSON summaries must be provided together" >&2
  exit 2
fi

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_release_gate}"
if [[ -z "$SUMMARY_OUT" ]]; then
  SUMMARY_OUT="$BUILD_DIR/release_gate_summary.md"
fi
if [[ -z "$SUMMARY_JSON_OUT" ]]; then
  SUMMARY_JSON_OUT="$BUILD_DIR/release_gate_summary.json"
fi
if [[ -z "$SOAK_SUMMARY_OUT" ]]; then
  SOAK_SUMMARY_OUT="$BUILD_DIR/demo_soak_summary.md"
fi
if [[ -z "$SOAK_SUMMARY_JSON_OUT" ]]; then
  SOAK_SUMMARY_JSON_OUT="$BUILD_DIR/demo_soak_summary.json"
fi
if [[ -z "$BUNDLE_OUT" ]]; then
  BUNDLE_OUT="$BUILD_DIR/release_bundle"
fi
if [[ -z "$EXPORT_OUT_DIR" ]]; then
  EXPORT_OUT_DIR="$BUILD_DIR/release_export"
fi
if [[ -z "$EXPORT_SUMMARY_OUT" ]]; then
  EXPORT_SUMMARY_OUT="$EXPORT_OUT_DIR/release_export_summary.md"
fi
if [[ -z "$EXPORT_SUMMARY_JSON_OUT" ]]; then
  EXPORT_SUMMARY_JSON_OUT="$EXPORT_OUT_DIR/release_export_summary.json"
fi
if [[ -z "$EXPORT_RELEASE_SPEC" ]]; then
  EXPORT_RELEASE_SPEC="$ROOT_DIR/docs/release-publish-v1.1.0.json"
fi
eval "$(python3 tools/validate/release_spec.py --shell "$EXPORT_RELEASE_SPEC")"
if [[ ( $WITH_BUNDLE -ne 0 || $WITH_EXPORT -ne 0 ) && "$SPEC_VERSION" == "v1.1.0" && -z "$CONTENT_SOAK_SUMMARY" ]]; then
  echo "trace_id=release.gate.content_soak.required error_code=-1 error_name=VN_E_INVALID_ARG message=v1.1.0 bundle/export requires real-content soak markdown and JSON summaries" >&2
  exit 2
fi
if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
  CONTENT_PACK_PATH="$(python3 tools/validate/release_spec.py "$EXPORT_RELEASE_SPEC" --asset-path-for-name content-demo.vnpak)"
  python3 tools/validate/validate_content_soak_summary.py \
    "$CONTENT_SOAK_SUMMARY_JSON" --summary-md "$CONTENT_SOAK_SUMMARY" --pack "$CONTENT_PACK_PATH"
fi
if [[ -z "$CI_SUITE_SUMMARY" ]]; then
  CI_SUITE_SUMMARY="$ROOT_DIR/build_ci_cc/ci_suite_summary.md"
fi
if [[ -z "$HOST_SDK_SUMMARY_OUT" ]]; then
  HOST_SDK_SUMMARY_OUT="$BUILD_DIR/host_sdk_smoke_summary.md"
fi
if [[ -z "$HOST_SDK_SUMMARY_JSON_OUT" ]]; then
  HOST_SDK_SUMMARY_JSON_OUT="$BUILD_DIR/host_sdk_smoke_summary.json"
fi
if [[ -z "$PLATFORM_EVIDENCE_OUT_DIR" ]]; then
  PLATFORM_EVIDENCE_OUT_DIR="$BUILD_DIR/platform_evidence"
fi
if [[ -z "$PLATFORM_EVIDENCE_SUMMARY_OUT" ]]; then
  PLATFORM_EVIDENCE_SUMMARY_OUT="$PLATFORM_EVIDENCE_OUT_DIR/platform_evidence_summary.md"
fi
if [[ -z "$PLATFORM_EVIDENCE_SUMMARY_JSON_OUT" ]]; then
  PLATFORM_EVIDENCE_SUMMARY_JSON_OUT="$PLATFORM_EVIDENCE_OUT_DIR/platform_evidence_summary.json"
fi
if [[ -z "$PREVIEW_EVIDENCE_OUT_DIR" ]]; then
  PREVIEW_EVIDENCE_OUT_DIR="$BUILD_DIR/preview_evidence"
fi
if [[ -z "$PREVIEW_EVIDENCE_SUMMARY_OUT" ]]; then
  PREVIEW_EVIDENCE_SUMMARY_OUT="$PREVIEW_EVIDENCE_OUT_DIR/preview_evidence_summary.md"
fi
if [[ -z "$PREVIEW_EVIDENCE_SUMMARY_JSON_OUT" ]]; then
  PREVIEW_EVIDENCE_SUMMARY_JSON_OUT="$PREVIEW_EVIDENCE_OUT_DIR/preview_evidence_summary.json"
fi
LOG_DIR="$BUILD_DIR/logs"
TMP_BUILD_DIR="$BUILD_DIR/tmp"
mkdir -p "$BUILD_DIR" "$LOG_DIR" "$TMP_BUILD_DIR"
mkdir -p "$(dirname "$SUMMARY_OUT")" "$(dirname "$SUMMARY_JSON_OUT")" "$(dirname "$SOAK_SUMMARY_OUT")" "$(dirname "$SOAK_SUMMARY_JSON_OUT")" "$(dirname "$EXPORT_SUMMARY_OUT")" "$(dirname "$EXPORT_SUMMARY_JSON_OUT")"
export TMPDIR="$TMP_BUILD_DIR"

run_step() {
  local name="$1"
  shift
  local slug
  slug="$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]' | tr ' /' '__')"
  local log_path="$LOG_DIR/${slug}.log"
  echo "[release-gate] $name"
  "$@" >"$log_path" 2>&1
  cat "$log_path"
}

write_summary() {
  local status="$1"
  local soak_summary_text=""
  if [[ $WITH_SOAK -ne 0 && -f "$SOAK_SUMMARY_OUT" ]]; then
    soak_summary_text="$(cat "$SOAK_SUMMARY_OUT")"
  fi
  {
    echo "# Release Gate Summary"
    echo
    echo "- Status: \`$status\`"
    echo "- Head: \`$(git rev-parse --short HEAD)\`"
    echo "- Branch: \`$(git branch --show-current)\`"
    echo "- Build dir: \`$BUILD_DIR\`"
    echo "- Log dir: \`$LOG_DIR\`"
    echo "- Summary path: \`$SUMMARY_OUT\`"
    echo "- allow_dirty: \`$ALLOW_DIRTY\`"
    echo "- skip_cc_suite: \`$SKIP_CC_SUITE\`"
    echo "- with_soak: \`$WITH_SOAK\`"
    echo "- with_bundle: \`$WITH_BUNDLE\`"
    echo "- with_export: \`$WITH_EXPORT\`"
    if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
      echo "- Content soak summary: \`$CONTENT_SOAK_SUMMARY\`"
      echo "- Content soak summary JSON: \`$CONTENT_SOAK_SUMMARY_JSON\`"
    fi
    if [[ $ALLOW_DIRTY -eq 0 ]]; then
      echo "- Worktree policy: clean required"
    else
      echo "- Worktree policy: dirty allowed"
    fi
    echo
    echo "## Steps"
    echo
    echo "1. \`python3 tools/toolchain.py validate-all\`"
    echo "2. \`./scripts/check_c89.sh\`"
    echo "3. \`./scripts/check_api_docs_sync.sh\`"
    if [[ $SKIP_CC_SUITE -eq 0 ]]; then
      echo "4. \`./scripts/ci/run_cc_suite.sh\`"
    else
      echo "4. \`./scripts/ci/run_cc_suite.sh\` skipped"
    fi
    if [[ $WITH_SOAK -ne 0 ]]; then
      echo "5. \`./scripts/release/run_demo_soak.sh ...\`"
      echo "- Soak summary path: \`$SOAK_SUMMARY_OUT\`"
    fi
    if [[ $WITH_BUNDLE -ne 0 ]]; then
      echo "6. \`./scripts/release/run_release_bundle.sh ...\`"
      echo "- Bundle out dir: \`$BUNDLE_OUT\`"
      echo "- Host SDK summary path: \`$HOST_SDK_SUMMARY_OUT\`"
      echo "- Platform evidence summary path: \`$PLATFORM_EVIDENCE_SUMMARY_OUT\`"
      echo "- Preview evidence summary path: \`$PREVIEW_EVIDENCE_SUMMARY_OUT\`"
    fi
    if [[ $WITH_EXPORT -ne 0 ]]; then
      echo "7. \`./scripts/release/run_release_export.sh ...\`"
      echo "- Export out dir: \`$EXPORT_OUT_DIR\`"
      if [[ -n "$EXPORT_REMOTE_RELEASE_JSON" || -n "$EXPORT_REMOTE_RELEASE_JSON_URL" || -n "$EXPORT_REMOTE_GITHUB_REPO" ]]; then
        echo "- Remote release alignment enabled"
      fi
    fi
    if [[ -n "$soak_summary_text" ]]; then
      echo
      echo "## Soak Summary"
      echo
      printf "%s\n" "$soak_summary_text"
    fi
  } >"$SUMMARY_OUT"
  python3 - "$SUMMARY_JSON_OUT" "$status" "$(git rev-parse --short HEAD)" \
    "$(git branch --show-current)" "$ALLOW_DIRTY" "$SKIP_CC_SUITE" "$WITH_SOAK" \
    "$WITH_BUNDLE" "$WITH_EXPORT" "$CONTENT_SOAK_SUMMARY" "$CONTENT_SOAK_SUMMARY_JSON" \
    "$SUMMARY_OUT" "$SOAK_SUMMARY_OUT" "$SOAK_SUMMARY_JSON_OUT" \
    "$HOST_SDK_SUMMARY_OUT" "$HOST_SDK_SUMMARY_JSON_OUT" \
    "$PLATFORM_EVIDENCE_SUMMARY_OUT" "$PLATFORM_EVIDENCE_SUMMARY_JSON_OUT" \
    "$PREVIEW_EVIDENCE_SUMMARY_OUT" "$PREVIEW_EVIDENCE_SUMMARY_JSON_OUT" \
    "$BUNDLE_OUT" "$EXPORT_OUT_DIR" "$EXPORT_SUMMARY_OUT" "$EXPORT_SUMMARY_JSON_OUT" \
    "$EXPORT_REMOTE_RELEASE_JSON" "$EXPORT_REMOTE_RELEASE_JSON_URL" "$EXPORT_REMOTE_GITHUB_REPO" <<'PY'
import json
import sys
from pathlib import Path

(
    output_path, status, head, branch, allow_dirty, skip_cc_suite, with_soak,
    with_bundle, with_export, content_soak_summary, content_soak_summary_json,
    summary_md, soak_summary_md, soak_summary_json, host_sdk_summary_md,
    host_sdk_summary_json, platform_summary_md, platform_summary_json,
    preview_summary_md, preview_summary_json, bundle_out_dir, export_out_dir,
    export_summary_md, export_summary_json, remote_release_json,
    remote_release_json_url, remote_github_repo,
) = sys.argv[1:]
payload = {
    "status": status,
    "head": head,
    "branch": branch,
    "allow_dirty": int(allow_dirty),
    "skip_cc_suite": int(skip_cc_suite),
    "with_soak": int(with_soak),
    "with_bundle": int(with_bundle),
    "with_export": int(with_export),
    "content_soak_summary": content_soak_summary,
    "content_soak_summary_json": content_soak_summary_json,
    "summary_md": summary_md,
    "soak_summary_md": soak_summary_md,
    "soak_summary_json": soak_summary_json,
    "host_sdk_summary_md": host_sdk_summary_md,
    "host_sdk_summary_json": host_sdk_summary_json,
    "platform_evidence_summary_md": platform_summary_md,
    "platform_evidence_summary_json": platform_summary_json,
    "preview_evidence_summary_md": preview_summary_md,
    "preview_evidence_summary_json": preview_summary_json,
    "bundle_out_dir": bundle_out_dir,
    "export_out_dir": export_out_dir,
    "export_summary_md": export_summary_md,
    "export_summary_json": export_summary_json,
    "remote_release_json": remote_release_json,
    "remote_release_json_url": remote_release_json_url,
    "remote_github_repo": remote_github_repo,
}
Path(output_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY
}

trap 'rc=$?; if [[ $rc -eq 0 ]]; then write_summary success; else write_summary failed; fi; exit $rc' EXIT

require_clean_worktree() {
  local phase
  phase="$1"
  if [[ $ALLOW_DIRTY -ne 0 ]]; then
    return 0
  fi
  if [[ -n "$(git status --porcelain)" ]]; then
    echo "trace_id=release.gate.worktree.dirty error_code=-3 error_name=VN_E_FORMAT phase=$phase message=worktree must be clean" >&2
    return 1
  fi
}

require_clean_worktree initial

run_step "validate-all" python3 tools/toolchain.py validate-all
run_step "check-c89" ./scripts/check_c89.sh
run_step "check-api-docs-sync" ./scripts/check_api_docs_sync.sh

if [[ $SKIP_CC_SUITE -eq 0 ]]; then
  run_step "run-cc-suite" ./scripts/ci/run_cc_suite.sh
fi

if [[ $WITH_SOAK -ne 0 ]]; then
  soak_cmd=(bash scripts/release/run_demo_soak.sh --summary-out "$SOAK_SUMMARY_OUT" --summary-json-out "$SOAK_SUMMARY_JSON_OUT")
  if [[ ${#SOAK_ARGS[@]} -gt 0 ]]; then
    soak_cmd+=("${SOAK_ARGS[@]}")
  fi
  run_step "release-soak" "${soak_cmd[@]}"
fi

if [[ $WITH_BUNDLE -ne 0 || $WITH_EXPORT -ne 0 ]]; then
  write_summary in_progress
fi

if [[ $WITH_BUNDLE -ne 0 || $WITH_EXPORT -ne 0 ]]; then
  run_step "release-host-sdk-smoke" bash scripts/release/run_host_sdk_smoke.sh --summary-out "$HOST_SDK_SUMMARY_OUT" --summary-json-out "$HOST_SDK_SUMMARY_JSON_OUT"
  run_step "release-platform-evidence" bash scripts/release/run_platform_evidence.sh --out-dir "$PLATFORM_EVIDENCE_OUT_DIR" --ci-suite-summary "$CI_SUITE_SUMMARY" --summary-out "$PLATFORM_EVIDENCE_SUMMARY_OUT" --summary-json-out "$PLATFORM_EVIDENCE_SUMMARY_JSON_OUT"
  run_step "release-preview-evidence" bash scripts/release/run_preview_evidence.sh --out-dir "$PREVIEW_EVIDENCE_OUT_DIR" --summary-out "$PREVIEW_EVIDENCE_SUMMARY_OUT" --summary-json-out "$PREVIEW_EVIDENCE_SUMMARY_JSON_OUT"
  write_summary success
fi

if [[ $WITH_BUNDLE -ne 0 ]]; then
  bundle_cmd=(bash scripts/release/run_release_bundle.sh --out-dir "$BUNDLE_OUT" --release-spec "$EXPORT_RELEASE_SPEC" --gate-summary "$SUMMARY_OUT")
  if [[ $WITH_SOAK -ne 0 ]]; then
    bundle_cmd+=(--soak-summary "$SOAK_SUMMARY_OUT")
  fi
  bundle_cmd+=(--ci-summary "$CI_SUITE_SUMMARY")
  if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
    bundle_cmd+=(--content-soak-summary "$CONTENT_SOAK_SUMMARY")
    bundle_cmd+=(--content-soak-summary-json "$CONTENT_SOAK_SUMMARY_JSON")
  fi
  bundle_cmd+=(--host-sdk-summary "$HOST_SDK_SUMMARY_OUT" --host-sdk-summary-json "$HOST_SDK_SUMMARY_JSON_OUT")
  bundle_cmd+=(--platform-evidence-summary "$PLATFORM_EVIDENCE_SUMMARY_OUT" --platform-evidence-summary-json "$PLATFORM_EVIDENCE_SUMMARY_JSON_OUT")
  bundle_cmd+=(--preview-evidence-summary "$PREVIEW_EVIDENCE_SUMMARY_OUT" --preview-evidence-summary-json "$PREVIEW_EVIDENCE_SUMMARY_JSON_OUT")
  if [[ ${#BUNDLE_ARGS[@]} -gt 0 ]]; then
    bundle_cmd+=("${BUNDLE_ARGS[@]}")
  fi
  run_step "release-bundle" "${bundle_cmd[@]}"
fi

if [[ $WITH_EXPORT -ne 0 ]]; then
  export_cmd=(bash scripts/release/run_release_export.sh
    --out-dir "$EXPORT_OUT_DIR"
    --release-spec "$EXPORT_RELEASE_SPEC"
    --gate-summary "$SUMMARY_OUT"
    --ci-suite-summary "$CI_SUITE_SUMMARY"
    --host-sdk-summary "$HOST_SDK_SUMMARY_OUT"
    --host-sdk-summary-json "$HOST_SDK_SUMMARY_JSON_OUT"
    --platform-evidence-summary "$PLATFORM_EVIDENCE_SUMMARY_OUT"
    --platform-evidence-summary-json "$PLATFORM_EVIDENCE_SUMMARY_JSON_OUT"
    --preview-evidence-summary "$PREVIEW_EVIDENCE_SUMMARY_OUT"
    --preview-evidence-summary-json "$PREVIEW_EVIDENCE_SUMMARY_JSON_OUT"
    --summary-out "$EXPORT_SUMMARY_OUT"
    --summary-json-out "$EXPORT_SUMMARY_JSON_OUT")
  if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
    export_cmd+=(--content-soak-summary "$CONTENT_SOAK_SUMMARY")
    export_cmd+=(--content-soak-summary-json "$CONTENT_SOAK_SUMMARY_JSON")
  fi
  if [[ $WITH_SOAK -ne 0 ]]; then
    export_cmd+=(--soak-summary "$SOAK_SUMMARY_OUT")
  fi
  if [[ -n "$EXPORT_REMOTE_RELEASE_JSON" ]]; then
    export_cmd+=(--remote-release-json "$EXPORT_REMOTE_RELEASE_JSON")
  fi
  if [[ -n "$EXPORT_REMOTE_RELEASE_JSON_URL" ]]; then
    export_cmd+=(--remote-release-json-url "$EXPORT_REMOTE_RELEASE_JSON_URL")
  fi
  if [[ -n "$EXPORT_REMOTE_GITHUB_REPO" ]]; then
    export_cmd+=(--remote-github-repo "$EXPORT_REMOTE_GITHUB_REPO")
  fi
  if [[ -n "$EXPORT_REMOTE_TAG" ]]; then
    export_cmd+=(--remote-tag "$EXPORT_REMOTE_TAG")
  fi
  if [[ -n "$EXPORT_REMOTE_API_ROOT" ]]; then
    export_cmd+=(--remote-api-root "$EXPORT_REMOTE_API_ROOT")
  fi
  if [[ -n "$EXPORT_REMOTE_TOKEN_ENV" ]]; then
    export_cmd+=(--remote-token-env "$EXPORT_REMOTE_TOKEN_ENV")
  fi
  run_step "release-export" "${export_cmd[@]}"
fi

write_summary success
require_clean_worktree final

echo "trace_id=release.gate.ok summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT"
