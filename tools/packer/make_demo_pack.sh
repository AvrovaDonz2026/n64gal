#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_PATH="${1:-$ROOT_DIR/assets/demo/demo.vnpak}"
MANIFEST_PATH="${2:-$ROOT_DIR/assets/demo/manifest.json}"
IMAGES_MANIFEST_PATH="${3:-$ROOT_DIR/assets/demo/images/images.json}"

"$ROOT_DIR/tools/scriptc/build_demo_scripts.sh"
if [[ -f "$IMAGES_MANIFEST_PATH" ]]; then
  python3 "$ROOT_DIR/tools/packer/make_demo_pack.py" \
    --scripts-dir "$ROOT_DIR/assets/demo/scripts" \
    --images-manifest "$IMAGES_MANIFEST_PATH" \
    --out "$OUT_PATH" \
    --manifest-out "$MANIFEST_PATH"
else
  python3 "$ROOT_DIR/tools/packer/make_demo_pack.py" \
    --scripts-dir "$ROOT_DIR/assets/demo/scripts" \
    --out "$OUT_PATH" \
    --manifest-out "$MANIFEST_PATH"
fi
