# Dirty-Tile Repeat-Median Perf Report (2026-03-07)

## Scope

1. Goal: verify that the landed dirty-tile runtime path is no longer paying avoidable planner cost in scenes that are known to collapse to full redraw, and reduce single-run drift in dirty on/off measurement.
2. Commit scope: `HEAD` after the dirty runtime fast-path, full-redraw shallow commit / lazy `prev_bounds` refresh, partial-path incremental tile counting, and compare `--repeat` aggregation landed on top of `97cc92a`.
3. Scope: `avx2` backend, `S0,S3`, requested resolution `600x800`, compare `dirty-tile off -> on`.
4. This is local trend evidence, not a release benchmark.

## Environment

1. Host: `Linux x86_64`
2. Compiler: `cc (Debian 14.2.0-19) 14.2.0`
3. Runtime defaults preserved during compare:
   - `VN_RUNTIME_PERF_FRAME_REUSE=on`
   - `VN_RUNTIME_PERF_OP_CACHE=on`
4. Variable under test:
   - `VN_RUNTIME_PERF_DIRTY_TILE=off/on`
5. Measurement note:
   - short-window runs remained noisy on this host, so the compare now uses `repeat=3` and aggregates `p95_frame_ms` / `avg_frame_ms` / `max_rss_mb` by scene median.

## Command

```bash
./tests/perf/run_perf_compare.sh \
  --baseline avx2 \
  --baseline-label avx2_dirty_off \
  --baseline-perf-dirty-tile off \
  --candidate avx2 \
  --candidate-label avx2_dirty_on \
  --candidate-perf-dirty-tile on \
  --scenes S0,S3 \
  --duration-sec 6 \
  --warmup-sec 1 \
  --dt-ms 16 \
  --resolution 600x800 \
  --repeat 3 \
  --out-dir /tmp/avx2_dirty_compare_repeat3
```

## Per-Run S3 p95

| run | dirty off p95 ms | dirty on p95 ms | p95 gain |
|---|---:|---:|---:|
| run_01 | 12.545 | 13.365 | -6.54% |
| run_02 | 13.682 | 12.583 | 8.03% |
| run_03 | 14.935 | 12.591 | 15.69% |

## Repeat-Median Aggregate Result

| scene | baseline samples | candidate samples | baseline p95 ms | candidate p95 ms | p95 speedup | p95 gain | baseline avg ms | candidate avg ms | avg speedup | avg gain | rss delta mb |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| S0 | 313 | 313 | 0.001 | 0.001 | 1.000x | 0.00% | 0.000 | 0.000 | 0.000x | 0.00% | 0.008 |
| S3 | 313 | 313 | 13.682 | 12.591 | 1.087x | 7.97% | 11.893 | 11.582 | 1.027x | 2.61% | 0.016 |

## Interpretation

1. Runtime-side fixed costs have now been reduced in three places: forced-full-redraw planner short-circuit, full-redraw shallow commit, and partial-path incremental tile counting.
2. A single short run can still flip sign on a noisy host, but `repeat=3` median aggregation preserves the positive direction for `S3` without changing renderer semantics.
3. This does not make dirty-tile a release gate yet; it makes the compare artifact more trustworthy for issue tracking and CI trend review.
4. The default runtime policy stays conservative: keep `VN_RUNTIME_PERF_DIRTY_TILE` default-off until target-machine evidence is stronger.

## Caveats

1. These numbers are local Linux x64 samples, not GitHub runner measurements.
2. Host load was visibly elevated during the session, so absolute frame times are worse than earlier quieter local runs; the useful signal here is the repeat-median direction, not the absolute p95 value.
3. This report is regression evidence for `ISSUE-008`, not a release benchmark.
