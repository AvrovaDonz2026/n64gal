# Perf Report Workflow

## Goals

1. 固定场景、分辨率、热身窗口与采样字段。
2. 既能比较不同 backend，也能比较同一 backend 的前后提交。
3. 输出既适合人读，也适合 CI artifact 保存。

## Single Backend Capture

```bash
./tests/perf/run_perf.sh \
  --backend scalar \
  --scenes S0,S1,S2,S3 \
  --duration-sec 120 \
  --warmup-sec 20 \
  --dt-ms 16 \
  --resolution 600x800 \
  --out-dir /tmp/n64gal_perf/scalar
```

输出：

1. `perf_<scene>.csv`
2. `perf_summary.csv`
3. `perf_report_template.md`

## Baseline vs Candidate Backend

```bash
./tests/perf/run_perf_compare.sh \
  --baseline scalar \
  --candidate avx2 \
  --scenes S0,S1,S2,S3 \
  --duration-sec 120 \
  --warmup-sec 20 \
  --dt-ms 16 \
  --resolution 600x800 \
  --out-dir /tmp/n64gal_perf/compare_scalar_avx2
```

输出目录结构：

1. `scalar/perf_summary.csv`
2. `avx2/perf_summary.csv`
3. `compare/perf_compare.csv`
4. `compare/perf_compare.md`

`perf_compare.md` 会给出每个 scene 的 `p95/avg/max_rss` 对比，以及 speedup / gain 百分比。

## Before vs After Commit

同一 backend 前后对照时，先在两个提交分别采样，再用 `compare_perf.sh` 合并：

```bash
./tests/perf/compare_perf.sh \
  --baseline before:/tmp/n64gal_perf_before/rvv/perf_summary.csv \
  --candidate after:/tmp/n64gal_perf_after/rvv/perf_summary.csv \
  --out-dir /tmp/n64gal_perf_delta/rvv
```

这条命令正是用来固化类似“RVV `combine`/`tex-hash` 向量化前后收益”的标准路径。

## CI Artifact

`linux-x64` CI job 会生成 `perf-linux-x64` artifact，默认比较：

1. `scalar`
2. `avx2`

当前 CI 目标是快速回归与报告留档，不替代长时间本地压测。需要发布级结论时，仍应运行完整 `120s/20s` 窗口。

## Reading Rules

1. 优先看 `p95_frame_ms`，其次看 `avg_frame_ms`。
2. `gain > 0` 代表 candidate 更快。
3. `rss_delta_mb < 0` 代表 candidate 峰值 RSS 更低。
4. 所有性能结论都必须附带：commit、设备、OS、toolchain、命令行参数。
