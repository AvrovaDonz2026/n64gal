#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_publish}"
RELEASE_SPEC="${RELEASE_SPEC:-$ROOT_DIR/docs/release-publish-v0.1.0-alpha.json}"
TAG_NAME=""
RELEASE_URL=""
RELEASE_NOTE=""
ASSET_PATH=""
BUNDLE_INDEX="${BUNDLE_INDEX:-$ROOT_DIR/build_release_bundle/release_bundle_index.md}"
BUNDLE_MANIFEST="${BUNDLE_MANIFEST:-$ROOT_DIR/build_release_bundle/release_bundle_manifest.json}"
REPORT_JSON="${REPORT_JSON:-$ROOT_DIR/build_release_report/release_report.json}"
MAP_OUT=""
MAP_JSON_OUT=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_publish_map.sh [--out-dir <dir>] [--release-spec <path>] [--tag <tag>] [--release-url <url>] [--release-note <path>] [--asset <path>] [--bundle-index <path>] [--bundle-manifest <path>] [--report-json <path>] [--map-out <path>] [--map-json-out <path>]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out-dir)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      OUT_DIR="$1"
      shift
      ;;
    --release-spec)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_SPEC="$1"
      shift
      ;;
    --tag)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      TAG_NAME="$1"
      shift
      ;;
    --release-url)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_URL="$1"
      shift
      ;;
    --release-note)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_NOTE="$1"
      shift
      ;;
    --asset)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      ASSET_PATH="$1"
      shift
      ;;
    --bundle-index)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      BUNDLE_INDEX="$1"
      shift
      ;;
    --bundle-manifest)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      BUNDLE_MANIFEST="$1"
      shift
      ;;
    --report-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      REPORT_JSON="$1"
      shift
      ;;
    --map-out)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      MAP_OUT="$1"
      shift
      ;;
    --map-json-out)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      MAP_JSON_OUT="$1"
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

mkdir -p "$OUT_DIR"
if [[ -z "$MAP_OUT" ]]; then
  MAP_OUT="$OUT_DIR/release_publish_map.md"
fi
if [[ -z "$MAP_JSON_OUT" ]]; then
  MAP_JSON_OUT="$OUT_DIR/release_publish_map.json"
fi

require_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "trace_id=release.publish_map.missing error_code=-2 error_name=VN_E_IO path=$path message=required publish map input missing" >&2
    exit 1
  fi
}

require_file "$RELEASE_SPEC"
resolve_spec_path() {
  local value="$1"
  if [[ -z "$value" ]]; then
    printf '%s' ""
  elif [[ "$value" = /* ]]; then
    printf '%s' "$value"
  else
    printf '%s' "$ROOT_DIR/$value"
  fi
}

eval "$(
python3 - "$RELEASE_SPEC" <<'PY'
import json
import shlex
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    payload = json.load(handle)

asset = payload.get("asset", {})
fields = {
    "SPEC_VERSION": payload.get("version", ""),
    "SPEC_TAG": payload.get("tag", ""),
    "SPEC_RELEASE_URL": payload.get("release_url", ""),
    "SPEC_RELEASE_NOTE": payload.get("release_note", ""),
    "SPEC_ASSET_PATH": asset.get("path", ""),
}
for key, value in fields.items():
    print(f"{key}={shlex.quote(str(value))}")
PY
)"

if [[ -z "$TAG_NAME" ]]; then
  TAG_NAME="$SPEC_TAG"
fi
if [[ -z "$RELEASE_URL" ]]; then
  RELEASE_URL="$SPEC_RELEASE_URL"
fi
if [[ -z "$RELEASE_NOTE" ]]; then
  RELEASE_NOTE="$(resolve_spec_path "$SPEC_RELEASE_NOTE")"
fi
if [[ -z "$ASSET_PATH" ]]; then
  ASSET_PATH="$(resolve_spec_path "$SPEC_ASSET_PATH")"
fi

require_file "$RELEASE_NOTE"
require_file "$ASSET_PATH"
require_file "$BUNDLE_INDEX"
require_file "$BUNDLE_MANIFEST"
require_file "$REPORT_JSON"

asset_sha="$(sha256sum "$ASSET_PATH" | awk '{print $1}')"
asset_bytes="$(wc -c <"$ASSET_PATH" | tr -d '[:space:]')"

{
  echo "# Release Publish Map"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Tag: \`$TAG_NAME\`"
  echo "- Release URL: \`$RELEASE_URL\`"
  echo "- Release spec: \`$RELEASE_SPEC\`"
  echo
  echo "## Release Inputs"
  echo
  echo "1. Release spec: \`$RELEASE_SPEC\`"
  echo "2. Release note: \`$RELEASE_NOTE\`"
  echo "3. Asset: \`$ASSET_PATH\`"
  echo "4. Bundle index: \`$BUNDLE_INDEX\`"
  echo "5. Bundle manifest: \`$BUNDLE_MANIFEST\`"
  echo "6. Release report json: \`$REPORT_JSON\`"
  echo
  echo "## Asset Digest"
  echo
  echo "1. Asset bytes: \`$asset_bytes\`"
  echo "2. Asset sha256: \`$asset_sha\`"
} >"$MAP_OUT"

{
  printf '{\n'
  printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
  printf '  "branch": "%s",\n' "$(git branch --show-current)"
  printf '  "release_spec": "%s",\n' "$RELEASE_SPEC"
  printf '  "tag": "%s",\n' "$TAG_NAME"
  printf '  "release_url": "%s",\n' "$RELEASE_URL"
  printf '  "release_note": "%s",\n' "$RELEASE_NOTE"
  printf '  "asset": {"path":"%s","bytes":%s,"sha256":"%s"},\n' "$ASSET_PATH" "$asset_bytes" "$asset_sha"
  printf '  "bundle_index": "%s",\n' "$BUNDLE_INDEX"
  printf '  "bundle_manifest": "%s",\n' "$BUNDLE_MANIFEST"
  printf '  "report_json": "%s",\n' "$REPORT_JSON"
  printf '  "map_md": "%s",\n' "$MAP_OUT"
  printf '  "map_json": "%s"\n' "$MAP_JSON_OUT"
  printf '}\n'
} >"$MAP_JSON_OUT"

echo "trace_id=release.publish_map.ok map=$MAP_OUT map_json=$MAP_JSON_OUT tag=$TAG_NAME release_url=$RELEASE_URL asset=$ASSET_PATH"
