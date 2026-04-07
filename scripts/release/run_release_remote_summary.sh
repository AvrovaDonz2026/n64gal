#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_remote}"
RELEASE_SPEC="${RELEASE_SPEC:-$ROOT_DIR/docs/release-publish-v1.0.0.json}"
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
  eval "$(
  python3 - "$RELEASE_SPEC" <<'PY'
import json
import shlex
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    payload = json.load(handle)

fields = {
    "SPEC_TAG": payload.get("tag", ""),
    "SPEC_RELEASE_URL": payload.get("release_url", ""),
    "SPEC_REPOSITORY": payload.get("repository", ""),
}
for key, value in fields.items():
    print(f"{key}={shlex.quote(str(value))}")
PY
  )"
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
python3 - "$RELEASE_SPEC" "$RELEASE_JSON" <<'PY'
import json
import shlex
import sys

spec = json.load(open(sys.argv[1], "r", encoding="utf-8"))
remote = json.load(open(sys.argv[2], "r", encoding="utf-8"))
asset_path = spec["asset"]["path"]
asset_name = asset_path.split("/")[-1]
asset_entry = next(item for item in remote["assets"] if item["name"] == asset_name)
fields = {
    "REMOTE_TAG": remote["tag_name"],
    "REMOTE_HTML_URL": remote["html_url"],
    "REMOTE_ASSET_NAME": asset_entry["name"],
    "REMOTE_ASSET_URL": asset_entry["browser_download_url"],
    "REMOTE_ASSET_SIZE": str(asset_entry["size"]),
}
for key, value in fields.items():
    print(f"{key}={shlex.quote(str(value))}")
PY
)"

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
  echo "3. Asset: \`$REMOTE_ASSET_NAME\`"
  echo "4. Asset size: \`$REMOTE_ASSET_SIZE\`"
  echo "5. Asset URL: \`$REMOTE_ASSET_URL\`"
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
  printf '  "asset": {"name":"%s","size":%s,"url":"%s"},\n' "$REMOTE_ASSET_NAME" "$REMOTE_ASSET_SIZE" "$REMOTE_ASSET_URL"
  printf '  "summary_md": "%s",\n' "$SUMMARY_OUT"
  printf '  "summary_json": "%s"\n' "$SUMMARY_JSON_OUT"
  printf '}\n'
} >"$SUMMARY_JSON_OUT"

echo "trace_id=release.remote_summary.ok summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT release_json=$RELEASE_JSON"
