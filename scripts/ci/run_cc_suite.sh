#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build_ci_cc}"
LOG_DIR="$BUILD_DIR/ci_logs"
GOLDEN_ARTIFACT_DIR="$BUILD_DIR/golden_artifacts"
SUMMARY_MD="$BUILD_DIR/ci_suite_summary.md"
mkdir -p "$BUILD_DIR" "$LOG_DIR" "$GOLDEN_ARTIFACT_DIR"

run_capture() {
  local log_path
  log_path="$1"
  shift
  "$@" >"$log_path" 2>&1
  cat "$log_path"
}

dirty_submit_matched_backends() {
  local log_path
  local matches

  log_path="$1"
  if [[ ! -f "$log_path" ]]; then
    echo "none"
    return 0
  fi

  matches="$({ grep "test_renderer_dirty_submit matched backend=" "$log_path" || true; } | sed -E 's/.*backend=([^ ]+).*/\1/' | paste -sd ',' -)"
  if [[ -z "$matches" ]]; then
    echo "none"
  else
    echo "$matches"
  fi
}

write_summary() {
  local status
  local dirty_log
  local dirty_matches

  status="$1"
  dirty_log="$LOG_DIR/test_renderer_dirty_submit.log"
  dirty_matches="$(dirty_submit_matched_backends "$dirty_log")"
  {
    echo "# CI Suite Summary"
    echo
    echo "- Status: \`$status\`"
    echo "- Build dir: \`$BUILD_DIR\`"
    echo "- Log dir: \`$LOG_DIR\`"
    echo "- Golden artifact dir: \`$GOLDEN_ARTIFACT_DIR\`"
    echo "- Fallback log: \`$LOG_DIR/test_renderer_fallback.log\`"
    echo "- Dirty submit log: \`$dirty_log\`"
    echo "- Dirty submit matched backends: \`$dirty_matches\`"
    echo "- Golden runtime log: \`$LOG_DIR/test_runtime_golden.log\`"
    if compgen -G "$GOLDEN_ARTIFACT_DIR/*" >/dev/null; then
      echo "- Golden artifacts present: yes"
    else
      echo "- Golden artifacts present: no (exact-match run or no diff output)"
    fi
  } > "$SUMMARY_MD"
}

trap 'rc=$?; if [[ $rc -eq 0 ]]; then write_summary success; else write_summary failed; fi; exit $rc' EXIT

run_capture "$LOG_DIR/check_c89.log" ./scripts/check_c89.sh
run_capture "$LOG_DIR/build_demo_scripts.log" ./tools/scriptc/build_demo_scripts.sh
run_capture "$LOG_DIR/make_demo_pack.log" ./tools/packer/make_demo_pack.sh

COMMON_SRC=(
  src/core/backend_registry.c
  src/core/renderer.c
  src/core/vm.c
  src/core/pack.c
  src/core/platform.c
  src/core/runtime_cli.c
  src/core/dynamic_resolution.c
  src/frontend/render_ops.c
  src/frontend/dirty_tiles.c
  src/backend/common/pixel_pipeline.c
  src/backend/avx2/avx2_backend.c
  src/backend/avx2/avx2_textured.c
  src/backend/neon/neon_backend.c
  src/backend/rvv/rvv_backend.c
  src/backend/scalar/scalar_backend.c
)
PREVIEW_SRC=(
  src/tools/preview_cli.c
)
CFLAGS=(
  -std=c89
  -Wall
  -Wextra
  -Werror
  -pedantic-errors
  -Iinclude
)
TESTS=(
  test_backend_registry
  test_render_ops
  test_dirty_tiles
  test_dynamic_resolution
  test_vnpak
  test_renderer_fallback
  test_renderer_dirty_submit
  test_backend_consistency
  test_backend_priority
  test_avx2_fastpath_parity
  test_vm
  test_runtime_api
  test_runtime_dynamic_resolution
  test_runtime_session
  test_runtime_input
  test_runtime_golden
  test_platform_paths
)

for test_name in "${TESTS[@]}"; do
  cc "${CFLAGS[@]}" "tests/unit/${test_name}.c" "${COMMON_SRC[@]}" -o "$BUILD_DIR/$test_name"
done

cc "${CFLAGS[@]}" tests/integration/test_preview_protocol.c "${PREVIEW_SRC[@]}" "${COMMON_SRC[@]}" -o "$BUILD_DIR/test_preview_protocol"
cc "${CFLAGS[@]}" src/main.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/vn_player"
cc "${CFLAGS[@]}" src/tools/previewd_main.c "${PREVIEW_SRC[@]}" "${COMMON_SRC[@]}" -o "$BUILD_DIR/vn_previewd"
cc "${CFLAGS[@]}" tests/perf/backend_kernel_bench.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/vn_backend_kernel_bench"
cc "${CFLAGS[@]}" examples/host-embed/session_loop.c "${COMMON_SRC[@]}" -o "$BUILD_DIR/example_host_embed"

for test_name in "${TESTS[@]}"; do
  if [[ "$test_name" == "test_runtime_golden" ]]; then
    run_capture "$LOG_DIR/${test_name}.log" env VN_GOLDEN_ARTIFACT_DIR="$GOLDEN_ARTIFACT_DIR" "$BUILD_DIR/$test_name"
  else
    run_capture "$LOG_DIR/${test_name}.log" "$BUILD_DIR/$test_name"
  fi
done

run_capture "$LOG_DIR/test_preview_protocol.log" "$BUILD_DIR/test_preview_protocol"
run_capture "$LOG_DIR/test_backend_kernel_bench.log" "$BUILD_DIR/vn_backend_kernel_bench" --backend scalar --iterations 4 --warmup 1
run_capture "$LOG_DIR/example_host_embed.log" "$BUILD_DIR/example_host_embed"
