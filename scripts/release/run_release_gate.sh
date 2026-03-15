#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

ALLOW_DIRTY=0
SKIP_CC_SUITE=0
WITH_SOAK=0
WITH_BUNDLE=0
SUMMARY_OUT=""
SUMMARY_JSON_OUT=""
LOG_DIR=""
SOAK_ARGS=()
SOAK_SUMMARY_OUT=""
SOAK_SUMMARY_JSON_OUT=""
BUNDLE_ARGS=()
BUNDLE_OUT=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_gate.sh [--allow-dirty] [--skip-cc-suite] [--with-soak] [--with-bundle] [--summary-out <path>] [--summary-json-out <path>] [--soak-...] [--bundle-...]
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
    --soak-scenes|--soak-frames-per-scene|--soak-backend|--soak-pack|--soak-resolution|--soak-dt-ms|--soak-scene-duration-sec|--soak-summary-out|--soak-runner-bin)
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
LOG_DIR="$BUILD_DIR/logs"
TMP_BUILD_DIR="$BUILD_DIR/tmp"
mkdir -p "$BUILD_DIR" "$LOG_DIR" "$TMP_BUILD_DIR"
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
    fi
    if [[ -n "$soak_summary_text" ]]; then
      echo
      echo "## Soak Summary"
      echo
      printf "%s\n" "$soak_summary_text"
    fi
  } >"$SUMMARY_OUT"
  {
    printf '{\n'
    printf '  "status": "%s",\n' "$status"
    printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
    printf '  "branch": "%s",\n' "$(git branch --show-current)"
    printf '  "allow_dirty": %s,\n' "$ALLOW_DIRTY"
    printf '  "skip_cc_suite": %s,\n' "$SKIP_CC_SUITE"
    printf '  "with_soak": %s,\n' "$WITH_SOAK"
    printf '  "with_bundle": %s,\n' "$WITH_BUNDLE"
    printf '  "summary_md": "%s",\n' "$SUMMARY_OUT"
    printf '  "soak_summary_md": "%s",\n' "$SOAK_SUMMARY_OUT"
    printf '  "soak_summary_json": "%s",\n' "$SOAK_SUMMARY_JSON_OUT"
    printf '  "bundle_out_dir": "%s"\n' "$BUNDLE_OUT"
    printf '}\n'
  } >"$SUMMARY_JSON_OUT"
}

trap 'rc=$?; if [[ $rc -eq 0 ]]; then write_summary success; else write_summary failed; fi; exit $rc' EXIT

if [[ $ALLOW_DIRTY -eq 0 ]]; then
  if [[ -n "$(git status --porcelain)" ]]; then
    echo "trace_id=release.gate.worktree.dirty error_code=-3 error_name=VN_E_FORMAT message=worktree must be clean" >&2
    exit 1
  fi
fi

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

if [[ $WITH_BUNDLE -ne 0 ]]; then
  bundle_cmd=(bash scripts/release/run_release_bundle.sh --out-dir "$BUNDLE_OUT" --gate-summary "$SUMMARY_OUT")
  if [[ $WITH_SOAK -ne 0 ]]; then
    bundle_cmd+=(--soak-summary "$SOAK_SUMMARY_OUT")
  fi
  bundle_cmd+=(--ci-summary "$ROOT_DIR/build_ci_cc/ci_suite_summary.md")
  if [[ ${#BUNDLE_ARGS[@]} -gt 0 ]]; then
    bundle_cmd+=("${BUNDLE_ARGS[@]}")
  fi
  run_step "release-bundle" "${bundle_cmd[@]}"
fi

echo "trace_id=release.gate.ok summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT"
