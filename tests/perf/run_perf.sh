#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BACKEND="scalar"
SCENES="S0,S1,S2,S3"
OUT_DIR="tests/perf"
DURATION_SEC=120
WARMUP_SEC=20
DT_MS=16
RESOLUTION="600x800"
FRAMES_OVERRIDE=""
KEEP_RAW=0
MAX_PASSES=2048

while [[ $# -gt 0 ]]; do
  case "$1" in
    --backend)
      BACKEND="$2"
      shift 2
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
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

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

mkdir -p "$OUT_DIR"

"$ROOT_DIR/tools/packer/make_demo_pack.sh" >/tmp/vn_make_pack.out

cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -Iinclude \
  src/main.c \
  src/core/backend_registry.c \
  src/core/renderer.c \
  src/core/vm.c \
  src/core/pack.c \
  src/core/runtime_cli.c \
  src/frontend/render_ops.c \
  src/backend/common/pixel_pipeline.c \
  src/backend/avx2/avx2_backend.c \
  src/backend/neon/neon_backend.c \
  src/backend/rvv/rvv_backend.c \
  src/backend/scalar/scalar_backend.c \
  -o /tmp/n64gal_perf_runner

SUMMARY_CSV="$OUT_DIR/perf_summary.csv"
{
  echo "scene,samples,p95_frame_ms,avg_frame_ms,max_rss_mb,warmup_sec,duration_sec,backend,dt_ms,resolution,passes"
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

  while awk "BEGIN { exit !($SIM_ELAPSED_MS < $TOTAL_MS) }"; do
    PASS=$((PASS + 1))
    if [[ "$PASS" -gt "$MAX_PASSES" ]]; then
      echo "[perf] exceeded max passes scene=$SCENE max_passes=$MAX_PASSES" >&2
      exit 1
    fi

    RAW_LOG="$OUT_DIR/perf_${SCENE}.raw.${PASS}.log"
    RAW_ERR="$OUT_DIR/perf_${SCENE}.raw.${PASS}.err"

    /tmp/n64gal_perf_runner \
      --backend="$BACKEND" \
      --scene="$SCENE" \
      --resolution="$RESOLUTION" \
      --frames="$FRAMES" \
      --dt-ms="$DT_MS" \
      --trace \
      --hold-end \
      >"$RAW_LOG" \
      2>"$RAW_ERR"

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
    echo "${SCENE},${SAMPLE_COUNT},${P95_FRAME_MS},${AVG_FRAME_MS},${MAX_RSS_MB},${WARMUP_SEC},${DURATION_SEC},${BACKEND},${DT_MS},${RESOLUTION},${PASS}"
  } >> "$SUMMARY_CSV"

  echo "[perf] wrote $OUT_CSV samples=$SAMPLE_COUNT p95=${P95_FRAME_MS}ms passes=$PASS"
done

cp "$ROOT_DIR/tests/perf/report_template.md" "$OUT_DIR/perf_report_template.md"
rm -f /tmp/n64gal_perf_runner

echo "[perf] wrote $SUMMARY_CSV"
echo "[perf] wrote $OUT_DIR/perf_report_template.md"
echo "[perf] done backend=$BACKEND scenes=$SCENES duration_sec=$DURATION_SEC warmup_sec=$WARMUP_SEC dt_ms=$DT_MS"
