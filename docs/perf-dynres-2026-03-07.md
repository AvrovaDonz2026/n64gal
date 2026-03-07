# Dynamic Resolution Smoke Report (2026-03-07)

## Scope

1. Goal: validate the landed dynamic-resolution runtime slice on a real runtime path, not just controller/unit tests.
2. Commit: `97cc92a` (`perf: land dynamic resolution runtime slice`)
3. Scope: `scalar` backend, `S3`, requested resolution `1200x1600`, compare `dynamic-resolution off -> on`.
4. This is a smoke-level perf report, not a release benchmark.

## Environment

1. Host: `Linux x86_64`
2. Compiler: `cc (Debian 14.2.0-19) 14.2.0`
3. Runtime defaults preserved during compare:
   - `VN_RUNTIME_PERF_FRAME_REUSE=on`
   - `VN_RUNTIME_PERF_OP_CACHE=on`
4. Variable under test:
   - `VN_RUNTIME_PERF_DYNAMIC_RESOLUTION=off/on`

## Command

```bash
./tests/perf/run_perf_compare.sh \
  --baseline scalar \
  --baseline-label scalar_dynres_off \
  --baseline-perf-dynamic-resolution off \
  --candidate scalar \
  --candidate-label scalar_dynres_on \
  --candidate-perf-dynamic-resolution on \
  --scenes S3 \
  --duration-sec 6 \
  --warmup-sec 1 \
  --dt-ms 16 \
  --resolution 1200x1600 \
  --out-dir /tmp/n64gal_perf_dynres_compare
```

## Result

| scene | baseline samples | candidate samples | baseline p95 ms | candidate p95 ms | p95 speedup | p95 gain | baseline avg ms | candidate avg ms | avg speedup | avg gain | rss delta mb |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| S3 | 313 | 313 | 32.513 | 29.265 | 1.111x | 9.99% | 29.600 | 15.434 | 1.918x | 47.86% | 0.078 |

## Interpretation

1. The runtime controller is able to reduce `p95_frame_ms` on an oversubscribed scene without changing the frontend/backend API contract.
2. `avg_frame_ms` improves much more than `p95_frame_ms`, which matches the expected behavior of a downshifted steady-state path after the first switch.
3. Peak RSS change is negligible in this smoke window.

## Caveats

1. This is a local Linux x64 smoke result, not a GitHub runner result.
2. This covers only `scalar` and only `S3`; it does not prove the same gain shape for `avx2`, `neon`, or `rvv`.
3. The result is intended as checked-in regression evidence for `ISSUE-008`, not as a ship/no-ship performance gate.
