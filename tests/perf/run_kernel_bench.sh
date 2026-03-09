#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

source "$ROOT_DIR/tests/perf/host_cpu.sh"

SOURCE_ROOT="$ROOT_DIR"
BACKEND="scalar"
RESOLUTION="600x800"
ITERATIONS=128
WARMUP=16
OUT_CSV=""
RUNNER_BIN=""
SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --source-root)
      SOURCE_ROOT="$2"
      shift 2
      ;;
    --backend)
      BACKEND="$2"
      shift 2
      ;;
    --resolution)
      RESOLUTION="$2"
      shift 2
      ;;
    --iterations)
      ITERATIONS="$2"
      shift 2
      ;;
    --warmup)
      WARMUP="$2"
      shift 2
      ;;
    --out-csv)
      OUT_CSV="$2"
      shift 2
      ;;
    --runner-bin)
      RUNNER_BIN="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift 1
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ "$SOURCE_ROOT" != /* ]]; then
  SOURCE_ROOT="$ROOT_DIR/$SOURCE_ROOT"
fi
if [[ ! -d "$SOURCE_ROOT" ]]; then
  echo "source-root not found: $SOURCE_ROOT" >&2
  exit 2
fi

if [[ -z "$OUT_CSV" ]]; then
  OUT_CSV="$ROOT_DIR/tests/perf/kernel_bench_${BACKEND}.csv"
elif [[ "$OUT_CSV" != /* ]]; then
  OUT_CSV="$ROOT_DIR/$OUT_CSV"
fi
mkdir -p "$(dirname "$OUT_CSV")"

HOST_CPU="$(vn_perf_detect_host_cpu)"
printf "%s\n" "$HOST_CPU" > "$(dirname "$OUT_CSV")/kernel_host_cpu.txt"

if [[ -z "$RUNNER_BIN" ]]; then
  RUNNER_BIN="$ROOT_DIR/build_ci_cc/vn_backend_kernel_bench"
fi
if [[ "$RUNNER_BIN" != /* ]]; then
  RUNNER_BIN="$ROOT_DIR/$RUNNER_BIN"
fi

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  COMMON_SRC=(
    src/core/backend_registry.c
    src/core/renderer.c
    src/core/platform.c
    src/backend/common/pixel_pipeline.c
    src/backend/avx2/avx2_backend.c
    src/backend/avx2/avx2_textured.c
    src/backend/neon/neon_backend.c
    src/backend/rvv/rvv_backend.c
    src/backend/scalar/scalar_backend.c
  )
  CFLAGS=(
    -std=c89
    -Wall
    -Wextra
    -Werror
    -pedantic-errors
    -Iinclude
  )
  COMPILE_CMD=(
    "${CC:-cc}"
    "${CFLAGS[@]}"
    "$SOURCE_ROOT/tests/perf/backend_kernel_bench.c"
  )
  for src in "${COMMON_SRC[@]}"; do
    COMPILE_CMD+=("$SOURCE_ROOT/$src")
  done
  COMPILE_CMD+=(-o "$RUNNER_BIN")
  if [[ -n "${VN_PERF_CFLAGS:-}" ]]; then
    # shellcheck disable=SC2206
    EXTRA_CFLAGS=( $VN_PERF_CFLAGS )
    COMPILE_CMD=("${COMPILE_CMD[0]}" "${EXTRA_CFLAGS[@]}" "${COMPILE_CMD[@]:1}")
  fi
  if [[ -n "${VN_PERF_LDFLAGS:-}" ]]; then
    # shellcheck disable=SC2206
    EXTRA_LDFLAGS=( $VN_PERF_LDFLAGS )
    COMPILE_CMD+=("${EXTRA_LDFLAGS[@]}")
  fi
  "${COMPILE_CMD[@]}"
else
  if [[ ! -f "$RUNNER_BIN" ]]; then
    echo "runner-bin not found: $RUNNER_BIN" >&2
    exit 2
  fi
fi

"$RUNNER_BIN" \
  --backend "$BACKEND" \
  --resolution "$RESOLUTION" \
  --iterations "$ITERATIONS" \
  --warmup "$WARMUP" \
  --csv "$OUT_CSV"

TMP_CSV="$(mktemp)"
awk -F, -v host_cpu="$HOST_CPU" 'NR == 1 { print $0 ",host_cpu"; next } { print $0 "," host_cpu }' "$OUT_CSV" > "$TMP_CSV"
mv "$TMP_CSV" "$OUT_CSV"

echo "[kernel-bench] wrote $OUT_CSV backend=$BACKEND resolution=$RESOLUTION iterations=$ITERATIONS warmup=$WARMUP host_cpu=$HOST_CPU"
echo "[kernel-bench] wrote $(dirname "$OUT_CSV")/kernel_host_cpu.txt cpu=$HOST_CPU"
