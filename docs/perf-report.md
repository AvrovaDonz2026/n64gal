# Perf Report Workflow

## Goals

1. 固定场景、分辨率、热身窗口与采样字段。
2. 既能比较不同 backend，也能比较同一 backend 的前后提交。
3. 输出既适合人读，也适合 CI artifact 保存。
4. 允许跨架构/跨 runner 复用同一套 perf 脚本，而不要求历史 revision 自带新工具能力。

## Inputs And Overrides

`tests/perf/run_perf.sh` 默认在当前仓库源码树内构建并运行，但也支持把“当前脚本”指向任意源码快照：

1. `--source-root DIR`：在指定源码树中打包、编译、运行。
2. `CC=...`：切换编译器，例如 `riscv64-linux-gnu-gcc`。
3. `VN_PERF_CFLAGS=...`：附加交叉架构编译参数，例如 `-march=rv64gcv -mabi=lp64d`。
4. `VN_PERF_LDFLAGS=...`：附加链接参数。
5. `VN_PERF_RUNNER_PREFIX=...`：为 perf runner 加前缀，例如 `qemu-riscv64 -cpu max,v=true -L /usr/riscv64-linux-gnu`。
6. `VN_PERF_RUNNER_BIN=...`：覆盖临时 runner 输出路径，便于并行或 revision 对照。

这套注入能力也是 `run_perf_compare_revs.sh` 的基础：脚本会先 `git archive` 两个 revision，再复用当前版本的 `run_perf.sh` 去驱动 baseline/candidate 两个源码树。因此，历史 revision 不需要额外回填新的 perf 工具逻辑。

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

## Baseline vs Candidate Revision

同一 backend 前后对照时，优先使用 `run_perf_compare_revs.sh`，而不是手动切分两个工作树：

```bash
./tests/perf/run_perf_compare_revs.sh \
  --baseline-rev 75ee8f9 \
  --candidate-rev ee42c39 \
  --backend rvv \
  --scenes S0,S3 \
  --duration-sec 2 \
  --warmup-sec 1 \
  --dt-ms 16 \
  --resolution 600x800 \
  --out-dir /tmp/n64gal_perf_rvv_compare
```

跨架构 runner 示例：

```bash
CC=riscv64-linux-gnu-gcc \
VN_PERF_CFLAGS='-march=rv64gcv -mabi=lp64d' \
VN_PERF_RUNNER_PREFIX='qemu-riscv64 -cpu max,v=true -L /usr/riscv64-linux-gnu' \
./tests/perf/run_perf_compare_revs.sh \
  --baseline-rev 75ee8f9 \
  --candidate-rev ee42c39 \
  --backend rvv \
  --scenes S0,S3 \
  --duration-sec 2 \
  --warmup-sec 1 \
  --dt-ms 16 \
  --resolution 600x800 \
  --out-dir /tmp/n64gal_perf_rvv_compare
```

输出目录结构：

1. `baseline/perf_summary.csv`
2. `candidate/perf_summary.csv`
3. `compare/perf_compare.csv`
4. `compare/perf_compare.md`
5. `compare/perf_compare_revs.md`

其中 `perf_compare_revs.md` 会额外记录 host、compiler、runner prefix 与 revision 元数据，适合直接入库作为 issue 证据链。

## Checked-In Evidence

当前仓库已经固化一份 RVV qemu smoke 报告：

1. [`docs/perf-rvv-2026-03-06.md`](./perf-rvv-2026-03-06.md)

这份报告对应 `75ee8f9 -> ee42c39` 的 `rvv` 对比，主要用于证明融合优化系列在 `qemu-user` 环境下可以稳定得到正收益。它不是发布级原生基准，不能替代 riscv64 真机 perf。

另外，CI 已新增 `.github/workflows/riscv-perf-report.yml` 与 `scripts/ci/run_riscv64_qemu_perf_report.sh` 包装入口，供 `workflow_dispatch` / nightly 的 `linux-riscv64-qemu-rvv-perf-report` job 直接产出同格式 artifact。该 workflow 默认使用比本地 smoke 更长的 `4s/2s` 窗口，以降低 qemu-user 短窗口抖动。

## CI Artifact

`linux-x64` CI job 会生成 `perf-linux-x64` artifact，默认比较：

1. `scalar`
2. `avx2`

当前 CI 目标是快速回归与报告留档，不替代长时间本地压测。需要发布级结论时，仍应运行完整 `120s/20s` 窗口，并优先在原生目标机上采样。

## Reading Rules

1. 优先看 `p95_frame_ms`，其次看 `avg_frame_ms`。
2. `gain > 0` 代表 candidate 更快。
3. `rss_delta_mb < 0` 代表 candidate 峰值 RSS 更低。
4. 所有性能结论都必须附带：commit、设备、OS、toolchain、命令行参数。
5. `qemu-user` perf 只用于回归趋势判断；发版门槛必须看目标架构原生机器结果。
