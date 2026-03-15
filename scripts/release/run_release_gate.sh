#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

ALLOW_DIRTY=0
SKIP_CC_SUITE=0
SUMMARY_OUT=""
LOG_DIR=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_gate.sh [--allow-dirty] [--skip-cc-suite] [--summary-out <path>]
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
    --summary-out)
      shift
      if [[ $# -eq 0 ]]; then
        usage
        exit 2
      fi
      SUMMARY_OUT="$1"
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
  } >"$SUMMARY_OUT"
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

echo "trace_id=release.gate.ok summary=$SUMMARY_OUT"
