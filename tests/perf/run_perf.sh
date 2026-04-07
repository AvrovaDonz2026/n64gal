#!/usr/bin/env bash
set -euo pipefail

SCRIPT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SOURCE_ROOT="${VN_PERF_SOURCE_ROOT:-$SCRIPT_ROOT}"

BACKEND="scalar"
SCENES="S0,S1,S2,S3,S10"
OUT_DIR="tests/perf"
DURATION_SEC=120
WARMUP_SEC=20
DT_MS=16
RESOLUTION="600x800"
FRAMES_OVERRIDE=""
KEEP_RAW=0
MAX_PASSES=2048
PERF_CC="${CC:-cc}"
PERF_RUNNER_BIN="${VN_PERF_RUNNER_BIN:-/tmp/n64gal_perf_runner}"
SKIP_BUILD="${VN_PERF_SKIP_BUILD:-0}"
BUILD_ONLY="${VN_PERF_BUILD_ONLY:-0}"
PERF_RUNNER_PREFIX="${VN_PERF_RUNNER_PREFIX:-}"
PERF_CFLAGS="${VN_PERF_CFLAGS:-}"
PERF_LDFLAGS="${VN_PERF_LDFLAGS:-}"
PERF_FRAME_REUSE=""
PERF_OP_CACHE=""
PERF_DIRTY_TILE=""
PERF_DYNAMIC_RESOLUTION=""

append_source_if_exists() {
  local rel_path

  rel_path="$1"
  if [[ -f "$SOURCE_ROOT/$rel_path" ]]; then
    COMPILE_CMD+=("$rel_path")
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --backend)
      BACKEND="$2"
      shift 2
      ;;
    --source-root)
      SOURCE_ROOT="$2"
      shift 2
      ;;
    --runner-bin)
      PERF_RUNNER_BIN="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift 1
      ;;
    --build-only)
      BUILD_ONLY=1
      shift 1
      ;;
    --scenes)
      SCENES="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    --duration-sec)
      DURATION_SEC="$2"
      shift 2
      ;;
    --warmup-sec)
      WARMUP_SEC="$2"
      shift 2
      ;;
    --dt-ms)
      DT_MS="$2"
      shift 2
      ;;
    --resolution)
      RESOLUTION="$2"
      shift 2
      ;;
    --frames)
      FRAMES_OVERRIDE="$2"
      shift 2
      ;;
    --keep-raw)
      KEEP_RAW=1
      shift 1
      ;;
    --perf-frame-reuse)
      PERF_FRAME_REUSE="$2"
      shift 2
      ;;
    --perf-op-cache)
      PERF_OP_CACHE="$2"
      shift 2
      ;;
    --perf-dirty-tile)
      PERF_DIRTY_TILE="$2"
      shift 2
      ;;
    --perf-dynamic-resolution)
      PERF_DYNAMIC_RESOLUTION="$2"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ ! -d "$SOURCE_ROOT" ]]; then
  echo "source-root not found: $SOURCE_ROOT" >&2
  exit 2
fi
SOURCE_ROOT="$(cd "$SOURCE_ROOT" && pwd)"
cd "$SOURCE_ROOT"

source "$SCRIPT_ROOT/tests/perf/host_cpu.sh"

if [[ "$DT_MS" -le 0 ]]; then
  echo "dt-ms must be > 0" >&2
  exit 2
fi
if [[ "$DURATION_SEC" -le 0 ]]; then
  echo "duration-sec must be > 0" >&2
  exit 2
fi
if [[ "$WARMUP_SEC" -lt 0 ]]; then
  echo "warmup-sec must be >= 0" >&2
  exit 2
fi
if [[ "$WARMUP_SEC" -ge "$DURATION_SEC" ]]; then
  echo "warmup-sec must be < duration-sec" >&2
  exit 2
fi

if ! [[ "$SKIP_BUILD" =~ ^[01]$ ]]; then
  echo "skip-build must be 0 or 1" >&2
  exit 2
fi
if ! [[ "$BUILD_ONLY" =~ ^[01]$ ]]; then
  echo "build-only must be 0 or 1" >&2
  exit 2
fi

if [[ -n "$FRAMES_OVERRIDE" ]]; then
  FRAMES="$FRAMES_OVERRIDE"
else
  FRAMES=$(( (DURATION_SEC * 1000) / DT_MS ))
fi
if [[ "$FRAMES" -le 0 ]]; then
  echo "frames must be > 0" >&2
  exit 2
fi

WARMUP_MS=$(( WARMUP_SEC * 1000 ))
TOTAL_MS=$(( DURATION_SEC * 1000 ))

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  mkdir -p "$(dirname "$PERF_RUNNER_BIN")"

  COMPILE_CMD=(
    "$PERF_CC"
    -std=c89
    -pedantic-errors
    -Wall
    -Wextra
    -Werror
  )
  if [[ -n "$PERF_CFLAGS" ]]; then
    # shellcheck disable=SC2206
    EXTRA_CFLAGS=( $PERF_CFLAGS )
    COMPILE_CMD+=("${EXTRA_CFLAGS[@]}")
  fi
  COMPILE_CMD+=(
    -Iinclude
    src/main.c
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
  )
  append_source_if_exists src/core/dynamic_resolution.c
  COMPILE_CMD+=(
    src/frontend/render_ops.c
  )
  append_source_if_exists src/frontend/dirty_tiles.c
  COMPILE_CMD+=(
    src/backend/common/pixel_pipeline.c
    src/backend/avx2/avx2_backend.c
    src/backend/avx2/avx2_fill_fade.c
  )
  append_source_if_exists src/backend/avx2/avx2_textured.c
  COMPILE_CMD+=(
    src/backend/neon/neon_backend.c
    src/backend/rvv/rvv_backend.c
    src/backend/scalar/scalar_backend.c
    -o
    "$PERF_RUNNER_BIN"
  )
  if [[ -n "$PERF_LDFLAGS" ]]; then
    # shellcheck disable=SC2206
    EXTRA_LDFLAGS=( $PERF_LDFLAGS )
    COMPILE_CMD+=("${EXTRA_LDFLAGS[@]}")
  fi

  "${COMPILE_CMD[@]}"
else
  if [[ ! -f "$PERF_RUNNER_BIN" ]]; then
    echo "runner-bin not found: $PERF_RUNNER_BIN" >&2
    exit 2
  fi
fi

if [[ "$BUILD_ONLY" -eq 1 ]]; then
  exit 0
fi

mkdir -p "$OUT_DIR"

HOST_CPU="$(vn_perf_detect_host_cpu)"
printf "%s\n" "$HOST_CPU" > "$OUT_DIR/perf_host_cpu.txt"

"$SOURCE_ROOT/tools/packer/make_demo_pack.sh" >/tmp/vn_make_pack.out

SUMMARY_CSV="$OUT_DIR/perf_summary.csv"
{
  echo "scene,samples,p95_frame_ms,avg_frame_ms,max_rss_mb,warmup_sec,duration_sec,backend,dt_ms,resolution,passes,perf_frame_reuse,perf_op_cache,perf_dirty_tile,perf_dynamic_resolution,requested_backend,actual_backend,host_cpu"
} > "$SUMMARY_CSV"

IFS=',' read -r -a SCENE_ARRAY <<< "$SCENES"

for SCENE in "${SCENE_ARRAY[@]}"; do
  OUT_CSV="$OUT_DIR/perf_${SCENE}.csv"
  {
    echo "scene,frame,frame_ms,vm_ms,build_ms,raster_ms,audio_ms,rss_mb"
  } > "$OUT_CSV"

  SIM_ELAPSED_MS="0.000"
  FRAME_CURSOR=0
  PASS=0
  ACTUAL_BACKEND=""

  while awk "BEGIN { exit !($SIM_ELAPSED_MS < $TOTAL_MS) }"; do
    PASS=$((PASS + 1))
    if [[ "$PASS" -gt "$MAX_PASSES" ]]; then
      echo "[perf] exceeded max passes scene=$SCENE max_passes=$MAX_PASSES" >&2
      exit 1
    fi

    RAW_LOG="$OUT_DIR/perf_${SCENE}.raw.${PASS}.log"
    RAW_ERR="$OUT_DIR/perf_${SCENE}.raw.${PASS}.err"

    RUNNER_CMD=(
      "$PERF_RUNNER_BIN"
      --backend="$BACKEND"
      --scene="$SCENE"
      --resolution="$RESOLUTION"
      --frames="$FRAMES"
      --dt-ms="$DT_MS"
      --trace
      --hold-end
    )
    if [[ -n "$PERF_FRAME_REUSE" ]]; then
      RUNNER_CMD+=("--perf-frame-reuse=$PERF_FRAME_REUSE")
    fi
    if [[ -n "$PERF_OP_CACHE" ]]; then
      RUNNER_CMD+=("--perf-op-cache=$PERF_OP_CACHE")
    fi
    if [[ -n "$PERF_DIRTY_TILE" ]]; then
      RUNNER_CMD+=("--perf-dirty-tile=$PERF_DIRTY_TILE")
    fi
    if [[ -n "$PERF_DYNAMIC_RESOLUTION" ]]; then
      RUNNER_CMD+=("--perf-dynamic-resolution=$PERF_DYNAMIC_RESOLUTION")
    fi
    if [[ -n "$PERF_RUNNER_PREFIX" ]]; then
      # shellcheck disable=SC2206
      RUNNER_PREFIX=( $PERF_RUNNER_PREFIX )
      RUNNER_CMD=("${RUNNER_PREFIX[@]}" "${RUNNER_CMD[@]}")
    fi

    "${RUNNER_CMD[@]}" >"$RAW_LOG" 2>"$RAW_ERR"

    ACTUAL_BACKEND_PASS="$(awk '
      /^vn_runtime ok / {
        for (i = 1; i <= NF; ++i) {
          split($i, kv, "=");
          if (length(kv) == 2 && kv[1] == "backend") {
            backend = kv[2];
          }
        }
      }
      END {
        if (backend != "") {
          print backend;
        }
      }
    ' "$RAW_LOG")"
    if [[ -z "$ACTUAL_BACKEND_PASS" ]]; then
      ACTUAL_BACKEND_PASS="unknown"
    fi
    if [[ -z "$ACTUAL_BACKEND" || "$ACTUAL_BACKEND" == "unknown" ]]; then
      ACTUAL_BACKEND="$ACTUAL_BACKEND_PASS"
    elif [[ "$ACTUAL_BACKEND_PASS" != "unknown" && "$ACTUAL_BACKEND_PASS" != "$ACTUAL_BACKEND" ]]; then
      echo "[perf] inconsistent actual backend scene=$SCENE requested=$BACKEND first=$ACTUAL_BACKEND current=$ACTUAL_BACKEND_PASS pass=$PASS" >&2
      echo "[perf] see $RAW_LOG / $RAW_ERR" >&2
      exit 1
    fi

    STATE_TMP="$(mktemp)"
    awk \
      -v scene="$SCENE" \
      -v warmup_ms="$WARMUP_MS" \
      -v max_ms="$TOTAL_MS" \
      -v sim_start="$SIM_ELAPSED_MS" \
      -v dt_ms="$DT_MS" \
      -v frame_start="$FRAME_CURSOR" \
      -v state_out="$STATE_TMP" \
      '
      BEGIN {
        sim_elapsed = sim_start + 0.0;
        frame_idx = frame_start + 0;
      }
      {
        frame = "";
        frame_ms = "";
        vm_ms = "";
        build_ms = "";
        raster_ms = "";
        audio_ms = "";
        rss_mb = "";
        n = split($0, fields, " ");
        for (i = 1; i <= n; ++i) {
          split(fields[i], kv, "=");
          if (length(kv) != 2) {
            continue;
          }
          if (kv[1] == "frame") {
            frame = kv[2];
          } else if (kv[1] == "frame_ms") {
            frame_ms = kv[2];
          } else if (kv[1] == "vm_ms") {
            vm_ms = kv[2];
          } else if (kv[1] == "build_ms") {
            build_ms = kv[2];
          } else if (kv[1] == "raster_ms") {
            raster_ms = kv[2];
          } else if (kv[1] == "audio_ms") {
            audio_ms = kv[2];
          } else if (kv[1] == "rss_mb") {
            rss_mb = kv[2];
          }
        }
        if (frame_ms == "") {
          next;
        }
        sim_elapsed += (dt_ms + 0.0);
        if (sim_elapsed > (max_ms + 0.0001)) {
          exit;
        }
        if (sim_elapsed <= (warmup_ms + 0.0)) {
          next;
        }
        if (vm_ms == "") {
          vm_ms = "0.000";
        }
        if (build_ms == "") {
          build_ms = "0.000";
        }
        if (raster_ms == "") {
          raster_ms = "0.000";
        }
        if (audio_ms == "") {
          audio_ms = "0.000";
        }
        if (rss_mb == "") {
          rss_mb = "0.000";
        }
        printf "%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
               scene,
               frame_idx,
               frame_ms + 0.0,
               vm_ms + 0.0,
               build_ms + 0.0,
               raster_ms + 0.0,
               audio_ms + 0.0,
               rss_mb + 0.0;
        frame_idx += 1;
      }
      END {
        printf "%.3f %d\n", sim_elapsed, frame_idx > state_out;
      }
      ' "$RAW_LOG" >> "$OUT_CSV"

    read -r NEW_SIM_ELAPSED_MS NEW_FRAME_CURSOR < "$STATE_TMP"
    rm -f "$STATE_TMP"

    if awk "BEGIN { exit !($NEW_SIM_ELAPSED_MS <= ($SIM_ELAPSED_MS + 0.0001)) }"; then
      echo "[perf] no simulation progress scene=$SCENE pass=$PASS; see $RAW_LOG / $RAW_ERR" >&2
      exit 1
    fi

    SIM_ELAPSED_MS="$NEW_SIM_ELAPSED_MS"
    FRAME_CURSOR="$NEW_FRAME_CURSOR"

    if [[ "$KEEP_RAW" -eq 0 ]]; then
      rm -f "$RAW_LOG" "$RAW_ERR"
    fi
  done

  SAMPLE_COUNT=$(( $(wc -l < "$OUT_CSV") - 1 ))
  if [[ "$SAMPLE_COUNT" -le 0 ]]; then
    echo "[perf] no samples for scene=$SCENE after warmup window=${WARMUP_SEC}s" >&2
    exit 1
  fi

  SORT_TMP="$(mktemp)"
  tail -n +2 "$OUT_CSV" | cut -d, -f3 | sort -n > "$SORT_TMP"
  P95_INDEX=$(( (95 * SAMPLE_COUNT + 99) / 100 ))
  P95_FRAME_MS="$(sed -n "${P95_INDEX}p" "$SORT_TMP")"
  rm -f "$SORT_TMP"

  AVG_FRAME_MS="$(awk -F, 'NR>1 { sum += $3; n += 1 } END { if (n == 0) printf "0.000"; else printf "%.3f", sum / n }' "$OUT_CSV")"
  MAX_RSS_MB="$(awk -F, 'NR>1 { if ($8 + 0 > max) max = $8 + 0 } END { printf "%.3f", max + 0.0 }' "$OUT_CSV")"

  {
    echo "${SCENE},${SAMPLE_COUNT},${P95_FRAME_MS},${AVG_FRAME_MS},${MAX_RSS_MB},${WARMUP_SEC},${DURATION_SEC},${BACKEND},${DT_MS},${RESOLUTION},${PASS},${PERF_FRAME_REUSE:-default},${PERF_OP_CACHE:-default},${PERF_DIRTY_TILE:-default},${PERF_DYNAMIC_RESOLUTION:-default},${BACKEND},${ACTUAL_BACKEND:-unknown},${HOST_CPU}"
  } >> "$SUMMARY_CSV"

  echo "[perf] wrote $OUT_CSV samples=$SAMPLE_COUNT p95=${P95_FRAME_MS}ms passes=$PASS requested_backend=$BACKEND actual_backend=${ACTUAL_BACKEND:-unknown}"
done

cp "$SCRIPT_ROOT/tests/perf/report_template.md" "$OUT_DIR/perf_report_template.md"
if [[ "$SKIP_BUILD" -eq 0 ]]; then
  rm -f "$PERF_RUNNER_BIN"
fi

echo "[perf] wrote $OUT_DIR/perf_report_template.md"
echo "[perf] wrote $OUT_DIR/perf_host_cpu.txt cpu=$HOST_CPU"
echo "[perf] done backend=$BACKEND scenes=$SCENES duration_sec=$DURATION_SEC warmup_sec=$WARMUP_SEC dt_ms=$DT_MS source_root=$SOURCE_ROOT perf_frame_reuse=${PERF_FRAME_REUSE:-default} perf_op_cache=${PERF_OP_CACHE:-default} perf_dirty_tile=${PERF_DIRTY_TILE:-default} perf_dynamic_resolution=${PERF_DYNAMIC_RESOLUTION:-default} host_cpu=$HOST_CPU actual_backend=per-scene"
