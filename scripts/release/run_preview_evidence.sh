#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_release_preview}"
LOG_DIR="$BUILD_DIR/logs"
TMP_BUILD_DIR="$BUILD_DIR/tmp"
PREVIEW_BIN_OVERRIDE=""
REQUEST_PATH_OVERRIDE=""
RESPONSE_PATH_OVERRIDE=""
SUMMARY_OUT=""
SUMMARY_JSON_OUT=""
SKIP_BUILD=0

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_preview_evidence.sh [--out-dir <dir>] [--preview-bin <path>] [--request-out <path>] [--response-out <path>] [--summary-out <path>] [--summary-json-out <path>] [--skip-build]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out-dir)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      BUILD_DIR="$1"
      shift
      ;;
    --preview-bin)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PREVIEW_BIN_OVERRIDE="$1"
      shift
      ;;
    --request-out)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      REQUEST_PATH_OVERRIDE="$1"
      shift
      ;;
    --response-out)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RESPONSE_PATH_OVERRIDE="$1"
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
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

LOG_DIR="$BUILD_DIR/logs"
TMP_BUILD_DIR="$BUILD_DIR/tmp"
PREVIEW_BIN="${PREVIEW_BIN_OVERRIDE:-$BUILD_DIR/vn_previewd}"
REQUEST_PATH="${REQUEST_PATH_OVERRIDE:-$BUILD_DIR/preview_request.txt}"
RESPONSE_PATH="${RESPONSE_PATH_OVERRIDE:-$BUILD_DIR/preview_response.json}"
SCREENSHOT_PATH="$BUILD_DIR/content_preview.ppm"

if [[ -z "$SUMMARY_OUT" ]]; then
  SUMMARY_OUT="$BUILD_DIR/preview_evidence_summary.md"
fi
if [[ -z "$SUMMARY_JSON_OUT" ]]; then
  SUMMARY_JSON_OUT="$BUILD_DIR/preview_evidence_summary.json"
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
  src/core/scene_catalog.c
  src/core/runtime_texture.c
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
  src/tools/preview_parse.c
  src/tools/preview_cli.c
  src/tools/preview_report.c
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

cat >"$REQUEST_PATH" <<EOF
preview_protocol=v1
project_dir=.
pack=assets/demo/content-demo.vnpak
screenshot=$SCREENSHOT_PATH
scene_name=Opening
backend=scalar
resolution=64x48
frames=8
dt_ms=40
trace=1
command=step_frame:8
command=capture_screenshot
EOF

printf 'response=%s\n' "$RESPONSE_PATH" >>"$REQUEST_PATH"

run_capture "run-preview" "$PREVIEW_BIN" --request "$REQUEST_PATH" --response "$RESPONSE_PATH"

if [[ ! -f "$RESPONSE_PATH" ]]; then
  echo "trace_id=release.preview.response.missing error_code=-2 error_name=VN_E_IO path=$RESPONSE_PATH message=preview response missing" >&2
  exit 1
fi

head_short="$(git rev-parse --short HEAD)"
branch_name="$(git branch --show-current)"

python3 - "$RESPONSE_PATH" "$REQUEST_PATH" "$SCREENSHOT_PATH" "$SUMMARY_JSON_OUT" "$SUMMARY_OUT" "$PREVIEW_BIN" "$BUILD_DIR" "$LOG_DIR" "$head_short" "$branch_name" <<'PY'
import json
import sys
import zlib
from pathlib import Path


def fail(message: str) -> None:
    print(
        f"trace_id=release.preview.response.invalid error_code=-3 error_name=VN_E_FORMAT "
        f"path={response_path} message={message}",
        file=sys.stderr,
    )
    raise SystemExit(1)


(
    response_path,
    request_path,
    screenshot_path,
    summary_json_path,
    summary_md_path,
    preview_bin,
    build_dir,
    log_dir,
    head_short,
    branch_name,
) = sys.argv[1:]

response = json.loads(Path(response_path).read_text(encoding="utf-8"))
request_text = Path(request_path).read_text(encoding="utf-8")
screenshot_file = Path(screenshot_path)

request_info = response.get("request", {})
final_state = response.get("final_state", {})
response_summary = response.get("summary", {})
screenshot = response.get("screenshot") or {}
events = response.get("events")

ppm_header = b"P6\n64 48\n255\n"
ppm_bytes = screenshot_file.read_bytes() if screenshot_file.is_file() else b""
ppm_payload = ppm_bytes[len(ppm_header):] if ppm_bytes.startswith(ppm_header) else b""
payload_crc = f"{zlib.crc32(ppm_payload) & 0xFFFFFFFF:08x}"
expected_screenshot_crc = "995ff007"

checks = {
    "preview_protocol_v1": response.get("preview_protocol") == "v1",
    "status_ok": response.get("status") == "ok",
    "trace_id_ok": response.get("trace_id") == "preview.ok",
    "scene_name_opening": request_info.get("scene_name") == "Opening",
    "content_pack_selected": request_info.get("pack_path") == "./assets/demo/content-demo.vnpak",
    "scalar_backend_selected": final_state.get("backend_name") == "scalar",
    "screenshot_count_1": response_summary.get("screenshot_count") == 1,
    "screenshot_path_matches": screenshot.get("path") == screenshot_path,
    "screenshot_dimensions_64x48": screenshot.get("width") == 64 and screenshot.get("height") == 48,
    "screenshot_ppm_valid": len(ppm_payload) == 64 * 48 * 3,
    "screenshot_crc_matches_payload": screenshot.get("crc32") == payload_crc,
    "screenshot_golden_crc": payload_crc == expected_screenshot_crc,
    "events_present": isinstance(events, list) and len(events) > 0,
    "dirty_tile_total_present": "dirty_tile_total" in final_state,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    fail("preview response checks failed failed_checks=" + ",".join(failed))

summary = {
    "trace_id": "release.preview.ok",
    "status": "ok",
    "head": head_short,
    "branch": branch_name,
    "build_dir": build_dir,
    "log_dir": log_dir,
    "preview_bin": preview_bin,
    "request": request_path,
    "response": response_path,
    "screenshot": screenshot_path,
    "summary_md": summary_md_path,
    "summary_json": summary_json_path,
    "checks": checks,
    "request_lines": request_text.splitlines(),
    "response_facts": {
        "host_os": response.get("host_os"),
        "host_arch": response.get("host_arch"),
        "scene_name": request_info.get("scene_name"),
        "pack_path": request_info.get("pack_path"),
        "backend_name": final_state.get("backend_name"),
        "frames_executed": final_state.get("frames_executed"),
        "dirty_tile_total": final_state.get("dirty_tile_total"),
        "screenshot_path": screenshot.get("path"),
        "screenshot_width": screenshot.get("width"),
        "screenshot_height": screenshot.get("height"),
        "screenshot_crc32": screenshot.get("crc32"),
        "events_count": len(events),
    },
}

Path(summary_json_path).write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
PY

{
  echo "# Preview Evidence Summary"
  echo
  echo "- Trace ID: \`release.preview.ok\`"
  echo "- Status: \`ok\`"
  echo "- Head: \`$head_short\`"
  echo "- Branch: \`$branch_name\`"
  echo "- Build dir: \`$BUILD_DIR\`"
  echo "- Preview bin: \`$PREVIEW_BIN\`"
  echo "- Request: \`$REQUEST_PATH\`"
  echo "- Response: \`$RESPONSE_PATH\`"
  echo "- Screenshot: \`$SCREENSHOT_PATH\`"
  echo "- Summary JSON: \`$SUMMARY_JSON_OUT\`"
  echo
  echo "## Request"
  echo
  sed 's/^/- /' "$REQUEST_PATH"
  echo
  echo "## Response Checks"
  echo
  echo "1. \`preview_protocol=v1\`"
  echo "2. \`status=ok\`"
  echo "3. \`trace_id=preview.ok\`"
  echo "4. \`scene_name=Opening\` from \`assets/demo/content-demo.vnpak\`"
  echo "5. scalar screenshot \`64x48\` with CRC32 \`995ff007\`"
  echo "6. screenshot PPM payload and response CRC agree"
  echo "7. \`events\` present"
  echo "8. \`dirty_tile_total\` present"
} >"$SUMMARY_OUT"

echo "trace_id=release.preview.ok summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT response=$RESPONSE_PATH screenshot=$SCREENSHOT_PATH"
