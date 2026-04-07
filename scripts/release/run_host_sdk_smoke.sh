#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_release_host_sdk}"
LOG_DIR="$BUILD_DIR/logs"
TMP_BUILD_DIR="$BUILD_DIR/tmp"
SUMMARY_OUT=""
SUMMARY_JSON_OUT=""
SKIP_BUILD=0

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_host_sdk_smoke.sh [--summary-out <path>] [--summary-json-out <path>] [--skip-build]
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
    --summary-json-out)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      SUMMARY_JSON_OUT="$1"
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
  SUMMARY_OUT="$BUILD_DIR/host_sdk_smoke_summary.md"
fi
if [[ -z "$SUMMARY_JSON_OUT" ]]; then
  SUMMARY_JSON_OUT="$BUILD_DIR/host_sdk_smoke_summary.json"
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
  src/core/runtime_input.c
  src/core/runtime_parse.c
  src/core/runtime_persist.c
  src/core/runtime_session_support.c
  src/core/runtime_session_loop.c
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

run_capture() {
  local name="$1"
  shift
  local slug
  slug="$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]' | tr ' /' '__')"
  local log_path="$LOG_DIR/${slug}.log"
  echo "[host-sdk-smoke] $name"
  "$@" >"$log_path" 2>&1
  cat "$log_path"
}

SESSION_BIN="$BUILD_DIR/example_host_embed"
LINUX_TTY_BIN="$BUILD_DIR/example_host_embed_linux_tty"
WINDOWS_CONSOLE_BIN="$BUILD_DIR/example_host_embed_windows_console"

if [[ $SKIP_BUILD -eq 0 ]]; then
  run_capture "build-demo-scripts" ./tools/scriptc/build_demo_scripts.sh
  run_capture "make-demo-pack" ./tools/packer/make_demo_pack.sh
  run_capture "build-example-host-embed" "$CC_BIN" "${CFLAGS[@]}" examples/host-embed/session_loop.c "${COMMON_SRC[@]}" -o "$SESSION_BIN"
  run_capture "build-example-host-embed-linux-tty" "$CC_BIN" "${CFLAGS[@]}" examples/host-embed/linux_tty_loop.c "${COMMON_SRC[@]}" -o "$LINUX_TTY_BIN"
  run_capture "build-example-host-embed-windows-console" "$CC_BIN" "${CFLAGS[@]}" examples/host-embed/windows_console_loop.c "${COMMON_SRC[@]}" -o "$WINDOWS_CONSOLE_BIN"
fi

for bin in "$SESSION_BIN" "$LINUX_TTY_BIN" "$WINDOWS_CONSOLE_BIN"; do
  if [[ ! -x "$bin" ]]; then
    echo "trace_id=release.host_sdk.binary.missing error_code=-2 error_name=VN_E_IO path=$bin message=host sdk example binary missing" >&2
    exit 1
  fi
done

run_capture "run-example-host-embed" "$SESSION_BIN"
run_capture "run-example-host-embed-linux-tty" "$LINUX_TTY_BIN"
run_capture "run-example-host-embed-windows-console" "$WINDOWS_CONSOLE_BIN"

{
  echo "# Host SDK Smoke Summary"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Build dir: \`$BUILD_DIR\`"
  echo "- Log dir: \`$LOG_DIR\`"
  echo
  echo "## Commands"
  echo
  echo "1. \`examples/host-embed/session_loop.c\`"
  echo "2. \`examples/host-embed/linux_tty_loop.c\`"
  echo "3. \`examples/host-embed/windows_console_loop.c\`"
  echo
  echo "## Results"
  echo
  echo "1. \`$(tail -n 1 "$LOG_DIR/run-example-host-embed.log")\`"
  echo "2. \`$(tail -n 1 "$LOG_DIR/run-example-host-embed-linux-tty.log")\`"
  echo "3. \`$(tail -n 1 "$LOG_DIR/run-example-host-embed-windows-console.log")\`"
} >"$SUMMARY_OUT"

{
  printf '{\n'
  printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
  printf '  "branch": "%s",\n' "$(git branch --show-current)"
  printf '  "summary_md": "%s",\n' "$SUMMARY_OUT"
  printf '  "session_log": "%s",\n' "$LOG_DIR/run-example-host-embed.log"
  printf '  "linux_tty_log": "%s",\n' "$LOG_DIR/run-example-host-embed-linux-tty.log"
  printf '  "windows_console_log": "%s"\n' "$LOG_DIR/run-example-host-embed-windows-console.log"
  printf '}\n'
} >"$SUMMARY_JSON_OUT"

echo "trace_id=release.host_sdk.ok summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT"
