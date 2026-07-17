#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_remote}"
RELEASE_SPEC="${RELEASE_SPEC:-$ROOT_DIR/docs/release-publish-v1.1.0.json}"
RELEASE_JSON=""
RELEASE_JSON_URL=""
GITHUB_REPO=""
INPUT_MODE=""
TAG_NAME=""
API_ROOT="${API_ROOT:-https://api.github.com}"
TOKEN_ENV="${TOKEN_ENV:-GITHUB_TOKEN}"
SUMMARY_OUT=""
SUMMARY_JSON_OUT=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_remote_summary.sh (--release-json <path> | --release-json-url <url> | --github-repo <owner/repo>) [--tag <tag>] [--api-root <url>] [--token-env <env>] [--release-spec <path>] [--out-dir <dir>] [--summary-out <path>] [--summary-json-out <path>]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_JSON="$1"
      shift
      ;;
    --release-json-url)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_JSON_URL="$1"
      shift
      ;;
    --github-repo)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      GITHUB_REPO="$1"
      shift
      ;;
    --tag)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      TAG_NAME="$1"
      shift
      ;;
    --api-root)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      API_ROOT="$1"
      shift
      ;;
    --token-env)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      TOKEN_ENV="$1"
      shift
      ;;
    --release-spec)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_SPEC="$1"
      shift
      ;;
    --out-dir)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      OUT_DIR="$1"
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
      exit 2
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

source_spec_defaults() {
  eval "$(python3 tools/validate/release_spec.py --shell "$RELEASE_SPEC")"
  mapfile -t SPEC_ASSET_NAMES < <(python3 tools/validate/release_spec.py --asset-names "$RELEASE_SPEC")
}

if [[ -z "$RELEASE_JSON" && -z "$RELEASE_JSON_URL" && -z "$GITHUB_REPO" ]]; then
  usage
  exit 2
fi
mode_count=0
[[ -n "$RELEASE_JSON" ]] && mode_count=$((mode_count + 1))
[[ -n "$RELEASE_JSON_URL" ]] && mode_count=$((mode_count + 1))
[[ -n "$GITHUB_REPO" ]] && mode_count=$((mode_count + 1))
if [[ $mode_count -ne 1 ]]; then
  usage
  exit 2
fi
if [[ -n "$RELEASE_JSON" ]]; then
  INPUT_MODE="release_json"
elif [[ -n "$RELEASE_JSON_URL" ]]; then
  INPUT_MODE="release_json_url"
else
  INPUT_MODE="github_repo"
fi

mkdir -p "$OUT_DIR"
if [[ -z "$SUMMARY_OUT" ]]; then
  SUMMARY_OUT="$OUT_DIR/release_remote_summary.md"
fi
if [[ -z "$SUMMARY_JSON_OUT" ]]; then
  SUMMARY_JSON_OUT="$OUT_DIR/release_remote_summary.json"
fi

source_spec_defaults
if [[ -z "$TAG_NAME" ]]; then
  TAG_NAME="$SPEC_TAG"
fi
if [[ "$INPUT_MODE" == "github_repo" && -z "$GITHUB_REPO" ]]; then
  GITHUB_REPO="$SPEC_REPOSITORY"
fi

if [[ "$INPUT_MODE" == "release_json_url" ]]; then
  RELEASE_JSON="$OUT_DIR/release_remote_state.json"
  curl -fsSL "$RELEASE_JSON_URL" -o "$RELEASE_JSON"
fi

if [[ "$INPUT_MODE" == "github_repo" ]]; then
  RELEASE_JSON="$OUT_DIR/release_remote_state.json"
  RELEASE_JSON_URL="$API_ROOT/repos/$GITHUB_REPO/releases/tags/$TAG_NAME"
  curl_args=(-fsSL -H "Accept: application/vnd.github+json")
  token_value="${!TOKEN_ENV:-}"
  if [[ -n "$token_value" ]]; then
    curl_args+=(-H "Authorization: Bearer $token_value")
  fi
  curl "${curl_args[@]}" "$RELEASE_JSON_URL" -o "$RELEASE_JSON"
fi

python3 tools/validate/validate_release_remote_state.py --release-spec "$RELEASE_SPEC" --release-json "$RELEASE_JSON" >/tmp/n64gal_release_remote_validate.log

eval "$(
python3 - "$RELEASE_JSON" <<'PY'
import json
import shlex
import sys

remote = json.load(open(sys.argv[1], "r", encoding="utf-8"))
fields = {
    "REMOTE_TAG": remote["tag_name"],
    "REMOTE_HTML_URL": remote["html_url"],
}
for key, value in fields.items():
    print(f"{key}={shlex.quote(str(value))}")
PY
)"
mapfile -t REMOTE_ASSET_SIZES < <(
  python3 - "$RELEASE_JSON" "${SPEC_ASSET_NAMES[@]}" <<'PY'
import json
import sys

remote = json.load(open(sys.argv[1], "r", encoding="utf-8"))
assets = {entry["name"]: entry for entry in remote["assets"]}
for name in sys.argv[2:]:
    print(assets[name]["size"])
PY
)
mapfile -t REMOTE_ASSET_URLS < <(
  python3 - "$RELEASE_JSON" "${SPEC_ASSET_NAMES[@]}" <<'PY'
import json
import sys

remote = json.load(open(sys.argv[1], "r", encoding="utf-8"))
assets = {entry["name"]: entry for entry in remote["assets"]}
for name in sys.argv[2:]:
    print(assets[name]["browser_download_url"])
PY
)

{
  echo "# Release Remote Summary"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Release spec: \`$RELEASE_SPEC\`"
  echo "- Release json: \`$RELEASE_JSON\`"
  if [[ -n "$RELEASE_JSON_URL" ]]; then
    echo "- Release json url: \`$RELEASE_JSON_URL\`"
  fi
  echo
  echo "## Remote Match"
  echo
  echo "1. Tag: \`$REMOTE_TAG\`"
  echo "2. Release URL: \`$REMOTE_HTML_URL\`"
  echo "3. Assets: \`${#SPEC_ASSET_NAMES[@]}\`"
  for i in "${!SPEC_ASSET_NAMES[@]}"; do
    echo "$((i + 4)). \`${SPEC_ASSET_NAMES[$i]}\`: size=\`${REMOTE_ASSET_SIZES[$i]}\`, url=\`${REMOTE_ASSET_URLS[$i]}\`"
  done
} >"$SUMMARY_OUT"

{
  printf '{\n'
  printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
  printf '  "branch": "%s",\n' "$(git branch --show-current)"
  printf '  "release_spec": "%s",\n' "$RELEASE_SPEC"
  printf '  "release_json": "%s",\n' "$RELEASE_JSON"
  printf '  "release_json_url": "%s",\n' "$RELEASE_JSON_URL"
  printf '  "tag": "%s",\n' "$REMOTE_TAG"
  printf '  "release_url": "%s",\n' "$REMOTE_HTML_URL"
  printf '  "asset": {"name":"%s","size":%s,"url":"%s"},\n' "${SPEC_ASSET_NAMES[0]}" "${REMOTE_ASSET_SIZES[0]}" "${REMOTE_ASSET_URLS[0]}"
  printf '  "assets": [\n'
  for i in "${!SPEC_ASSET_NAMES[@]}"; do
    printf '    {"name":"%s","size":%s,"url":"%s"}' "${SPEC_ASSET_NAMES[$i]}" "${REMOTE_ASSET_SIZES[$i]}" "${REMOTE_ASSET_URLS[$i]}"
    if [[ $i -lt $((${#SPEC_ASSET_NAMES[@]} - 1)) ]]; then
      printf ','
    fi
    printf '\n'
  done
  printf '  ],\n'
  printf '  "summary_md": "%s",\n' "$SUMMARY_OUT"
  printf '  "summary_json": "%s"\n' "$SUMMARY_JSON_OUT"
  printf '}\n'
} >"$SUMMARY_JSON_OUT"

echo "trace_id=release.remote_summary.ok summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT release_json=$RELEASE_JSON"
