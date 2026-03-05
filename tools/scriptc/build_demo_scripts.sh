#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT_DIR="$ROOT_DIR/assets/demo/scripts"
COMPILER="$ROOT_DIR/tools/scriptc/compile_vns.py"

python3 "$COMPILER" "$SCRIPT_DIR/S0.vns.txt" "$SCRIPT_DIR/S0.vns.bin"
python3 "$COMPILER" "$SCRIPT_DIR/S1.vns.txt" "$SCRIPT_DIR/S1.vns.bin"
python3 "$COMPILER" "$SCRIPT_DIR/S2.vns.txt" "$SCRIPT_DIR/S2.vns.bin"
python3 "$COMPILER" "$SCRIPT_DIR/S3.vns.txt" "$SCRIPT_DIR/S3.vns.bin"

echo "[scriptc] demo scripts ready"
