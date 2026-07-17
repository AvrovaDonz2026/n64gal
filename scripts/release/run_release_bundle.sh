#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-$ROOT_DIR/build_release_bundle}"
RELEASE_SPEC="${RELEASE_SPEC:-$ROOT_DIR/docs/release-publish-v1.1.0.json}"
RELEASE_GATE_SUMMARY="${RELEASE_GATE_SUMMARY:-$ROOT_DIR/build_release_gate/release_gate_summary.md}"
DEMO_SOAK_SUMMARY="${DEMO_SOAK_SUMMARY:-$ROOT_DIR/build_release_gate/demo_soak_summary.md}"
CONTENT_SOAK_SUMMARY=""
CONTENT_SOAK_SUMMARY_JSON=""
CI_SUITE_SUMMARY="${CI_SUITE_SUMMARY:-$ROOT_DIR/build_ci_cc/ci_suite_summary.md}"
HOST_SDK_SUMMARY="${HOST_SDK_SUMMARY:-$ROOT_DIR/build_release_host_sdk/host_sdk_smoke_summary.md}"
HOST_SDK_SUMMARY_JSON="${HOST_SDK_SUMMARY_JSON:-$ROOT_DIR/build_release_host_sdk/host_sdk_smoke_summary.json}"
PLATFORM_EVIDENCE_SUMMARY="${PLATFORM_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_platform/platform_evidence_summary.md}"
PLATFORM_EVIDENCE_SUMMARY_JSON="${PLATFORM_EVIDENCE_SUMMARY_JSON:-$ROOT_DIR/build_release_platform/platform_evidence_summary.json}"
PREVIEW_EVIDENCE_SUMMARY="${PREVIEW_EVIDENCE_SUMMARY:-$ROOT_DIR/build_release_preview/preview_evidence_summary.md}"
PREVIEW_EVIDENCE_SUMMARY_JSON="${PREVIEW_EVIDENCE_SUMMARY_JSON:-$ROOT_DIR/build_release_preview/preview_evidence_summary.json}"
RELEASE_REPORT_MD=""
RELEASE_REPORT_JSON=""
RELEASE_PUBLISH_MAP_MD=""
RELEASE_PUBLISH_MAP_JSON=""
RELEASE_REMOTE_SUMMARY_MD=""
RELEASE_REMOTE_SUMMARY_JSON=""

usage() {
  cat >&2 <<'EOF'
usage: scripts/release/run_release_bundle.sh [--out-dir <dir>] [--release-spec <path>] [--release-gate-summary <path>|--gate-summary <path>] [--demo-soak-summary <path>|--soak-summary <path>] [--content-soak-summary <path> --content-soak-summary-json <path>] [--ci-suite-summary <path>|--ci-summary <path>] [--host-sdk-summary <path>] [--host-sdk-summary-json <path>] [--platform-evidence-summary <path>] [--platform-evidence-summary-json <path>] [--preview-evidence-summary <path>] [--preview-evidence-summary-json <path>] [--report-md <path>] [--report-json <path>] [--publish-map-md <path>] [--publish-map-json <path>] [--remote-summary-md <path>] [--remote-summary-json <path>]
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
    --release-gate-summary|--gate-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_GATE_SUMMARY="$1"
      shift
      ;;
    --demo-soak-summary|--soak-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      DEMO_SOAK_SUMMARY="$1"
      shift
      ;;
    --content-soak-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      CONTENT_SOAK_SUMMARY="$1"
      shift
      ;;
    --content-soak-summary-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      CONTENT_SOAK_SUMMARY_JSON="$1"
      shift
      ;;
    --ci-suite-summary|--ci-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      CI_SUITE_SUMMARY="$1"
      shift
      ;;
    --host-sdk-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      HOST_SDK_SUMMARY="$1"
      shift
      ;;
    --host-sdk-summary-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      HOST_SDK_SUMMARY_JSON="$1"
      shift
      ;;
    --platform-evidence-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PLATFORM_EVIDENCE_SUMMARY="$1"
      shift
      ;;
    --platform-evidence-summary-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PLATFORM_EVIDENCE_SUMMARY_JSON="$1"
      shift
      ;;
    --preview-evidence-summary)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PREVIEW_EVIDENCE_SUMMARY="$1"
      shift
      ;;
    --preview-evidence-summary-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      PREVIEW_EVIDENCE_SUMMARY_JSON="$1"
      shift
      ;;
    --report-md)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_REPORT_MD="$1"
      shift
      ;;
    --report-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_REPORT_JSON="$1"
      shift
      ;;
    --publish-map-md)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_PUBLISH_MAP_MD="$1"
      shift
      ;;
    --publish-map-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_PUBLISH_MAP_JSON="$1"
      shift
      ;;
    --remote-summary-md)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_REMOTE_SUMMARY_MD="$1"
      shift
      ;;
    --remote-summary-json)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RELEASE_REMOTE_SUMMARY_JSON="$1"
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

if [[ -n "$CONTENT_SOAK_SUMMARY" && -z "$CONTENT_SOAK_SUMMARY_JSON" ]] ||
   [[ -z "$CONTENT_SOAK_SUMMARY" && -n "$CONTENT_SOAK_SUMMARY_JSON" ]]; then
  echo "trace_id=release.bundle.content_soak.pair_required error_code=-1 error_name=VN_E_INVALID_ARG message=content soak markdown and JSON summaries must be provided together" >&2
  exit 2
fi

mkdir -p "$OUT_DIR"
DOC_DIR="$OUT_DIR/docs"
SUM_DIR="$OUT_DIR/summaries"
PREVIEW_EVIDENCE_DIR="$OUT_DIR/evidence/preview"
PREVIEW_EVIDENCE_FILES=()
mkdir -p "$DOC_DIR" "$SUM_DIR" "$PREVIEW_EVIDENCE_DIR"

eval "$(python3 tools/validate/release_spec.py --shell "$RELEASE_SPEC")"
mapfile -t SPEC_ASSET_PATHS < <(python3 tools/validate/release_spec.py --asset-paths "$RELEASE_SPEC")
mapfile -t SPEC_ASSET_NAMES < <(python3 tools/validate/release_spec.py --asset-names "$RELEASE_SPEC")
if [[ "$SPEC_VERSION" == "v1.1.0" && -z "$CONTENT_SOAK_SUMMARY" ]]; then
  echo "trace_id=release.bundle.content_soak.required error_code=-1 error_name=VN_E_INVALID_ARG message=v1.1.0 bundle requires real-content soak markdown and JSON summaries" >&2
  exit 2
fi
if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
  CONTENT_PACK_PATH="$(python3 tools/validate/release_spec.py "$RELEASE_SPEC" --asset-path-for-name content-demo.vnpak)"
  python3 tools/validate/validate_content_soak_summary.py \
    "$CONTENT_SOAK_SUMMARY_JSON" --summary-md "$CONTENT_SOAK_SUMMARY" --pack "$CONTENT_PACK_PATH"
fi

copy_required() {
  local src="$1"
  local dst="$2"
  if [[ ! -f "$src" ]]; then
    echo "trace_id=release.bundle.missing error_code=-2 error_name=VN_E_IO path=$src message=required bundle input missing" >&2
    exit 1
  fi
  cp "$src" "$dst"
}

copy_required "$SPEC_RELEASE_NOTE" "$DOC_DIR/$(basename "$SPEC_RELEASE_NOTE")"
copy_required "$SPEC_RELEASE_EVIDENCE" "$DOC_DIR/$(basename "$SPEC_RELEASE_EVIDENCE")"
copy_required "$SPEC_RELEASE_PACKAGE" "$DOC_DIR/$(basename "$SPEC_RELEASE_PACKAGE")"
copy_required "$SPEC_RELEASE_CHECKLIST" "$DOC_DIR/$(basename "$SPEC_RELEASE_CHECKLIST")"
copy_required "README.md" "$OUT_DIR/README.md"
copy_required "CHANGELOG.md" "$OUT_DIR/CHANGELOG.md"
for i in "${!SPEC_ASSET_PATHS[@]}"; do
  copy_required "${SPEC_ASSET_PATHS[$i]}" "$OUT_DIR/${SPEC_ASSET_NAMES[$i]}"
done
copy_required "$RELEASE_GATE_SUMMARY" "$SUM_DIR/release_gate_summary.md"
copy_required "$DEMO_SOAK_SUMMARY" "$SUM_DIR/demo_soak_summary.md"
if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
  copy_required "$CONTENT_SOAK_SUMMARY" "$SUM_DIR/content_soak_summary.md"
  copy_required "$CONTENT_SOAK_SUMMARY_JSON" "$SUM_DIR/content_soak_summary.json"
fi
copy_required "$CI_SUITE_SUMMARY" "$SUM_DIR/ci_suite_summary.md"
copy_required "$HOST_SDK_SUMMARY" "$SUM_DIR/host_sdk_smoke_summary.md"
copy_required "$HOST_SDK_SUMMARY_JSON" "$SUM_DIR/host_sdk_smoke_summary.json"
copy_required "$PLATFORM_EVIDENCE_SUMMARY" "$SUM_DIR/platform_evidence_summary.md"
copy_required "$PLATFORM_EVIDENCE_SUMMARY_JSON" "$SUM_DIR/platform_evidence_summary.json"
copy_required "$PREVIEW_EVIDENCE_SUMMARY" "$SUM_DIR/preview_evidence_summary.md"
copy_required "$PREVIEW_EVIDENCE_SUMMARY_JSON" "$SUM_DIR/preview_evidence_summary.json"
while IFS=$'\t' read -r artifact_kind artifact_path; do
  case "$artifact_kind" in
    request) artifact_name="preview_request.txt" ;;
    response) artifact_name="preview_response.json" ;;
    screenshot) artifact_name="content_preview.ppm" ;;
    *) continue ;;
  esac
  copy_required "$artifact_path" "$PREVIEW_EVIDENCE_DIR/$artifact_name"
  PREVIEW_EVIDENCE_FILES+=("evidence/preview/$artifact_name")
done < <(python3 - "$PREVIEW_EVIDENCE_SUMMARY_JSON" <<'PY'
import json
import sys
from pathlib import Path

payload = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
for key in ("request", "response", "screenshot"):
    value = payload.get(key)
    if isinstance(value, str) and value:
        print(f"{key}\t{value}")
PY
)
if [[ -n "$RELEASE_REPORT_MD" ]]; then
  copy_required "$RELEASE_REPORT_MD" "$SUM_DIR/release_report.md"
fi
if [[ -n "$RELEASE_REPORT_JSON" ]]; then
  copy_required "$RELEASE_REPORT_JSON" "$SUM_DIR/release_report.json"
fi
if [[ -n "$RELEASE_PUBLISH_MAP_MD" ]]; then
  copy_required "$RELEASE_PUBLISH_MAP_MD" "$SUM_DIR/release_publish_map.md"
fi
if [[ -n "$RELEASE_PUBLISH_MAP_JSON" ]]; then
  copy_required "$RELEASE_PUBLISH_MAP_JSON" "$SUM_DIR/release_publish_map.json"
fi
if [[ -n "$RELEASE_REMOTE_SUMMARY_MD" ]]; then
  copy_required "$RELEASE_REMOTE_SUMMARY_MD" "$SUM_DIR/release_remote_summary.md"
fi
if [[ -n "$RELEASE_REMOTE_SUMMARY_JSON" ]]; then
  copy_required "$RELEASE_REMOTE_SUMMARY_JSON" "$SUM_DIR/release_remote_summary.json"
fi

INDEX_MD="$OUT_DIR/release_bundle_index.md"
INDEX_JSON="$OUT_DIR/release_bundle_index.json"
MANIFEST_MD="$OUT_DIR/release_bundle_manifest.md"
MANIFEST_JSON="$OUT_DIR/release_bundle_manifest.json"
BUNDLE_FILES=(
  "docs/$(basename "$SPEC_RELEASE_NOTE")"
  "docs/$(basename "$SPEC_RELEASE_EVIDENCE")"
  "docs/$(basename "$SPEC_RELEASE_PACKAGE")"
  "docs/$(basename "$SPEC_RELEASE_CHECKLIST")"
  "README.md"
  "CHANGELOG.md"
  "summaries/release_gate_summary.md"
  "summaries/demo_soak_summary.md"
  "summaries/ci_suite_summary.md"
  "summaries/host_sdk_smoke_summary.md"
  "summaries/host_sdk_smoke_summary.json"
  "summaries/platform_evidence_summary.md"
  "summaries/platform_evidence_summary.json"
  "summaries/preview_evidence_summary.md"
  "summaries/preview_evidence_summary.json"
)
if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
  BUNDLE_FILES+=("summaries/content_soak_summary.md")
  BUNDLE_FILES+=("summaries/content_soak_summary.json")
fi
for asset_name in "${SPEC_ASSET_NAMES[@]}"; do
  BUNDLE_FILES+=("$asset_name")
done
if [[ ${#PREVIEW_EVIDENCE_FILES[@]} -gt 0 ]]; then
  BUNDLE_FILES+=("${PREVIEW_EVIDENCE_FILES[@]}")
fi
if [[ -n "$RELEASE_REPORT_MD" ]]; then
  BUNDLE_FILES+=("summaries/release_report.md")
fi
if [[ -n "$RELEASE_REPORT_JSON" ]]; then
  BUNDLE_FILES+=("summaries/release_report.json")
fi
if [[ -n "$RELEASE_PUBLISH_MAP_MD" ]]; then
  BUNDLE_FILES+=("summaries/release_publish_map.md")
fi
if [[ -n "$RELEASE_PUBLISH_MAP_JSON" ]]; then
  BUNDLE_FILES+=("summaries/release_publish_map.json")
fi
if [[ -n "$RELEASE_REMOTE_SUMMARY_MD" ]]; then
  BUNDLE_FILES+=("summaries/release_remote_summary.md")
fi
if [[ -n "$RELEASE_REMOTE_SUMMARY_JSON" ]]; then
  BUNDLE_FILES+=("summaries/release_remote_summary.json")
fi
INDEX_DOCS=(
  "docs/$(basename "$SPEC_RELEASE_NOTE")"
  "docs/$(basename "$SPEC_RELEASE_EVIDENCE")"
  "docs/$(basename "$SPEC_RELEASE_PACKAGE")"
  "docs/$(basename "$SPEC_RELEASE_CHECKLIST")"
  "README.md"
  "CHANGELOG.md"
)
INDEX_SUMMARIES=(
  "summaries/release_gate_summary.md"
  "summaries/demo_soak_summary.md"
  "summaries/ci_suite_summary.md"
  "summaries/host_sdk_smoke_summary.md"
  "summaries/host_sdk_smoke_summary.json"
  "summaries/platform_evidence_summary.md"
  "summaries/platform_evidence_summary.json"
  "summaries/preview_evidence_summary.md"
  "summaries/preview_evidence_summary.json"
)
if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
  INDEX_SUMMARIES+=("summaries/content_soak_summary.md" "summaries/content_soak_summary.json")
fi
if [[ -n "$RELEASE_REPORT_MD" ]]; then INDEX_SUMMARIES+=("summaries/release_report.md"); fi
if [[ -n "$RELEASE_REPORT_JSON" ]]; then INDEX_SUMMARIES+=("summaries/release_report.json"); fi
if [[ -n "$RELEASE_PUBLISH_MAP_MD" ]]; then INDEX_SUMMARIES+=("summaries/release_publish_map.md"); fi
if [[ -n "$RELEASE_PUBLISH_MAP_JSON" ]]; then INDEX_SUMMARIES+=("summaries/release_publish_map.json"); fi
if [[ -n "$RELEASE_REMOTE_SUMMARY_MD" ]]; then INDEX_SUMMARIES+=("summaries/release_remote_summary.md"); fi
if [[ -n "$RELEASE_REMOTE_SUMMARY_JSON" ]]; then INDEX_SUMMARIES+=("summaries/release_remote_summary.json"); fi
INDEX_EVIDENCE=("${PREVIEW_EVIDENCE_FILES[@]}")
INDEX_ASSETS=("${SPEC_ASSET_NAMES[@]}")
{
  echo "# Release Bundle"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Out dir: \`$OUT_DIR\`"
  echo
  echo "## Docs"
  echo
  echo "1. \`docs/$(basename "$SPEC_RELEASE_NOTE")\`"
  echo "2. \`docs/$(basename "$SPEC_RELEASE_EVIDENCE")\`"
  echo "3. \`docs/$(basename "$SPEC_RELEASE_PACKAGE")\`"
  echo "4. \`docs/$(basename "$SPEC_RELEASE_CHECKLIST")\`"
  echo "5. \`README.md\`"
  echo "6. \`CHANGELOG.md\`"
  echo
  echo "## Summaries"
  echo
  echo "1. \`summaries/release_gate_summary.md\`"
  echo "2. \`summaries/demo_soak_summary.md\`"
  echo "3. \`summaries/ci_suite_summary.md\`"
  echo "4. \`summaries/host_sdk_smoke_summary.md\`"
  echo "5. \`summaries/host_sdk_smoke_summary.json\`"
  echo "6. \`summaries/platform_evidence_summary.md\`"
  echo "7. \`summaries/platform_evidence_summary.json\`"
  echo "8. \`summaries/preview_evidence_summary.md\`"
  echo "9. \`summaries/preview_evidence_summary.json\`"
  if [[ -n "$CONTENT_SOAK_SUMMARY" ]]; then
    echo "10. \`summaries/content_soak_summary.md\`"
    echo "11. \`summaries/content_soak_summary.json\`"
  fi
  if [[ -n "$RELEASE_REPORT_MD" ]]; then
    echo "12. \`summaries/release_report.md\`"
  fi
  if [[ -n "$RELEASE_REPORT_JSON" ]]; then
    echo "13. \`summaries/release_report.json\`"
  fi
  if [[ -n "$RELEASE_PUBLISH_MAP_MD" ]]; then
    echo "14. \`summaries/release_publish_map.md\`"
  fi
  if [[ -n "$RELEASE_PUBLISH_MAP_JSON" ]]; then
    echo "15. \`summaries/release_publish_map.json\`"
  fi
  if [[ -n "$RELEASE_REMOTE_SUMMARY_MD" ]]; then
    echo "16. \`summaries/release_remote_summary.md\`"
  fi
  if [[ -n "$RELEASE_REMOTE_SUMMARY_JSON" ]]; then
    echo "17. \`summaries/release_remote_summary.json\`"
  fi
  if [[ ${#PREVIEW_EVIDENCE_FILES[@]} -gt 0 ]]; then
    echo
    echo "## Preview Evidence"
    echo
    for i in "${!PREVIEW_EVIDENCE_FILES[@]}"; do
      echo "$((i + 1)). \`${PREVIEW_EVIDENCE_FILES[$i]}\`"
    done
  fi
  echo
  echo "## Assets"
  echo
  for i in "${!SPEC_ASSET_NAMES[@]}"; do
    echo "$((i + 1)). \`${SPEC_ASSET_NAMES[$i]}\`"
  done
  echo
  echo "## Manifests"
  echo
  echo "1. \`release_bundle_manifest.md\`"
  echo "2. \`release_bundle_manifest.json\`"
} >"$INDEX_MD"

python3 - "$INDEX_JSON" "$(git rev-parse --short HEAD)" "$(git branch --show-current)" \
  "$OUT_DIR" "$INDEX_MD" "$MANIFEST_MD" "$MANIFEST_JSON" \
  "${#INDEX_DOCS[@]}" "${INDEX_DOCS[@]}" \
  "${#INDEX_SUMMARIES[@]}" "${INDEX_SUMMARIES[@]}" \
  "${#INDEX_EVIDENCE[@]}" "${INDEX_EVIDENCE[@]}" \
  "${#INDEX_ASSETS[@]}" "${INDEX_ASSETS[@]}" <<'PY'
import json
import sys
from pathlib import Path

values = sys.argv[1:]
output_path, head, branch, out_dir, index_md, manifest_md, manifest_json = values[:7]
cursor = 7

def take_group():
    global cursor
    count = int(values[cursor])
    cursor += 1
    result = values[cursor:cursor + count]
    cursor += count
    return result

payload = {
    "head": head,
    "branch": branch,
    "out_dir": out_dir,
    "index_md": index_md,
    "manifest_md": manifest_md,
    "manifest_json": manifest_json,
    "external_reference_ready": True,
    "docs": take_group(),
    "summaries": take_group(),
    "evidence": take_group(),
    "assets": take_group(),
}
Path(output_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY

{
  echo "# Release Bundle Manifest"
  echo
  echo "- Head: \`$(git rev-parse --short HEAD)\`"
  echo "- Branch: \`$(git branch --show-current)\`"
  echo "- Out dir: \`$OUT_DIR\`"
  echo
  echo "| Path | SHA256 | Bytes |"
  echo "|---|---|---:|"
  for rel in "${BUNDLE_FILES[@]}"; do
    abs="$OUT_DIR/$rel"
    sha="$(sha256sum "$abs" | awk '{print $1}')"
    size="$(wc -c <"$abs" | tr -d '[:space:]')"
    echo "| \`$rel\` | \`$sha\` | $size |"
  done
} >"$MANIFEST_MD"

python3 - "$MANIFEST_JSON" "$(git rev-parse --short HEAD)" "$(git branch --show-current)" \
  "$OUT_DIR" "${BUNDLE_FILES[@]}" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

output_path, head, branch, out_dir = sys.argv[1:5]
root = Path(out_dir)
files = []
for relative_path in sys.argv[5:]:
    data = (root / relative_path).read_bytes()
    files.append(
        {
            "path": relative_path,
            "sha256": hashlib.sha256(data).hexdigest(),
            "bytes": len(data),
        }
    )
payload = {"head": head, "branch": branch, "out_dir": out_dir, "files": files}
Path(output_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY

echo "trace_id=release.bundle.ok out_dir=$OUT_DIR index=$INDEX_MD index_json=$INDEX_JSON manifest=$MANIFEST_MD manifest_json=$MANIFEST_JSON"
