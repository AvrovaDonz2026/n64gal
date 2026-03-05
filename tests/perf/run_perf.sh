#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BACKEND="scalar"
SCENES="S0,S1,S2,S3"
OUT_DIR="tests/perf"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --backend)
      BACKEND="$2"
      shift 2
      ;;
    --scenes)
      SCENES="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

mkdir -p "$OUT_DIR"

"$ROOT_DIR/tools/packer/make_demo_pack.sh" >/tmp/vn_make_pack.out

cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -Iinclude \
  src/main.c \
  src/core/backend_registry.c \
  src/core/renderer.c \
  src/core/vm.c \
  src/core/pack.c \
  src/frontend/render_ops.c \
  src/backend/avx2/avx2_backend.c \
  src/backend/scalar/scalar_backend.c \
  -o ./vn_player

IFS=',' read -r -a SCENE_ARRAY <<< "$SCENES"

for SCENE in "${SCENE_ARRAY[@]}"; do
  OUT_CSV="$OUT_DIR/perf_${SCENE}.csv"
  START_NS="$(date +%s%N)"
  ./vn_player --backend="$BACKEND" --scene="$SCENE" >/tmp/vn_player_perf.out
  END_NS="$(date +%s%N)"
  FRAME_MS="$(awk "BEGIN { print ($END_NS - $START_NS) / 1000000.0 }")"

  {
    echo "scene,frame,frame_ms,vm_ms,build_ms,raster_ms,audio_ms,rss_mb"
    echo "${SCENE},0,${FRAME_MS},0.00,0.00,0.00,0.00,0.00"
  } > "$OUT_CSV"

  echo "[perf] wrote $OUT_CSV"
done

rm -f ./vn_player
echo "[perf] done backend=$BACKEND scenes=$SCENES"
