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

## Runtime Perf Flags

当前 perf 脚本默认沿用 runtime 主线默认开关，也就是同时开启：

1. `VN_RUNTIME_PERF_FRAME_REUSE`
2. `VN_RUNTIME_PERF_OP_CACHE`

当前还提供两个额外的可观测开关：

3. `VN_RUNTIME_PERF_DIRTY_TILE`（默认 `off`）
4. `VN_RUNTIME_PERF_DYNAMIC_RESOLUTION`（默认 `off`）

这意味着：

1. `run_perf.sh` / `run_perf_compare.sh` / `run_perf_compare_revs.sh` 测到的是“当前 shipped 路径”的整机收益，而不是纯 backend 微基准。
2. 在稳定等待帧里，`frame_reuse_hit=1` 时会直接复用上一帧 framebuffer，因此 `raster_ms` 可能接近 `0.000`。
3. `op_cache_hit=1` 只代表跳过了 `VNRenderOp[]` 构建；该路径仍会执行 `renderer_submit` 与 raster。
4. `VN_RUNTIME_PERF_DIRTY_TILE` 已驱动实际 dirty submit；当前 `scalar` / `avx2` / `neon` / `rvv` 都实现了这条路径。
5. `VN_RUNTIME_PERF_DYNAMIC_RESOLUTION` 开启后，runtime 可能在一次 run 内把实际渲染尺寸从请求尺寸切到 `R1(75%)` 或 `R2(50%)`；当前 `perf_summary.csv` 会记录 `perf_dynamic_resolution` 开关状态，但若要看实际切档层级与最终尺寸，仍建议配合 `vn_player --trace` 或 preview `final_state` 一起看。
6. `run_perf.sh` 与 `run_perf_compare.sh` 现在都支持 `--perf-frame-reuse` / `--perf-op-cache` / `--perf-dirty-tile` / `--perf-dynamic-resolution`，可以直接做同一 backend 的开关对照。

若需要拆开归因，既可以直接运行 `vn_player --trace`，也可以把 perf 开关直接传给 perf 脚本：

```bash
./build_ci_cc/vn_player --scene S0 --frames 32 --hold-end --trace
./build_ci_cc/vn_player --scene S0 --frames 32 --hold-end --trace --perf-frame-reuse=off
./build_ci_cc/vn_player --scene S0 --frames 32 --hold-end --trace --perf-frame-reuse=off --perf-op-cache=off
./build_ci_cc/vn_player --scene S0 --frames 32 --hold-end --trace --perf-frame-reuse=off --perf-op-cache=off --perf-dirty-tile=on
./build_ci_cc/vn_player --backend scalar --scene S3 --resolution 1200x1600 --frames 128 --dt-ms 16 --hold-end --trace --perf-dynamic-resolution=on
./tests/perf/run_perf_compare.sh --baseline avx2 --baseline-label avx2_dirty_off --baseline-perf-dirty-tile off --candidate avx2 --candidate-label avx2_dirty_on --candidate-perf-dirty-tile on --scenes S0,S1,S2,S3 --duration-sec 120 --warmup-sec 20 --dt-ms 16 --resolution 600x800 --repeat 3 --out-dir /tmp/n64gal_perf_dirty_compare
./tests/perf/run_perf_compare.sh --baseline scalar --baseline-label scalar_dynres_off --baseline-perf-dynamic-resolution off --candidate scalar --candidate-label scalar_dynres_on --candidate-perf-dynamic-resolution on --scenes S3 --duration-sec 6 --warmup-sec 1 --dt-ms 16 --resolution 1200x1600 --out-dir /tmp/n64gal_perf_dynres_compare
./scripts/ci/run_perf_smoke_suite.sh --out-dir /tmp/n64gal_perf_ci
```

推荐读法：

1. 默认开启两层优化，先看整机 `p95_frame_ms` 是否下降。
2. 关闭 `frame reuse` 后，再看 `raster_ms` 是否回升，用于估算静态帧短路的收益。
3. 继续关闭 `op cache` 后，再看 `build_ms` 是否回升，用于估算命令缓存的收益。
4. 单独开启 `dirty tile` 后，先看 `dirty_tiles/dirty_rects/dirty_full_redraw` 是否符合预期；这些字段既代表 planner 质量，也开始反映 dirty submit 命中情况；目前 `scalar`、`avx2`、`neon`、`rvv` 都已实现 partial submit。
5. 单独开启 `dynamic resolution` 后，优先看 `p95_frame_ms` 是否回落，再配合 trace 中的 `render_width/render_height/dynres_tier/dynres_switches` 判断是否真的切到了更低档位。

## Baseline vs Candidate Backend

完整 sweep 示例：

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

当前 `linux-x64` CI smoke gate 示例：

```bash
./tests/perf/run_perf_compare.sh \
  --baseline scalar \
  --candidate avx2 \
  --scenes S1,S3 \
  --duration-sec 2 \
  --warmup-sec 1 \
  --dt-ms 16 \
  --resolution 600x800 \
  --threshold-file tests/perf/perf_thresholds.csv \
  --threshold-profile linux-x64-scalar-avx2-smoke \
  --out-dir /tmp/n64gal_perf/compare_scalar_avx2_smoke
```

输出目录结构：

1. `scalar/perf_summary.csv`
2. `avx2/perf_summary.csv`
3. `compare/perf_compare.csv`
4. `compare/perf_compare.md`
5. `compare/perf_threshold_metrics.csv` / `compare/perf_threshold_results.csv` / `compare/perf_threshold_report.md`（启用门限 profile 时）

`perf_compare.md` 会给出每个 scene 的 `p95/avg/max_rss` 对比，以及 speedup / gain 百分比。
`perf_threshold_report.md` 会把 profile 中的每条门限检查展开成表格，适合直接进 CI artifact 或 issue 证据链。

## Perf Threshold Gate

`tests/perf/perf_thresholds.csv` 用一份 CSV 定义不同 runner/profile 的性能门限；`tests/perf/check_perf_thresholds.sh` 会把 `perf_compare.csv` 转成聚合指标后执行门限判定。

当前已落地的 profile：

1. `linux-x64-scalar-avx2-smoke`：GitHub `ubuntu-latest` 上的 `scalar -> avx2` smoke hard gate。
2. `linux-arm64-scalar-neon-smoke`：GitHub `ubuntu-24.04-arm` 上的 `scalar -> neon` smoke hard gate。
3. `windows-x64-scalar-avx2-smoke`：GitHub `windows-latest` 上的 `scalar -> avx2` 正收益 gate；在连续两次 Windows x64 原生成功 run 都转正后，门限已收紧为 non-negative gain。
4. `windows-arm64-scalar-neon-smoke`：GitHub `windows-11-arm` 上的 `scalar -> neon` smoke hard gate。
5. `linux-riscv64-qemu-rvv-rev-smoke`：`qemu-riscv64` 下的 RVV revision compare 门限，现已接到 `.github/workflows/riscv-perf-report.yml`，默认以 `soft` 模式产出报告但不直接打红 workflow。

直接对已有 compare 结果做门限检查：

```bash
./tests/perf/check_perf_thresholds.sh \
  --compare-csv /tmp/n64gal_perf/compare_scalar_avx2/compare/perf_compare.csv \
  --threshold-file tests/perf/perf_thresholds.csv \
  --profile linux-x64-scalar-avx2-smoke
```

如果直接走 `run_perf_compare.sh` / `run_perf_compare_revs.sh`，可以用 `--threshold-file` + `--threshold-profile` 让 compare 与 gate 一次完成；`run_perf_compare_revs.sh` 还支持 `--threshold-soft-fail`，适合先在噪声较大的 runner 上保留报告、不立即转阻塞。`run_perf_compare.sh` 现在额外支持 `--repeat N`，会把重复采样的 `p95_frame_ms` / `avg_frame_ms` / `max_rss_mb` 按 scene 做中位数聚合后再生成 compare artifact，适合 dirty-tile 这类短窗口抖动更明显的 on/off 对照。

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
  --threshold-file tests/perf/perf_thresholds.csv \
  --threshold-profile linux-riscv64-qemu-rvv-rev-smoke \
  --threshold-soft-fail \
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
  --threshold-file tests/perf/perf_thresholds.csv \
  --threshold-profile linux-riscv64-qemu-rvv-rev-smoke \
  --threshold-soft-fail \
  --out-dir /tmp/n64gal_perf_rvv_compare
```

输出目录结构：

1. `baseline/perf_summary.csv`
2. `candidate/perf_summary.csv`
3. `compare/perf_compare.csv`
4. `compare/perf_compare.md`
5. `compare/perf_compare_revs.md`

其中 `perf_compare_revs.md` 会额外记录 host、compiler、runner prefix 与 revision 元数据，适合直接入库作为 issue 证据链。

当前 `linux-x64` 主线 smoke 已切到 `S1,S3`，因为 `S0` 在 shipped 路径上会被 frame reuse 压到约 `0.001ms`。但 `qemu-rvv` revision compare 仍暂时保留 `S0,S3` 作为 bring-up smoke：它主要服务于跨 revision 趋势留痕与 RVV 路径冒烟，和 native x64 主线 gate 的噪声结构并不相同；待 native RVV 设备到位后，再统一评估是否也切到更重场景。

## Checked-In Evidence

当前仓库已经固化四份 perf 证据：

1. [`docs/perf-rvv-2026-03-06.md`](./perf-rvv-2026-03-06.md)
2. [`docs/perf-dirty-2026-03-07.md`](./perf-dirty-2026-03-07.md)
3. [`docs/perf-dynres-2026-03-07.md`](./perf-dynres-2026-03-07.md)
4. [`docs/perf-windows-x64-2026-03-07.md`](./perf-windows-x64-2026-03-07.md)

其中：

1. `perf-rvv-2026-03-06.md` 对应 `75ee8f9 -> ee42c39` 的 `rvv` 对比，主要用于证明融合优化系列在 `qemu-user` 环境下可以稳定得到正收益。它不是发布级原生基准，不能替代 riscv64 真机 perf。
2. `perf-dirty-2026-03-07.md` 记录了 dirty runtime fast-path、full-redraw shallow commit、以及 partial 路径增量 tile 计数落地后的 `avx2 dirty off -> on` 本地 repeat-median compare；当前 compare 已从 `S0,S3` 调整到 `S1,S3`，因为 `S0` 在 shipped 路径上会被 frame reuse 压到约 `0.001ms`。它用于保留一个中等复杂度样本加一个更重样本的趋势证据，同样不是发布级基准，也不替代 GitHub runner 或目标机采样。
3. `perf-dynres-2026-03-07.md` 记录了 `97cc92a` 上 `scalar dynres off -> on` 的本地 smoke 结果，用于证明动态分辨率 runtime slice 已能在真实 runtime 路径上形成可观测整机收益。它同样不是发布级基准，也不替代 GitHub runner 或目标机采样。
4. `perf-windows-x64-2026-03-07.md` 专门记录 GitHub `windows-x64` runner 上 `scalar -> avx2` 仍为负收益时的调查结果：问题集中在 `raster_ms`，主要根因是 AVX2 覆盖面仍过窄，特别是整屏 `FADE` 在修正前仍走公共标量 `blend_rgb()`。该文档同时记录了当前已入树的第一轮修正（`uniform alpha/fade` AVX2 row kernel + 对齐后的 `fill_u32`），用于后续和 GitHub Windows runner 的复测结果对照。

另外，CI 已新增 `.github/workflows/riscv-perf-report.yml` 与 `scripts/ci/run_riscv64_qemu_perf_report.sh` 包装入口，供 `workflow_dispatch` / nightly 的 `linux-riscv64-qemu-rvv-perf-report` job 直接产出同格式 artifact。该 workflow 默认使用比本地 smoke 更长的 `4s/2s` 窗口，以降低 qemu-user 短窗口抖动；当前默认还会接入 `linux-riscv64-qemu-rvv-rev-smoke` 的 `soft` threshold mode，并把 `perf_threshold_report.md` 一并追加到 step summary / artifact。首次 GitHub `workflow_dispatch` run `22766736383` 已验证 `Generate QEMU RVV perf report -> Publish perf summary -> Upload perf artifact` 全链成功。

## CI Artifact

当前原生目标平台都通过参数化后的 `scripts/ci/run_perf_smoke_suite.sh` 生成 perf artifact：

1. `linux-x64 -> perf-linux-x64`：`scalar -> avx2`、`avx2 dirty off -> on`、`scalar dynamic-resolution off -> on`、`kernel scalar -> avx2`；`Compare A` 当前附 `linux-x64-scalar-avx2-smoke` threshold report，并作为 native perf hard gate。
2. `linux-arm64 -> perf-linux-arm64`：`scalar -> neon`、`neon dirty off -> on`、`scalar dynamic-resolution off -> on`、`kernel scalar -> neon`；`Compare A` 当前附 `linux-arm64-scalar-neon-smoke` threshold report，并作为 native perf hard gate。
3. `windows-x64 -> perf-windows-x64`：`scalar -> avx2`、`avx2 dirty off -> on`、`scalar dynamic-resolution off -> on`、`kernel scalar -> avx2`；`Compare A` 当前附 `windows-x64-scalar-avx2-smoke` threshold report，并已升级为正收益 gate。
4. `windows-arm64 -> perf-windows-arm64`：`scalar -> neon`、`neon dirty off -> on`、`scalar dynamic-resolution off -> on`、`kernel scalar -> neon`；`Compare A` 当前附 `windows-arm64-scalar-neon-smoke` threshold report，并作为 native perf hard gate。

`windows-x64` 的近况与专项分析见 [`docs/perf-windows-x64-2026-03-07.md`](./perf-windows-x64-2026-03-07.md)。当前仓库已先把 AVX2 的 `uniform alpha/fade` row kernel 补进后端，并把 `fill_u32` 改成对齐前缀 + aligned store；随后又把 `SPRITE/TEXT` 的 `sample/hash -> combine -> blend` 热循环收回 AVX2 TU，并继续补了一轮 textured row palette + repeated-`v8` reuse，避免 full-span 路径重复计算同一批 texel。GitHub `windows-x64` 连续两次成功 run `22795078202`（head `d6081b4`）与 `22795471059`（head `19788ce`，均为 2026-03-07）都已经显示 `scalar -> avx2` 的强正收益；因此 `windows-x64-scalar-avx2-smoke` 现已从 regression-envelope gate 升级为正收益 gate。

## Kernel Bench

为了解耦整机 smoke 和后端热路径，本仓库已新增 `tests/perf/backend_kernel_bench.c`、`tests/perf/run_kernel_bench.sh` 与 `tests/perf/run_kernel_compare.sh`。这套工具直接通过 `renderer_submit()` 强制跑单 op kernel，用于拆开观察 `CLEAR`、`FADE`、scene-sized textured op、以及 full-span textured op 的成本；backend 参数现支持 `scalar`、`avx2`、`neon`、`rvv` 与 `auto`（以当前 host/runner 实际可用 ISA 为准）。

常用命令：

```bash
./tests/perf/run_kernel_bench.sh --backend scalar --iterations 24 --warmup 6 --out-csv /tmp/n64gal_kernel_scalar.csv
./tests/perf/run_kernel_bench.sh --backend avx2 --iterations 24 --warmup 6 --out-csv /tmp/n64gal_kernel_avx2.csv
./tests/perf/run_kernel_compare.sh --baseline scalar --candidate avx2 --resolution 600x800 --iterations 24 --warmup 6 --out-dir /tmp/n64gal_kernel_compare
```

当前本地 `linux-x64` 样本显示：

1. `clear_full`: `2.072ms -> 0.662ms`
2. `fade_full_alpha160`: `5.215ms -> 3.791ms`
3. `sprite_full_opaque`: `25.306ms -> 19.173ms`
4. `sprite_full_alpha180`: `30.249ms -> 25.215ms`

这组数据说明当前 AVX2 的 `clear/fade` 已明显转正；如果 `windows-x64` runner 后续仍不转正，下一阶段主热点应继续集中排查 textured full-span 路径，而不是再次回头优化 clear/fade。

其中 `linux-x64` 的 smoke / dirty compare 已固定为 `S1,S3 @ 600x800`，`dirty` compare 额外使用 `6s/1s + repeat=3` 中位数聚合；`S0` 只保留在全量 sweep 与 `qemu-rvv` bring-up smoke，因为它在 shipped 主路径上会被 frame reuse 压到约 `0.001ms`。arm64 与 Windows 当前沿用同样的 smoke 场景与 dynres 场景，只把 SIMD candidate 切到各自平台优先 ISA。

artifact 顶层会额外生成 `perf_workflow_summary.md`，把四组 compare report 收口成一份 step summary 友好的 markdown。目录结构按 candidate backend 动态命名，常见形式包括：

1. `scalar_vs_avx2/compare/perf_compare.md` 或 `scalar_vs_neon/compare/perf_compare.md`
2. `scalar_vs_avx2/compare/perf_threshold_report.md`（仅在启用 threshold profile 时存在）
3. `avx2_dirty_tile/compare/perf_compare.md` 或 `neon_dirty_tile/compare/perf_compare.md`
4. `scalar_dynamic_resolution/compare/perf_compare.md`
5. `kernel_scalar_vs_avx2/compare/kernel_compare.md` 或 `kernel_scalar_vs_neon/compare/kernel_compare.md`
6. `perf_workflow_summary.md`

当前 CI 目标是快速回归与报告留档，不替代长时间本地压测。现阶段项目按 `qemu-first` 收口：先固化 `x64/arm64 native artifact + cross/qemu/golden/perf artifact`，原生 `native-riscv64` 设备到位前不把 nightly perf 当作日常阻塞。需要发布级结论时，仍应运行完整 `120s/20s` 窗口，并优先在原生目标机上采样。

## Reading Rules

1. 优先看 `p95_frame_ms`，其次看 `avg_frame_ms`。
2. `gain > 0` 代表 candidate 更快。
3. `rss_delta_mb < 0` 代表 candidate 峰值 RSS 更低。
4. 若 trace 中 `frame_reuse_hit=1`，则该帧已经跳过 build/raster；这属于整机收益，不应与“纯 backend 像素吞吐”混为一谈。
5. 若 trace 中 `op_cache_hit=1`，则该帧只跳过了命令构建；仍需结合 `raster_ms` 判断 backend 热点是否变化。
6. 所有性能结论都必须附带：commit、设备、OS、toolchain、命令行参数。
7. `qemu-user` perf 只用于回归趋势判断；发版门槛必须看目标架构原生机器结果。
8. `dirty_full_redraw=1` 说明 planner 主动回退整帧路径；即使 dirty submit 已接入，出现该值时也应按整帧提交理解。
9. CI 门限 profile 先按当前 runner 的 smoke 输入固化，后续随着优化落地再逐步收紧，不把“还未优化完”的目标值直接硬塞进日常短窗口 gate。
