#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

PLATFORM_DOC="${PLATFORM_DOC:-$ROOT_DIR/docs/platform-matrix.md}"
CI_SUITE_SUMMARY="${CI_SUITE_SUMMARY:-$ROOT_DIR/build_ci_cc/ci_suite_summary.md}"
OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_platform}"
SUMMARY_OUT=""
SUMMARY_JSON_OUT=""

usage() {
  cat >&2 <<'USAGE'
usage: scripts/release/run_platform_evidence.sh [--out-dir <dir>] [--platform-doc <path>] [--ci-suite-summary <path>] [--summary-out <path>] [--summary-json-out <path>]
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out-dir)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      OUT_DIR="$1"
      shift
      ;;
    --platform-doc)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PLATFORM_DOC="$1"
      shift
      ;;
    --ci-suite-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      CI_SUITE_SUMMARY="$1"
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

mkdir -p "$OUT_DIR"
if [[ -z "$SUMMARY_OUT" ]]; then
  SUMMARY_OUT="$OUT_DIR/platform_evidence_summary.md"
fi
if [[ -z "$SUMMARY_JSON_OUT" ]]; then
  SUMMARY_JSON_OUT="$OUT_DIR/platform_evidence_summary.json"
fi

require_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "trace_id=release.platform.missing error_code=-2 error_name=VN_E_IO path=$path message=required platform evidence input missing" >&2
    exit 1
  fi
}

require_file "$PLATFORM_DOC"
require_file "$CI_SUITE_SUMMARY"

linux_x64_line="$(grep -F '| `amd64` | Linux | `avx2 -> scalar` |' "$PLATFORM_DOC" | head -n 1 || true)"
windows_x64_line="$(grep -F '| `amd64` | Windows | `avx2 -> scalar` |' "$PLATFORM_DOC" | head -n 1 || true)"
linux_arm64_line="$(grep -F '| `arm64` | Linux | `neon -> scalar` |' "$PLATFORM_DOC" | head -n 1 || true)"
windows_arm64_line="$(grep -F '| `arm64` | Windows | `neon -> scalar` |' "$PLATFORM_DOC" | head -n 1 || true)"
riscv64_line="$(grep -F '| `riscv64` | Linux | `rvv -> scalar` |' "$PLATFORM_DOC" | head -n 1 || true)"

if [[ -z "$linux_x64_line" || -z "$windows_x64_line" || -z "$linux_arm64_line" || -z "$windows_arm64_line" || -z "$riscv64_line" ]]; then
  echo "trace_id=release.platform.matrix.invalid error_code=-3 error_name=VN_E_FORMAT message=platform matrix rows missing" >&2
  exit 1
fi

ci_status_line="$(grep 'Status:' "$CI_SUITE_SUMMARY" | head -n 1 || true)"
if [[ -z "$ci_status_line" ]]; then
  echo "trace_id=release.platform.ci.invalid error_code=-3 error_name=VN_E_FORMAT message=ci suite summary missing status" >&2
  exit 1
fi

head_short="$(git rev-parse --short HEAD)"
branch_name="$(git branch --show-current)"

{
  echo "# Platform Evidence Summary"
  echo
  echo "- Trace ID: \`release.platform.ok\`"
  echo "- Status: \`ok\`"
  echo "- Head: \`$head_short\`"
  echo "- Branch: \`$branch_name\`"
  echo "- Platform doc: \`$PLATFORM_DOC\`"
  echo "- CI suite summary: \`$CI_SUITE_SUMMARY\`"
  echo "- Summary JSON: \`$SUMMARY_JSON_OUT\`"
  echo
  echo "## Platform Rows"
  echo
  echo "1. $linux_x64_line"
  echo "2. $windows_x64_line"
  echo "3. $linux_arm64_line"
  echo "4. $windows_arm64_line"
  echo "5. $riscv64_line"
  echo
  echo "## Current Release-Like Commands"
  echo
  echo "1. \`python3 tools/toolchain.py release-gate --with-soak ...\`"
  echo "2. \`python3 tools/toolchain.py release-soak --runner-bin <path> ...\`"
  echo "3. \`python3 tools/toolchain.py release-bundle --out-dir <dir>\`"
  echo "4. \`python3 tools/toolchain.py release-report --out-dir <dir>\`"
  echo
  echo "## CI Summary"
  echo
  echo "- $ci_status_line"
} >"$SUMMARY_OUT"

python3 - "$SUMMARY_JSON_OUT" "$head_short" "$branch_name" "$PLATFORM_DOC" "$CI_SUITE_SUMMARY" "$SUMMARY_OUT" "$SUMMARY_JSON_OUT" "$ci_status_line" "$linux_x64_line" "$windows_x64_line" "$linux_arm64_line" "$windows_arm64_line" "$riscv64_line" <<'PY'
import json
import sys


def parse_row(row: str) -> dict:
    cells = [cell.strip() for cell in row.strip().strip("|").split("|")]
    if len(cells) != 6:
        raise SystemExit(
            "trace_id=release.platform.matrix.invalid error_code=-3 error_name=VN_E_FORMAT "
            "message=platform matrix row parse failed"
        )
    return {
        "arch": cells[0].strip("`"),
        "os": cells[1],
        "backend_policy": cells[2].strip("`"),
        "status": cells[3],
        "evidence": cells[4].strip("`"),
        "artifacts": cells[5],
        "raw": row,
    }


(
    out_path,
    head_short,
    branch_name,
    platform_doc,
    ci_suite_summary,
    summary_md,
    summary_json,
    ci_status_line,
    *rows_raw,
) = sys.argv[1:]

payload = {
    "trace_id": "release.platform.ok",
    "status": "ok",
    "head": head_short,
    "branch": branch_name,
    "platform_doc": platform_doc,
    "ci_suite_summary": ci_suite_summary,
    "summary_md": summary_md,
    "summary_json": summary_json,
    "ci_status": ci_status_line[2:].strip() if ci_status_line.startswith("- ") else ci_status_line.strip(),
    "release_commands": [
        "python3 tools/toolchain.py release-gate --with-soak ...",
        "python3 tools/toolchain.py release-soak --runner-bin <path> ...",
        "python3 tools/toolchain.py release-bundle --out-dir <dir>",
        "python3 tools/toolchain.py release-report --out-dir <dir>",
    ],
    "rows": [parse_row(row) for row in rows_raw],
}

with open(out_path, "w", encoding="utf-8") as f:
    json.dump(payload, f, indent=2, sort_keys=True)
    f.write("\n")
PY

echo "trace_id=release.platform.ok summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT ci_summary=$CI_SUITE_SUMMARY"
