#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_PATH="${1:-$ROOT_DIR/assets/demo/demo.vnpak}"

"$ROOT_DIR/tools/scriptc/build_demo_scripts.sh"
python3 "$ROOT_DIR/tools/packer/make_demo_pack.py" \
  --scripts-dir "$ROOT_DIR/assets/demo/scripts" \
  --out "$OUT_PATH"
