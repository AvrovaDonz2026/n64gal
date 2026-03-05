#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_PATH="${1:-$ROOT_DIR/assets/demo/demo.vnpak}"
OUT_DIR="$(dirname "$OUT_PATH")"

mkdir -p "$OUT_DIR"

# vnpak layout (little-endian):
# header: magic(4) version(u16) entry_count(u16)
# entry : type(u8) flags(u8) width(u16) height(u16) off(u32) size(u32)
# payload follows table
{
  printf '\x56\x4e\x50\x4b\x01\x00\x02\x00'
  printf '\x01\x00\x58\x02\x20\x03\x24\x00\x00\x00\x04\x00\x00\x00'
  printf '\x02\x01\x80\x02\xe0\x01\x28\x00\x00\x00\x08\x00\x00\x00'
  printf '\x00\x11\x22\x33'
  printf '\xaa\xbb\xcc\xdd\xee\xff\x10\x20'
} > "$OUT_PATH"

printf '[vnpak] wrote %s (%s bytes)\n' "$OUT_PATH" "$(wc -c < "$OUT_PATH")"
