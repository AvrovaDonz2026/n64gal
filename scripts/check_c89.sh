#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "[check_c89] scanning forbidden C99/C11 patterns..."
if grep -R -nE '\bstdint\.h\b|\bstdbool\.h\b|//|for \(int |\binline\b' include src; then
  echo "[check_c89] forbidden patterns found"
  exit 1
fi

echo "[check_c89] compiling headers in C89 mode..."
for h in include/*.h; do
  BASENAME="$(basename "$h")"
  printf '#include "%s"\nint main(void){return 0;}\n' "$BASENAME" \
    | cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -Iinclude -x c -c - -o /tmp/"${BASENAME}.o"
done

echo "[check_c89] OK"
