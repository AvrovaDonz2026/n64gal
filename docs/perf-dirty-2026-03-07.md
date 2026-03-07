# Dirty-Tile Repeat-Median Perf Report (2026-03-07)

## Scope

1. Goal: verify that the landed dirty-tile runtime path is no longer paying avoidable planner cost in scenes that are known to collapse to full redraw, and reduce single-run drift in dirty on/off measurement.
2. Commit scope: `HEAD` after the dirty runtime fast-path, full-redraw shallow commit / lazy `prev_bounds` refresh, partial-path incremental tile counting, and compare `--repeat` aggregation landed on top of `97cc92a`.
3. Scope: `avx2` backend, `S1,S3`, requested resolution `600x800`, compare `dirty-tile off -> on`.
4. Rationale for scene selection: `S0` now collapses to about `0.001ms` on the shipped `frame reuse + op cache` path, so it is no longer useful for measuring dirty-tile effect; the compare moved to `S1,S3` to retain one medium-complexity sample plus one heavier sample.
5. This is local trend evidence, not a release benchmark.

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
  --scenes S1,S3 \
  --duration-sec 6 \
  --warmup-sec 1 \
  --dt-ms 16 \
  --resolution 600x800 \
  --repeat 3 \
  --out-dir /tmp/n64gal_dirty_S1S3_repeat3
```

## Repeat-Median Aggregate Result

| scene | baseline samples | candidate samples | baseline p95 ms | candidate p95 ms | p95 speedup | p95 gain | baseline avg ms | candidate avg ms | avg speedup | avg gain | rss delta mb |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| S1 | 313 | 313 | 15.228 | 13.507 | 1.127x | 11.30% | 12.744 | 12.239 | 1.041x | 3.96% | -0.027 |
| S3 | 313 | 313 | 15.971 | 16.545 | 0.965x | -3.59% | 12.898 | 12.817 | 1.006x | 0.63% | 0.043 |

## Interpretation

1. Moving from `S0` to `S1` makes the compare materially more informative: the medium-complexity sample now shows a measurable dirty-tile win instead of a collapsed `0.001ms` flatline.
2. Runtime-side fixed costs have now been reduced in three places: forced-full-redraw planner short-circuit, full-redraw shallow commit, and partial-path incremental tile counting.
3. The current host is still noisy enough that `S3` can flip sign in a short `repeat=3` window, so dirty-tile should remain a trend artifact rather than a hard release gate.
4. The default runtime policy stays conservative: keep `VN_RUNTIME_PERF_DIRTY_TILE` default-off until target-machine evidence is stronger.

## Caveats

1. These numbers are local Linux x64 samples, not GitHub runner measurements.
2. The useful signal here is scene choice plus trend direction, not the absolute p95 value.
3. This report is regression evidence for `ISSUE-008`, not a release benchmark.
