#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

diff_source="working tree"

list_changed_files() {
  local changed=""
  local untracked=""

  if [[ -n "${API_DOC_SYNC_RANGE:-}" ]]; then
    diff_source="${API_DOC_SYNC_RANGE}"
    changed="$(git diff --name-only --relative "$API_DOC_SYNC_RANGE" -- || true)"
  elif git rev-parse --verify HEAD >/dev/null 2>&1; then
    if git diff --quiet HEAD -- && git rev-parse --verify HEAD~1 >/dev/null 2>&1; then
      diff_source="HEAD~1..HEAD"
      changed="$(git diff --name-only --relative HEAD~1..HEAD -- || true)"
    else
      diff_source="HEAD..working tree"
      changed="$(git diff --name-only --relative HEAD -- || true)"
    fi
  else
    changed="$(git diff --name-only --relative -- || true)"
  fi

  untracked="$(git ls-files --others --exclude-standard || true)"
  if [[ -n "$changed" && -n "$untracked" ]]; then
    printf "%s\n%s\n" "$changed" "$untracked"
  elif [[ -n "$changed" ]]; then
    printf "%s\n" "$changed"
  else
    printf "%s\n" "$untracked"
  fi
}

CHANGED_FILES="$(list_changed_files | awk 'NF > 0' | sort -u)"

has_changed() {
  local path="$1"
  if [[ -z "$CHANGED_FILES" ]]; then
    return 1
  fi
  printf "%s\n" "$CHANGED_FILES" | grep -Fx "$path" >/dev/null 2>&1
}

surface_changed=0
failed=0

require_docs() {
  local surface="$1"
  shift
  local missing=()
  local path

  surface_changed=1
  for path in "$@"; do
    if ! has_changed "$path"; then
      missing+=("$path")
    fi
  done

  if (( ${#missing[@]} > 0 )); then
    failed=1
    printf "[api-doc-sync] missing docs for %s:" "$surface" >&2
    for path in "${missing[@]}"; do
      printf " %s" "$path" >&2
    done
    printf "\n" >&2
  else
    printf "[api-doc-sync] %s docs updated\n" "$surface"
  fi
}

printf "[api-doc-sync] diff source: %s\n" "$diff_source"

if has_changed "include/vn_runtime.h" || has_changed "src/core/runtime_cli.c" || has_changed "src/core/runtime_input.c" || has_changed "src/core/runtime_parse.c" || has_changed "src/core/runtime_persist.c"; then
  require_docs "runtime" "docs/api/runtime.md"
fi

if has_changed "include/vn_preview.h" || has_changed "src/tools/preview_parse.c" || has_changed "src/tools/preview_cli.c" || has_changed "src/tools/preview_report.c"; then
  require_docs "preview" "docs/preview-protocol.md" "docs/errors.md"
fi

if has_changed "include/vn_backend.h"; then
  require_docs "backend" "docs/api/backend.md"
fi

if has_changed "include/vn_pack.h" || has_changed "src/core/pack.c"; then
  require_docs "pack" "docs/api/pack.md"
fi

if has_changed "include/vn_save.h" || has_changed "src/core/save.c" || has_changed "tools/migrate/vnsave_migrate.c"; then
  require_docs "save" "docs/api/save.md" "docs/migration.md" "docs/vnsave-version-policy.md"
fi

if has_changed "include/vn_error.h" || has_changed "src/core/error.c"; then
  require_docs "errors" "docs/errors.md"
fi

if (( surface_changed != 0 )); then
  if ! has_changed "docs/api/compat-log.md"; then
    failed=1
    printf "[api-doc-sync] missing docs/api/compat-log.md for public surface change\n" >&2
  else
    printf "[api-doc-sync] compat log updated\n"
  fi
else
  printf "[api-doc-sync] no public surface changes detected\n"
fi

if (( failed != 0 )); then
  exit 1
fi

printf "[api-doc-sync] OK\n"
