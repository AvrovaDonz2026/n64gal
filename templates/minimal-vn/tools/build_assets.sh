#!/usr/bin/env bash
set -euo pipefail

TEMPLATE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROOT_DIR="$(cd "$TEMPLATE_DIR/../.." && pwd)"

python3 "$ROOT_DIR/tools/toolchain.py" build-project "$TEMPLATE_DIR"
