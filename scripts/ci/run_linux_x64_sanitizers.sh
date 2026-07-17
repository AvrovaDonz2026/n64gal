#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

SCENE_DURATION_SEC=450
FRAMES_PER_SCENE=""
FRAMES_OVERRIDE=0
DT_MS=16
RESOLUTION="600x800"
MAX_RSS_MIB=64
SKIP_BUILD=0
SKIP_PACK=0
HOST_OS="$(uname -s)"
HOST_ARCH="$(uname -m)"

usage() {
  cat >&2 <<'EOF'
usage: scripts/ci/run_linux_x64_sanitizers.sh [options]

options:
  --scene-duration-sec <sec>   simulated seconds per scene (default: 450)
  --frames-per-scene <frames> override the simulated duration (for tests)
  --dt-ms <ms>                 simulated milliseconds per frame (default: 16)
  --resolution <WxH>           soak resolution (default: 600x800)
  --max-rss-mib <MiB>          per-process peak RSS limit (default: 64)
  --summary-out <path>         markdown summary path
  --summary-json-out <path>    JSON summary path
  --skip-build                 reuse binaries in BUILD_DIR
  --skip-pack                  reuse assets/demo/content-demo.vnpak
EOF
}

SUMMARY_OUT=""
SUMMARY_JSON_OUT=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --scene-duration-sec)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      SCENE_DURATION_SEC="$1"
      shift
      ;;
    --frames-per-scene)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      FRAMES_PER_SCENE="$1"
      FRAMES_OVERRIDE=1
      shift
      ;;
    --dt-ms)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      DT_MS="$1"
      shift
      ;;
    --resolution)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      RESOLUTION="$1"
      shift
      ;;
    --max-rss-mib)
      shift
      [[ $# -gt 0 ]] || { usage; exit 2; }
      MAX_RSS_MIB="$1"
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
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --skip-pack)
      SKIP_PACK=1
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

is_positive_integer() {
  [[ "$1" =~ ^[1-9][0-9]*$ ]]
}

if ! is_positive_integer "$SCENE_DURATION_SEC" ||
   ! is_positive_integer "$DT_MS" ||
   ! is_positive_integer "$MAX_RSS_MIB"; then
  echo "scene duration, dt, and RSS limit must be positive integers" >&2
  exit 2
fi
if [[ -n "$FRAMES_PER_SCENE" ]] && ! is_positive_integer "$FRAMES_PER_SCENE"; then
  echo "frames per scene must be a positive integer" >&2
  exit 2
fi
if [[ ! "$RESOLUTION" =~ ^[1-9][0-9]*x[1-9][0-9]*$ ]]; then
  echo "resolution must use WxH with positive integers" >&2
  exit 2
fi
if [[ "$HOST_OS" != "Linux" || "$HOST_ARCH" != "x86_64" ]]; then
  echo "linux-x64 sanitizer suite requires Linux x86_64, got $HOST_OS $HOST_ARCH" >&2
  exit 2
fi

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_ci_sanitizers}"
LOG_DIR="$BUILD_DIR/logs"
OBJ_DIR="$BUILD_DIR/obj"
TMP_BUILD_DIR="$BUILD_DIR/tmp"
RESULTS_TSV="$BUILD_DIR/results.tsv"
if [[ -z "$SUMMARY_OUT" ]]; then
  SUMMARY_OUT="$BUILD_DIR/sanitizer_summary.md"
fi
if [[ -z "$SUMMARY_JSON_OUT" ]]; then
  SUMMARY_JSON_OUT="$BUILD_DIR/sanitizer_summary.json"
fi
mkdir -p "$BUILD_DIR" "$LOG_DIR" "$OBJ_DIR" "$TMP_BUILD_DIR"
mkdir -p "$(dirname "$SUMMARY_OUT")" "$(dirname "$SUMMARY_JSON_OUT")"
: > "$RESULTS_TSV"
export TMPDIR="$TMP_BUILD_DIR"

if [[ -z "$FRAMES_PER_SCENE" ]]; then
  FRAMES_PER_SCENE=$(( (SCENE_DURATION_SEC * 1000 + DT_MS - 1) / DT_MS ))
fi
if [[ "$FRAMES_PER_SCENE" -gt 1000000 ]]; then
  echo "frames per scene exceeds vn_player limit (1000000)" >&2
  exit 2
fi

CC_BIN="${CC:-cc}"
MAX_RSS_KIB_LIMIT=$(( MAX_RSS_MIB * 1024 ))
MAX_OBSERVED_RSS_KIB=0
STATUS="failed"
FAILURE_REASON="validation did not complete"
ASAN_OPTIONS_VALUE="detect_leaks=0:halt_on_error=1:abort_on_error=1:strict_string_checks=1:quarantine_size_mb=8:thread_local_quarantine_size_kb=256"
UBSAN_OPTIONS_VALUE="halt_on_error=1:print_stacktrace=1"

CFLAGS=(
  -std=c89
  -Wall
  -Wextra
  -Werror
  -pedantic-errors
  -O1
  -g
  -fno-omit-frame-pointer
  -fno-sanitize-recover=all
  -fsanitize=address,undefined
  -Iinclude
)
LDFLAGS=(
  -fno-omit-frame-pointer
  -fno-sanitize-recover=all
  -fsanitize=address,undefined
)
COMMON_SRC=(
  src/core/error.c
  src/core/backend_registry.c
  src/core/renderer.c
  src/core/save.c
  src/core/vm.c
  src/core/pack.c
  src/core/platform.c
  src/core/runtime_cli.c
  src/core/runtime_input.c
  src/core/runtime_parse.c
  src/core/runtime_persist.c
  src/core/runtime_session_support.c
  src/core/runtime_session_loop.c
  src/core/scene_catalog.c
  src/core/runtime_texture.c
  src/core/dynamic_resolution.c
  src/frontend/render_ops.c
  src/frontend/dirty_tiles.c
  src/backend/common/pixel_pipeline.c
  src/backend/avx2/avx2_backend.c
  src/backend/avx2/avx2_fill_fade.c
  src/backend/avx2/avx2_textured.c
  src/backend/neon/neon_backend.c
  src/backend/rvv/rvv_backend.c
  src/backend/scalar/scalar_backend.c
)
TESTS=(
  test_vm
  test_scene_catalog
  test_resource_texture_backend
  test_runtime_session
  test_runtime_cli_errors
)
SCENES=(Opening Gallery)
SIMULATED_MS_PER_SCENE=$(( FRAMES_PER_SCENE * DT_MS ))
TOTAL_SIMULATED_MS=$(( SIMULATED_MS_PER_SCENE * ${#SCENES[@]} ))

rss_mib() {
  awk -v kib="$1" 'BEGIN { printf "%.2f", kib / 1024.0 }'
}

write_summary() {
  local head_value
  local branch_value
  local record_count
  local source_state

  head_value="$(git rev-parse --short HEAD 2>/dev/null || printf unknown)"
  branch_value="$(git branch --show-current 2>/dev/null || true)"
  if [[ -z "$branch_value" ]]; then
    branch_value="detached"
  fi
  if [[ -n "$(git status --porcelain 2>/dev/null)" ]]; then
    source_state="dirty"
  else
    source_state="clean"
  fi
  record_count="$(wc -l < "$RESULTS_TSV" | tr -d '[:space:]')"
  {
    echo "# Linux x64 Sanitizer Summary"
    echo
    echo "- Status: \`$STATUS\`"
    echo "- Failure reason: \`$FAILURE_REASON\`"
    echo "- Head: \`$head_value\`"
    echo "- Branch: \`$branch_value\`"
    echo "- Source state: \`$source_state\`"
    echo "- Compiler: \`$CC_BIN\`"
    echo "- Host: \`$HOST_OS/$HOST_ARCH\`"
    echo "- Language gate: \`C89 -Wall -Wextra -Werror -pedantic-errors\`"
    echo "- Sanitizers: \`AddressSanitizer, UndefinedBehaviorSanitizer\`"
    echo "- ASan options: \`$ASAN_OPTIONS_VALUE\`"
    echo "- Pack: \`assets/demo/content-demo.vnpak\`"
    echo "- Scenes: \`Opening,Gallery\`"
    echo "- Resolution: \`$RESOLUTION\`"
    echo "- DT ms: \`$DT_MS\`"
    echo "- Requested scene duration seconds: \`$SCENE_DURATION_SEC\`"
    echo "- Frames override: \`$FRAMES_OVERRIDE\`"
    echo "- Frames per scene: \`$FRAMES_PER_SCENE\`"
    echo "- Actual simulated ms per scene: \`$SIMULATED_MS_PER_SCENE\`"
    echo "- Actual total simulated ms: \`$TOTAL_SIMULATED_MS\`"
    echo "- Peak RSS limit: \`$MAX_RSS_MIB MiB ($MAX_RSS_KIB_LIMIT KiB)\`"
    echo "- Maximum observed peak RSS: \`$(rss_mib "$MAX_OBSERVED_RSS_KIB") MiB ($MAX_OBSERVED_RSS_KIB KiB)\`"
    echo "- RSS source: \`getrusage(RUSAGE_CHILDREN).ru_maxrss\` on Linux"
    echo "- Build dir: \`$BUILD_DIR\`"
    echo "- Log dir: \`$LOG_DIR\`"
    echo
    echo "## Measured Processes"
    echo
    if [[ "$record_count" -eq 0 ]]; then
      echo "No measured process completed."
    else
      echo "| Kind | Name | Status | Peak RSS | Log |"
      echo "| --- | --- | --- | ---: | --- |"
      while IFS=$'\t' read -r kind name record_status rss_kib log_path; do
        echo "| $kind | \`$name\` | \`$record_status\` | $(rss_mib "$rss_kib") MiB ($rss_kib KiB) | \`$log_path\` |"
      done < "$RESULTS_TSV"
    fi
  } > "$SUMMARY_OUT"

  python3 - "$RESULTS_TSV" "$SUMMARY_JSON_OUT" "$STATUS" "$FAILURE_REASON" \
    "$head_value" "$branch_value" "$source_state" "$CC_BIN" "$HOST_OS" "$HOST_ARCH" "$ASAN_OPTIONS_VALUE" "$SCENE_DURATION_SEC" \
    "$FRAMES_OVERRIDE" "$FRAMES_PER_SCENE" "$DT_MS" "$SIMULATED_MS_PER_SCENE" "$TOTAL_SIMULATED_MS" "$RESOLUTION" "$MAX_RSS_MIB" \
    "$MAX_RSS_KIB_LIMIT" "$MAX_OBSERVED_RSS_KIB" "$SUMMARY_OUT" <<'PY'
import json
import sys
from pathlib import Path

(
    records_path,
    output_path,
    status,
    failure_reason,
    head,
    branch,
    source_state,
    compiler,
    host_os,
    host_arch,
    asan_options,
    requested_scene_duration_sec,
    frames_override,
    frames_per_scene,
    dt_ms,
    simulated_ms_per_scene,
    total_simulated_ms,
    resolution,
    max_rss_mib,
    max_rss_kib,
    max_observed_rss_kib,
    summary_md,
) = sys.argv[1:]

records = []
for line in Path(records_path).read_text(encoding="utf-8").splitlines():
    kind, name, record_status, rss_kib, log_path = line.split("\t")
    records.append(
        {
            "kind": kind,
            "name": name,
            "status": record_status,
            "peak_rss_kib": int(rss_kib),
            "peak_rss_mib": round(int(rss_kib) / 1024.0, 2),
            "log": log_path,
        }
    )

payload = {
    "schema_version": 1,
    "status": status,
    "failure_reason": failure_reason,
    "head": head,
    "branch": branch,
    "source_state": source_state,
    "compiler": compiler,
    "host_os": host_os,
    "host_arch": host_arch,
    "strict_c89": True,
    "sanitizers": ["address", "undefined"],
    "asan_options": asan_options,
    "pack": "assets/demo/content-demo.vnpak",
    "scenes": ["Opening", "Gallery"],
    "requested_scene_duration_sec": int(requested_scene_duration_sec),
    "frames_override": bool(int(frames_override)),
    "frames_per_scene": int(frames_per_scene),
    "dt_ms": int(dt_ms),
    "simulated_ms_per_scene": int(simulated_ms_per_scene),
    "total_simulated_ms": int(total_simulated_ms),
    "resolution": resolution,
    "max_rss_mib_limit": int(max_rss_mib),
    "max_rss_kib_limit": int(max_rss_kib),
    "max_observed_rss_kib": int(max_observed_rss_kib),
    "rss_source": "getrusage(RUSAGE_CHILDREN).ru_maxrss on Linux",
    "summary_md": summary_md,
    "records": records,
}
Path(output_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY
}

on_exit() {
  local rc=$?
  set +e
  if [[ $rc -eq 0 ]]; then
    STATUS="success"
    FAILURE_REASON="none"
  fi
  write_summary
  if [[ $rc -eq 0 ]]; then
    echo "trace_id=ci.sanitizers.ok summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT max_rss_kib=$MAX_OBSERVED_RSS_KIB"
  else
    echo "trace_id=ci.sanitizers.failed summary=$SUMMARY_OUT summary_json=$SUMMARY_JSON_OUT reason=$FAILURE_REASON" >&2
  fi
  exit "$rc"
}
trap on_exit EXIT

record_result() {
  local kind="$1"
  local name="$2"
  local record_status="$3"
  local rss_kib="$4"
  local log_path="$5"

  printf '%s\t%s\t%s\t%s\t%s\n' "$kind" "$name" "$record_status" "$rss_kib" "$log_path" >> "$RESULTS_TSV"
  if [[ "$rss_kib" -gt "$MAX_OBSERVED_RSS_KIB" ]]; then
    MAX_OBSERVED_RSS_KIB="$rss_kib"
  fi
}

run_measured() {
  local kind="$1"
  local name="$2"
  local log_path="$3"
  local rss_path="$4"
  local rc
  local rss_kib

  shift 4
  : > "$log_path"
  rm -f "$rss_path"
  echo "[sanitizers] kind=$kind name=$name"
  set +e
  ASAN_OPTIONS="$ASAN_OPTIONS_VALUE" UBSAN_OPTIONS="$UBSAN_OPTIONS_VALUE" \
    python3 - "$rss_path" "$@" >"$log_path" 2>&1 <<'PY'
import resource
import subprocess
import sys
from pathlib import Path

rss_path = Path(sys.argv[1])
completed = subprocess.run(sys.argv[2:], check=False)
rss_kib = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
rss_path.write_text(f"{rss_kib}\n", encoding="ascii")
if completed.returncode < 0:
    raise SystemExit(128 - completed.returncode)
raise SystemExit(completed.returncode)
PY
  rc=$?
  set -e
  cat "$log_path"

  if [[ ! -f "$rss_path" ]]; then
    FAILURE_REASON="$name did not produce a peak RSS measurement"
    record_result "$kind" "$name" "rss-missing" 0 "$log_path"
    return 1
  fi
  rss_kib="$(tr -d '[:space:]' < "$rss_path")"
  if [[ ! "$rss_kib" =~ ^[0-9]+$ ]]; then
    FAILURE_REASON="$name produced an invalid peak RSS measurement"
    record_result "$kind" "$name" "rss-invalid" 0 "$log_path"
    return 1
  fi
  if [[ $rc -ne 0 ]]; then
    FAILURE_REASON="$name exited with status $rc"
    record_result "$kind" "$name" "failed" "$rss_kib" "$log_path"
    return 1
  fi
  if [[ "$rss_kib" -gt "$MAX_RSS_KIB_LIMIT" ]]; then
    FAILURE_REASON="$name peak RSS ${rss_kib} KiB exceeds ${MAX_RSS_KIB_LIMIT} KiB"
    record_result "$kind" "$name" "rss-limit" "$rss_kib" "$log_path"
    return 1
  fi
  record_result "$kind" "$name" "success" "$rss_kib" "$log_path"
  echo "[sanitizers] name=$name peak_rss_kib=$rss_kib limit_kib=$MAX_RSS_KIB_LIMIT"
}

run_build_step() {
  local name="$1"
  local log_path="$LOG_DIR/build_${name}.log"
  shift
  echo "[sanitizers] build=$name"
  if ! "$@" >"$log_path" 2>&1; then
    cat "$log_path"
    FAILURE_REASON="build step $name failed"
    return 1
  fi
}

if [[ $SKIP_PACK -eq 0 ]]; then
  run_build_step demo_scripts ./tools/scriptc/build_demo_scripts.sh
  run_build_step demo_pack ./tools/packer/make_demo_pack.sh
fi
if [[ ! -f assets/demo/content-demo.vnpak ]]; then
  FAILURE_REASON="assets/demo/content-demo.vnpak is missing"
  exit 1
fi

if [[ $SKIP_BUILD -eq 0 ]]; then
  COMMON_OBJECTS=()
  for source_path in "${COMMON_SRC[@]}"; do
    object_name="${source_path//\//_}"
    object_path="$OBJ_DIR/${object_name%.c}.o"
    run_build_step "${object_name%.c}" "$CC_BIN" "${CFLAGS[@]}" -c "$source_path" -o "$object_path"
    COMMON_OBJECTS+=("$object_path")
  done
  for test_name in "${TESTS[@]}"; do
    run_build_step "$test_name" "$CC_BIN" "${CFLAGS[@]}" "tests/unit/${test_name}.c" \
      "${COMMON_OBJECTS[@]}" "${LDFLAGS[@]}" -o "$BUILD_DIR/$test_name"
  done
  run_build_step vn_player "$CC_BIN" "${CFLAGS[@]}" src/main.c \
    "${COMMON_OBJECTS[@]}" "${LDFLAGS[@]}" -o "$BUILD_DIR/vn_player"
fi

for test_name in "${TESTS[@]}"; do
  if [[ ! -x "$BUILD_DIR/$test_name" ]]; then
    FAILURE_REASON="missing sanitizer test binary $BUILD_DIR/$test_name"
    exit 1
  fi
done
if [[ ! -x "$BUILD_DIR/vn_player" ]]; then
  FAILURE_REASON="missing sanitizer player binary $BUILD_DIR/vn_player"
  exit 1
fi

for test_name in "${TESTS[@]}"; do
  run_measured unit "$test_name" "$LOG_DIR/${test_name}.log" "$TMP_BUILD_DIR/${test_name}.rss" \
    "$BUILD_DIR/$test_name"
done

for scene in "${SCENES[@]}"; do
  scene_slug="$(printf '%s' "$scene" | tr '[:upper:]' '[:lower:]')"
  scene_log="$LOG_DIR/soak_${scene_slug}.log"
  run_measured soak "$scene" "$scene_log" "$TMP_BUILD_DIR/soak_${scene_slug}.rss" \
    "$BUILD_DIR/vn_player" \
      --pack assets/demo/content-demo.vnpak \
      --scene "$scene" \
      --resolution "$RESOLUTION" \
      --backend scalar \
      --frames "$FRAMES_PER_SCENE" \
      --dt-ms "$DT_MS" \
      --hold-end
  if ! grep -q 'trace_id=runtime.run.ok' "$scene_log"; then
    FAILURE_REASON="$scene soak log is missing the runtime success summary"
    exit 1
  fi
done

STATUS="success"
FAILURE_REASON="none"
