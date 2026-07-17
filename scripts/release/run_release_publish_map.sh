#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_publish}"
RELEASE_SPEC="${RELEASE_SPEC:-$ROOT_DIR/docs/release-publish-v1.1.0.json}"
TAG_NAME=""
RELEASE_URL=""
RELEASE_NOTE=""
ASSET_PATHS=()
ASSET_NAMES=()
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
      ASSET_PATHS+=("$1")
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

eval "$(python3 tools/validate/release_spec.py --shell "$RELEASE_SPEC")"
mapfile -t SPEC_ASSET_PATHS < <(python3 tools/validate/release_spec.py --asset-paths "$RELEASE_SPEC")
mapfile -t SPEC_ASSET_NAMES < <(python3 tools/validate/release_spec.py --asset-names "$RELEASE_SPEC")

if [[ -z "$TAG_NAME" ]]; then
  TAG_NAME="$SPEC_TAG"
fi
if [[ -z "$RELEASE_URL" ]]; then
  RELEASE_URL="$SPEC_RELEASE_URL"
fi
if [[ -z "$RELEASE_NOTE" ]]; then
  RELEASE_NOTE="$(resolve_spec_path "$SPEC_RELEASE_NOTE")"
fi
if [[ ${#ASSET_PATHS[@]} -eq 0 ]]; then
  for i in "${!SPEC_ASSET_PATHS[@]}"; do
    ASSET_PATHS+=("$(resolve_spec_path "${SPEC_ASSET_PATHS[$i]}")")
    ASSET_NAMES+=("${SPEC_ASSET_NAMES[$i]}")
  done
else
  for asset_path in "${ASSET_PATHS[@]}"; do
    ASSET_NAMES+=("$(basename "$asset_path")")
  done
fi

require_file "$RELEASE_NOTE"
for asset_path in "${ASSET_PATHS[@]}"; do
  require_file "$asset_path"
done
require_file "$BUNDLE_INDEX"
require_file "$BUNDLE_MANIFEST"
require_file "$REPORT_JSON"

ASSET_SHAS=()
ASSET_BYTES=()
for asset_path in "${ASSET_PATHS[@]}"; do
  ASSET_SHAS+=("$(sha256sum "$asset_path" | awk '{print $1}')")
  ASSET_BYTES+=("$(wc -c <"$asset_path" | tr -d '[:space:]')")
done

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
  echo "3. Assets: \`${#ASSET_PATHS[@]}\`"
  echo "4. Bundle index: \`$BUNDLE_INDEX\`"
  echo "5. Bundle manifest: \`$BUNDLE_MANIFEST\`"
  echo "6. Release report json: \`$REPORT_JSON\`"
  echo
  echo "## Asset Digests"
  echo
  for i in "${!ASSET_PATHS[@]}"; do
    echo "$((i + 1)). \`${ASSET_NAMES[$i]}\`: path=\`${ASSET_PATHS[$i]}\`, bytes=\`${ASSET_BYTES[$i]}\`, sha256=\`${ASSET_SHAS[$i]}\`"
  done
} >"$MAP_OUT"

{
  printf '{\n'
  printf '  "head": "%s",\n' "$(git rev-parse --short HEAD)"
  printf '  "branch": "%s",\n' "$(git branch --show-current)"
  printf '  "release_spec": "%s",\n' "$RELEASE_SPEC"
  printf '  "tag": "%s",\n' "$TAG_NAME"
  printf '  "release_url": "%s",\n' "$RELEASE_URL"
  printf '  "release_note": "%s",\n' "$RELEASE_NOTE"
  printf '  "asset": {"name":"%s","path":"%s","bytes":%s,"sha256":"%s"},\n' "${ASSET_NAMES[0]}" "${ASSET_PATHS[0]}" "${ASSET_BYTES[0]}" "${ASSET_SHAS[0]}"
  printf '  "assets": [\n'
  for i in "${!ASSET_PATHS[@]}"; do
    printf '    {"name":"%s","path":"%s","bytes":%s,"sha256":"%s"}' "${ASSET_NAMES[$i]}" "${ASSET_PATHS[$i]}" "${ASSET_BYTES[$i]}" "${ASSET_SHAS[$i]}"
    if [[ $i -lt $((${#ASSET_PATHS[@]} - 1)) ]]; then
      printf ','
    fi
    printf '\n'
  done
  printf '  ],\n'
  printf '  "bundle_index": "%s",\n' "$BUNDLE_INDEX"
  printf '  "bundle_manifest": "%s",\n' "$BUNDLE_MANIFEST"
  printf '  "report_json": "%s",\n' "$REPORT_JSON"
  printf '  "map_md": "%s",\n' "$MAP_OUT"
  printf '  "map_json": "%s"\n' "$MAP_JSON_OUT"
  printf '}\n'
} >"$MAP_JSON_OUT"

echo "trace_id=release.publish_map.ok map=$MAP_OUT map_json=$MAP_JSON_OUT tag=$TAG_NAME release_url=$RELEASE_URL assets=${#ASSET_PATHS[@]}"
