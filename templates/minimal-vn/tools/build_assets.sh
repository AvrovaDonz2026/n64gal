#!/usr/bin/env bash
set -euo pipefail

TEMPLATE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROOT_DIR="$(cd "$TEMPLATE_DIR/../.." && pwd)"
SOURCE_SCRIPTS_DIR="$TEMPLATE_DIR/assets/scripts"
IMAGES_MANIFEST="$TEMPLATE_DIR/assets/images/images.json"
OUT_DIR="$TEMPLATE_DIR/build"
COMPILED_SCRIPTS_DIR="$OUT_DIR/scripts"

mkdir -p "$OUT_DIR" "$COMPILED_SCRIPTS_DIR"
rm -f "$SOURCE_SCRIPTS_DIR"/*.vns.bin
rm -f "$COMPILED_SCRIPTS_DIR"/*.vns.bin "$OUT_DIR/minimal.vnpak" "$OUT_DIR/manifest.json"

for src in "$SOURCE_SCRIPTS_DIR"/*.vns.txt; do
  name="$(basename "$src" .vns.txt)"
  python3 "$ROOT_DIR/tools/scriptc/compile_vns.py" "$src" "$COMPILED_SCRIPTS_DIR/$name.vns.bin"
done

python3 "$ROOT_DIR/tools/packer/make_demo_pack.py" \
  --scripts-dir "$COMPILED_SCRIPTS_DIR" \
  --images-manifest "$IMAGES_MANIFEST" \
  --out "$OUT_DIR/minimal.vnpak" \
  --manifest-out "$OUT_DIR/manifest.json"

echo "[template] wrote $OUT_DIR/minimal.vnpak"
echo "[template] wrote $OUT_DIR/manifest.json"
echo "[template] wrote $COMPILED_SCRIPTS_DIR/*.vns.bin"
