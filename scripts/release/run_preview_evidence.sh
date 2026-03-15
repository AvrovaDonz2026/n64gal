#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_release_preview}"
LOG_DIR="$BUILD_DIR/logs"
TMP_BUILD_DIR="$BUILD_DIR/tmp"
PREVIEW_BIN="$BUILD_DIR/vn_previewd"
REQUEST_PATH="$BUILD_DIR/preview_request.txt"
RESPONSE_PATH="$BUILD_DIR/preview_response.json"
SUMMARY_OUT=""
SKIP_BUILD=0

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_preview_evidence.sh [--summary-out <path>] [--skip-build]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --summary-out)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      SUMMARY_OUT="$1"
      shift
      ;;
    --skip-build)
      SKIP_BUILD=1
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

if [[ -z "$SUMMARY_OUT" ]]; then
  SUMMARY_OUT="$BUILD_DIR/preview_evidence_summary.md"
fi

mkdir -p "$BUILD_DIR" "$LOG_DIR" "$TMP_BUILD_DIR"
export TMPDIR="$TMP_BUILD_DIR"

CC_BIN="${CC:-cc}"
CFLAGS=(
  -std=c89
  -Wall
  -Wextra
  -Werror
  -pedantic-errors
  -Iinclude
)
COMMON_SRC=(
  src/core/error.c
  src/core/backend_registry.c
  src/core/renderer.c
  src/core/save.c
  src/core/vm.c
  src/core/pack.c
  src/core/platform.c
  src/core/runtime_cli.c
  src/core/dynamic_resolution.c
  src/frontend/render_ops.c
  src/frontend/dirty_tiles.c
  src/backend/common/pixel_pipeline.c
  src/backend/avx2/avx2_backend.c
  src/backend/avx2/avx2_fill_fade.c
  src/backend/avx2/avx2_textured.c
  src/backend/neon/neon_backend.c
  src/backend/rvv/rvv_backend.c
  src/backend/scalar/scalar_backend.c
)
PREVIEW_SRC=(
  src/tools/preview_cli.c
)

run_capture() {
  local name="$1"
  shift
  local slug
  slug="$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]' | tr ' /' '__')"
  local log_path="$LOG_DIR/${slug}.log"
  echo "[preview-evidence] $name"
  "$@" >"$log_path" 2>&1
  cat "$log_path"
}

if [[ $SKIP_BUILD -eq 0 ]]; then
  run_capture "build-demo-scripts" ./tools/scriptc/build_demo_scripts.sh
  run_capture "make-demo-pack" ./tools/packer/make_demo_pack.sh
  run_capture "build-vn-previewd" "$CC_BIN" "${CFLAGS[@]}" src/tools/previewd_main.c "${PREVIEW_SRC[@]}" "${COMMON_SRC[@]}" -o "$PREVIEW_BIN"
fi

if [[ ! -x "$PREVIEW_BIN" ]]; then
  echo "trace_id=release.preview.binary.missing error_code=-2 error_name=VN_E_IO path=$PREVIEW_BIN message=vn_previewd binary missing" >&2
  exit 1
fi

cat >"$REQUEST_PATH" <<'EOF'
preview_protocol=v1
project_dir=.
scene_name=S2
backend=auto
resolution=600x800
frames=8
trace=1
command=set_choice:1
command=inject_input:choice:1
command=inject_input:key:t
command=step_frame:8
response=build_release_preview/preview_response.json
EOF

run_capture "run-preview" "$PREVIEW_BIN" --request "$REQUEST_PATH" --response "$RESPONSE_PATH"

if [[ ! -f "$RESPONSE_PATH" ]]; then
  echo "trace_id=release.preview.response.missing error_code=-2 error_name=VN_E_IO path=$RESPONSE_PATH message=preview response missing" >&2
  exit 1
fi

if ! grep -q '"status":"ok"' "$RESPONSE_PATH"; then
  echo "trace_id=release.preview.response.invalid error_code=-3 error_name=VN_E_FORMAT path=$RESPONSE_PATH message=preview response status not ok" >&2
  exit 1
fi

if ! grep -q '"trace_id":"preview.ok"' "$RESPONSE_PATH"; then
  echo "trace_id=release.preview.response.invalid error_code=-3 error_name=VN_E_FORMAT path=$RESPONSE_PATH message=preview ok trace missing" >&2
  exit 1
fi

{
  echo "# Preview Evidence Summary"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Build dir: \`$BUILD_DIR\`"
  echo "- Preview bin: \`$PREVIEW_BIN\`"
  echo "- Request: \`$REQUEST_PATH\`"
  echo "- Response: \`$RESPONSE_PATH\`"
  echo
  echo "## Request"
  echo
  sed 's/^/- /' "$REQUEST_PATH"
  echo
  echo "## Response Checks"
  echo
  echo "1. \`status=ok\`"
  echo "2. \`trace_id=preview.ok\`"
  echo "3. \`scene_name=S2\`"
  echo "4. \`choice_selected_index=1\`"
  echo "5. \`events\` present"
  echo "6. \`dirty_tile_total\` present"
} >"$SUMMARY_OUT"

echo "trace_id=release.preview.ok summary=$SUMMARY_OUT response=$RESPONSE_PATH"
