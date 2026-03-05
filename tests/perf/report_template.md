# N64GAL Perf Report Template

## 1. Metadata

- Date:
- Branch/Commit:
- Backend:
- Resolution:
- Duration (sec):
- Warmup (sec):
- dt_ms:

## 2. Device

- CPU:
- Memory:
- OS:
- Toolchain:

## 3. Command

```bash
./tests/perf/run_perf.sh --backend scalar --scenes S0,S1,S2,S3 --duration-sec 120 --warmup-sec 20 --dt-ms 16 --resolution 600x800
```

## 4. Summary

Copy from `tests/perf/perf_summary.csv`:

- scene
- samples
- p95_frame_ms
- avg_frame_ms
- max_rss_mb

## 5. Notes

- Any fallback events (`backend init failed`, etc.)
- Test environment noise
- Known limitations
