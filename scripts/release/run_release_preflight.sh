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

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_preflight.sh [--allow-dirty] [--skip-cc-suite] [--out-dir <dir>] [--summary-out <path>] [--summary-json-out <path>] [--ci-suite-summary <path>] [--soak-...] [--remote-...]
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
    --soak-scenes|--soak-frames-per-scene|--soak-backend|--soak-pack|--soak-resolution|--soak-dt-ms|--soak-scene-duration-sec|--soak-runner-bin|--remote-release-json|--remote-release-json-url|--remote-github-repo|--remote-tag|--remote-api-root|--remote-token-env|--remote-release-spec)
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
  echo
  echo "## Outputs"
  echo
  echo "1. Gate summary: \`$GATE_OUT_DIR/release_gate_summary.md\`"
  echo "2. Gate summary json: \`$GATE_OUT_DIR/release_gate_summary.json\`"
  echo "3. Export dir: \`$OUT_DIR/export\`"
  echo "4. Export summary: \`$OUT_DIR/export/release_export_summary.md\`"
} >"$SUMMARY_OUT"

{
  printf '{\n'
  printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
  printf '  "branch": "%s",\n' "$(git branch --show-current)"
  printf '  "out_dir": "%s",\n' "$OUT_DIR"
  printf '  "gate_summary_md": "%s",\n' "$GATE_OUT_DIR/release_gate_summary.md"
  printf '  "gate_summary_json": "%s",\n' "$GATE_OUT_DIR/release_gate_summary.json"
  printf '  "export_dir": "%s",\n' "$OUT_DIR/export"
  printf '  "summary_md": "%s",\n' "$SUMMARY_OUT"
  printf '  "summary_json": "%s"\n' "$SUMMARY_JSON_OUT"
  printf '}\n'
} >"$SUMMARY_JSON_OUT"

echo "trace_id=release.preflight.ok summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT gate_dir=$GATE_OUT_DIR export_dir=$OUT_DIR/export"
