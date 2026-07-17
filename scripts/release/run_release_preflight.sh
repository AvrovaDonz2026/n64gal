#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_preflight}"
ALLOW_DIRTY=0
SKIP_CC_SUITE=0
SUMMARY_OUT=""
SUMMARY_JSON_OUT=""
GATE_OUT_DIR=""
EXTRA_GATE_ARGS=()
CI_SUITE_SUMMARY=""
CONTENT_SOAK_SUMMARY=""
CONTENT_SOAK_SUMMARY_JSON=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_preflight.sh [--allow-dirty] [--skip-cc-suite] [--out-dir <dir>] [--summary-out <path>] [--summary-json-out <path>] [--ci-suite-summary <path>] [--content-soak-summary <path> --content-soak-summary-json <path>] [--soak-...] [--remote-...]
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
    --out-dir)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      OUT_DIR="$1"
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
    --ci-suite-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
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
    --release-spec|--soak-scenes|--soak-frames-per-scene|--soak-backend|--soak-pack|--soak-resolution|--soak-dt-ms|--soak-scene-duration-sec|--soak-runner-bin|--remote-release-json|--remote-release-json-url|--remote-github-repo|--remote-tag|--remote-api-root|--remote-token-env|--remote-release-spec)
      key="$1"
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      EXTRA_GATE_ARGS+=("$key" "$1")
      shift
      ;;
    --soak-skip-build|--soak-skip-pack)
      EXTRA_GATE_ARGS+=("$1")
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

if [[ $SKIP_CC_SUITE -ne 0 && -z "$CI_SUITE_SUMMARY" ]]; then
  echo "trace_id=release.preflight.ci_summary.required error_code=-1 error_name=VN_E_INVALID_ARG message=--skip-cc-suite requires --ci-suite-summary" >&2
  exit 2
fi
if [[ -n "$CONTENT_SOAK_SUMMARY" && -z "$CONTENT_SOAK_SUMMARY_JSON" ]] ||
   [[ -z "$CONTENT_SOAK_SUMMARY" && -n "$CONTENT_SOAK_SUMMARY_JSON" ]]; then
  echo "trace_id=release.preflight.content_soak.pair_required error_code=-1 error_name=VN_E_INVALID_ARG message=content soak markdown and JSON summaries must be provided together" >&2
  exit 2
fi
mkdir -p "$OUT_DIR"
if [[ -z "$SUMMARY_OUT" ]]; then
  SUMMARY_OUT="$OUT_DIR/release_preflight_summary.md"
fi
if [[ -z "$SUMMARY_JSON_OUT" ]]; then
  SUMMARY_JSON_OUT="$OUT_DIR/release_preflight_summary.json"
fi
if [[ -z "$GATE_OUT_DIR" ]]; then
  GATE_OUT_DIR="$OUT_DIR/gate"
fi

gate_cmd=(bash scripts/release/run_release_gate.sh
  --with-soak
  --with-export
  --summary-out "$GATE_OUT_DIR/release_gate_summary.md"
  --summary-json-out "$GATE_OUT_DIR/release_gate_summary.json"
  --export-out-dir "$OUT_DIR/export")

if [[ -n "$CI_SUITE_SUMMARY" ]]; then
  gate_cmd+=(--ci-suite-summary "$CI_SUITE_SUMMARY")
fi
if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
  gate_cmd+=(--content-soak-summary "$CONTENT_SOAK_SUMMARY")
  gate_cmd+=(--content-soak-summary-json "$CONTENT_SOAK_SUMMARY_JSON")
fi

if [[ $ALLOW_DIRTY -ne 0 ]]; then
  gate_cmd+=(--allow-dirty)
fi
if [[ $SKIP_CC_SUITE -ne 0 ]]; then
  gate_cmd+=(--skip-cc-suite)
fi
if [[ ${#EXTRA_GATE_ARGS[@]} -gt 0 ]]; then
  gate_cmd+=("${EXTRA_GATE_ARGS[@]}")
fi

"${gate_cmd[@]}"

{
  echo "# Release Preflight Summary"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Out dir: \`$OUT_DIR\`"
  if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
    echo "- Content soak summary: \`$CONTENT_SOAK_SUMMARY\`"
    echo "- Content soak summary JSON: \`$CONTENT_SOAK_SUMMARY_JSON\`"
  fi
  echo
  echo "## Outputs"
  echo
  echo "1. Gate summary: \`$GATE_OUT_DIR/release_gate_summary.md\`"
  echo "2. Gate summary json: \`$GATE_OUT_DIR/release_gate_summary.json\`"
  echo "3. Export dir: \`$OUT_DIR/export\`"
  echo "4. Export summary: \`$OUT_DIR/export/release_export_summary.md\`"
} >"$SUMMARY_OUT"

python3 - "$SUMMARY_JSON_OUT" "$(git rev-parse --short HEAD)" "$(git branch --show-current)" \
  "$OUT_DIR" "$CONTENT_SOAK_SUMMARY" "$CONTENT_SOAK_SUMMARY_JSON" \
  "$GATE_OUT_DIR/release_gate_summary.md" "$GATE_OUT_DIR/release_gate_summary.json" \
  "$OUT_DIR/export" "$SUMMARY_OUT" <<'PY'
import json
import sys
from pathlib import Path

(
    output_path, head, branch, out_dir, content_soak_summary,
    content_soak_summary_json, gate_summary_md, gate_summary_json,
    export_dir, summary_md,
) = sys.argv[1:]
payload = {
    "head": head,
    "branch": branch,
    "out_dir": out_dir,
    "content_soak_summary": content_soak_summary,
    "content_soak_summary_json": content_soak_summary_json,
    "gate_summary_md": gate_summary_md,
    "gate_summary_json": gate_summary_json,
    "export_dir": export_dir,
    "summary_md": summary_md,
    "summary_json": output_path,
}
Path(output_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY

echo "trace_id=release.preflight.ok summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT gate_dir=$GATE_OUT_DIR export_dir=$OUT_DIR/export"
