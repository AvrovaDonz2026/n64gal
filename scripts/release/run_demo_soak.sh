#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

SCENES="S0,S1,S2,S3,S10"
SCENE_DURATION_SEC=180
FRAMES_PER_SCENE=""
DT_MS=16
BACKEND="auto"
PACK_PATH="assets/demo/demo.vnpak"
RESOLUTION="600x800"
SUMMARY_OUT=""
SUMMARY_JSON_OUT=""
RUNNER_BIN=""
SKIP_BUILD=0
SKIP_PACK=0

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_demo_soak.sh [options]

options:
  --scenes <S0,S1,...>
  --scene-duration-sec <sec>
  --frames-per-scene <frames>
  --dt-ms <ms>
  --backend <name>
  --pack <path>
  --resolution <WxH>
  --runner-bin <path>
  --summary-out <path>
  --summary-json-out <path>
  --skip-build
  --skip-pack
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --scenes)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      SCENES="$1"
      shift
      ;;
    --scene-duration-sec)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      SCENE_DURATION_SEC="$1"
      shift
      ;;
    --frames-per-scene)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      FRAMES_PER_SCENE="$1"
      shift
      ;;
    --dt-ms)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      DT_MS="$1"
      shift
      ;;
    --backend)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      BACKEND="$1"
      shift
      ;;
    --pack)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PACK_PATH="$1"
      shift
      ;;
    --resolution)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RESOLUTION="$1"
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
    --runner-bin)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RUNNER_BIN="$1"
      shift
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --skip-pack)
      SKIP_PACK=1
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

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_release_soak}"
LOG_DIR="$BUILD_DIR/logs"
TMP_BUILD_DIR="$BUILD_DIR/tmp"
PLAYER_BIN="$BUILD_DIR/vn_player"
if [[ -z "$SUMMARY_OUT" ]]; then
  SUMMARY_OUT="$BUILD_DIR/demo_soak_summary.md"
fi
if [[ -z "$SUMMARY_JSON_OUT" ]]; then
  SUMMARY_JSON_OUT="$BUILD_DIR/demo_soak_summary.json"
fi
if [[ -n "$RUNNER_BIN" ]]; then
  PLAYER_BIN="$RUNNER_BIN"
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

run_log_step() {
  local name="$1"
  shift
  local slug
  slug="$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]' | tr ' /' '__')"
  local log_path="$LOG_DIR/${slug}.log"
  echo "[demo-soak] $name"
  "$@" >"$log_path" 2>&1
  cat "$log_path"
}

write_summary() {
  local status="$1"
  local frames_value="$2"
  local scene_lines="$3"
  local scenes_json="$4"
  {
    echo "# Demo Soak Summary"
    echo
    echo "- Status: \`$status\`"
    echo "- Head: \`$(git rev-parse --short HEAD)\`"
    echo "- Branch: \`$(git branch --show-current)\`"
    echo "- Backend: \`$BACKEND\`"
    echo "- Pack: \`$PACK_PATH\`"
    echo "- Resolution: \`$RESOLUTION\`"
    echo "- DT ms: \`$DT_MS\`"
    echo "- Scenes: \`$SCENES\`"
    echo "- Frames per scene: \`$frames_value\`"
    echo "- Runner bin: \`$PLAYER_BIN\`"
    echo "- Build dir: \`$BUILD_DIR\`"
    echo "- Log dir: \`$LOG_DIR\`"
    echo
    echo "## Scenes"
    echo
    printf "%s" "$scene_lines"
  } >"$SUMMARY_OUT"
  {
    printf '{\n'
    printf '  "status": "%s",\n' "$status"
    printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
    printf '  "branch": "%s",\n' "$(git branch --show-current)"
    printf '  "backend": "%s",\n' "$BACKEND"
    printf '  "pack": "%s",\n' "$PACK_PATH"
    printf '  "resolution": "%s",\n' "$RESOLUTION"
    printf '  "dt_ms": %s,\n' "$DT_MS"
    printf '  "frames_per_scene": %s,\n' "$frames_value"
    printf '  "runner_bin": "%s",\n' "$PLAYER_BIN"
    printf '  "summary_md": "%s",\n' "$SUMMARY_OUT"
    printf '  "scenes": [%s]\n' "$scenes_json"
    printf '}\n'
  } >"$SUMMARY_JSON_OUT"
}

if [[ $SKIP_PACK -eq 0 ]]; then
  run_log_step "build-demo-scripts" ./tools/scriptc/build_demo_scripts.sh
  run_log_step "make-demo-pack" ./tools/packer/make_demo_pack.sh
fi

if [[ -z "$RUNNER_BIN" && $SKIP_BUILD -eq 0 ]]; then
  run_log_step "build-vn-player" "$CC_BIN" "${CFLAGS[@]}" src/main.c "${COMMON_SRC[@]}" -o "$PLAYER_BIN"
fi

if [[ ! -x "$PLAYER_BIN" ]]; then
  echo "trace_id=release.soak.binary.missing error_code=-2 error_name=VN_E_IO message=vn_player binary missing" >&2
  exit 1
fi

if [[ -z "$FRAMES_PER_SCENE" ]]; then
  FRAMES_PER_SCENE=$(( (SCENE_DURATION_SEC * 1000 + DT_MS - 1) / DT_MS ))
fi
if [[ "$FRAMES_PER_SCENE" -le 0 ]]; then
  echo "trace_id=release.soak.frames.invalid error_code=-1 error_name=VN_E_INVALID_ARG message=frames-per-scene must be > 0" >&2
  exit 2
fi

IFS=',' read -r -a SCENE_LIST <<<"$SCENES"
SCENE_SUMMARY=""
SCENE_JSON=""
STATUS="success"

for scene in "${SCENE_LIST[@]}"; do
  scene="$(printf '%s' "$scene" | tr -d '[:space:]')"
  [[ -n "$scene" ]] || continue
  log_path="$LOG_DIR/scene_${scene}.log"
  echo "[demo-soak] scene=$scene frames=$FRAMES_PER_SCENE backend=$BACKEND"
  if ! "$PLAYER_BIN" \
      --pack "$PACK_PATH" \
      --scene "$scene" \
      --resolution "$RESOLUTION" \
      --backend "$BACKEND" \
      --frames "$FRAMES_PER_SCENE" \
      --dt-ms "$DT_MS" \
      --hold-end >"$log_path" 2>&1; then
    STATUS="failed"
    SCENE_SUMMARY="${SCENE_SUMMARY}- \`$scene\`: failed, see \`$log_path\`\n"
    write_summary "$STATUS" "$FRAMES_PER_SCENE" "$SCENE_SUMMARY" "$SCENE_JSON"
    cat "$log_path"
    echo "trace_id=release.soak.scene.failed scene=$scene error_code=-3 error_name=VN_E_FORMAT message=scene soak failed" >&2
    exit 1
  fi
  cat "$log_path"
  summary_line="$(grep 'trace_id=runtime.run.ok' "$log_path" | tail -n 1 || true)"
  if [[ -z "$summary_line" ]]; then
    STATUS="failed"
    SCENE_SUMMARY="${SCENE_SUMMARY}- \`$scene\`: missing runtime summary, see \`$log_path\`\n"
    write_summary "$STATUS" "$FRAMES_PER_SCENE" "$SCENE_SUMMARY" "$SCENE_JSON"
    echo "trace_id=release.soak.scene.summary_missing scene=$scene error_code=-3 error_name=VN_E_FORMAT message=scene summary missing" >&2
    exit 1
  fi
  SCENE_SUMMARY="${SCENE_SUMMARY}- \`$scene\`: \`$summary_line\`\n"
  if [[ -n "$SCENE_JSON" ]]; then
    SCENE_JSON="${SCENE_JSON}, "
  fi
  SCENE_JSON="${SCENE_JSON}\"${scene}\""
done

write_summary "$STATUS" "$FRAMES_PER_SCENE" "$SCENE_SUMMARY" "$SCENE_JSON"
echo "trace_id=release.soak.ok summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT scenes=$SCENES frames_per_scene=$FRAMES_PER_SCENE backend=$BACKEND"
