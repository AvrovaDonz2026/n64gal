# N64GAL Perf Report Template

## 1. Metadata

- Date:
- Branch/Commit:
- Backend or Comparison Pair:
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

Single backend:

```bash
./tests/perf/run_perf.sh --backend scalar --scenes S0,S1,S2,S3 --duration-sec 120 --warmup-sec 20 --dt-ms 16 --resolution 600x800
```

Baseline vs candidate:

```bash
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S0,S1,S2,S3 --duration-sec 120 --warmup-sec 20 --dt-ms 16 --resolution 600x800
```

## 4. Summary

Single backend capture:

- `perf_<scene>.csv`
- `perf_summary.csv`

Comparison capture:

- `compare/perf_compare.csv`
- `compare/perf_compare.md`

## 5. Notes

- Any fallback events (`backend init failed`, etc.)
- Test environment noise
- Known limitations
- If this is a before/after optimization check, attach both commit ids
