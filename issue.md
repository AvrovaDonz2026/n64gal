# N64GAL Issue Backlog (Refined)

基于 [`dream.md`](./dream.md) `v1.6-executable-c89-single-api` 整理。  
目标：将白皮书转为可直接执行的 GitHub issue 工单体系。

## 0.1 落地状态快照（2026-03-07）

### 已完成（代码已合入 main）

1. `ISSUE-001`：前后端单一 API 基本冻结（`vn_backend.h` + 注册/选择链）
2. `ISSUE-002`：Frontend 输出 `VNRenderOp[]`，且 VM -> IR 主路径已接通
3. `ISSUE-003`：`scalar` 最小可运行闭环（`600x800`、基础场景 `S0-S3`）
4. `ISSUE-004`：`vnpak v2`（CRC32）+ `manifest.json` + 资源一致性校验 + `PNG -> RGBA16/CI8/IA8` 已落地
5. `ISSUE-005`：`S0-S3+S10` perf CSV + 热身窗口 + p95 汇总 + revision-compare 工具链/入库报告已落地
6. `ISSUE-006`（部分）：C89 门禁脚本 + 头文件独立编译检查
7. `ISSUE-016`：Runtime Session API（`create/step/destroy` + input injection）已落地
8. `ISSUE-017`（部分）：`docs/api/*` 文档集与维护入口已建立
9. `ISSUE-014`（部分）：`riscv-perf-report` workflow 与 wrapper 已合入，且 `ci-matrix` 在 `f5eaa54` / `1792d6e` 两次 push 均为 `success`

### 近期追踪（2026-03-07）

1. `df68232`：落地 revision-compare perf 工具、qemu RVV smoke 报告与 perf 工作流文档。
2. `f5eaa54`：新增 `.github/workflows/riscv-perf-report.yml` 与 `scripts/ci/run_riscv64_qemu_perf_report.sh`。
3. `1792d6e`：修复 `riscv-perf-report` workflow 解析错误（`runner.temp -> github.workspace`）。
4. GitHub Actions：`ci-matrix` 在 `1792d6e` 对应 push 已 `success`；`riscv-perf-report` 的首次 `workflow_dispatch` 冒烟（run `22766736383`）已于 `2026-03-06 22:17 HKT` 完成并 `success`，artifact 与 step summary 均已产出。
5. 当前约束：手头暂无 `native-riscv64/RVV` 设备，`ISSUE-020` 保留但暂列外部阻塞项；现阶段优先收口 `qemu` 证据链、golden 阈值与性能门限。
6. `ISSUE-008` 第一阶段已落地：新增 `tests/perf/perf_thresholds.csv`、`tests/perf/check_perf_thresholds.sh`，并把 `linux-x64` 的 `scalar -> avx2` smoke compare 接成阻塞门限。
7. `ISSUE-014` 已补 Linux suite artifact：`run_cc_suite.sh` / `run_riscv64_qemu_suite.sh` 会生成 `ci_logs/`、`ci_suite_summary.md` 与 `golden_artifacts/`，`ci-matrix` 已上传 `suite-linux-x64`、`suite-linux-arm64`、`suite-linux-riscv64-qemu-scalar`、`suite-linux-riscv64-qemu-rvv` artifact。
8. `ISSUE-014` 本轮继续补 Windows suite artifact：`windows-x64` / `windows-arm64` 已接入 `suite-windows-x64` / `suite-windows-arm64` 上传，并额外复跑 `test_renderer_fallback`、`test_runtime_api`、`test_runtime_golden` 生成回退/ golden 证据；当前统一走 `scripts/ci/run_windows_suite.ps1`。
9. `ISSUE-014` 已新增 `scripts/ci/run_windows_suite.ps1`，把 Windows 的 `configure -> build -> ctest -> fallback/golden evidence -> suite summary` 收口到单一 PowerShell 入口，供 GitHub Actions 与本地复跑共用；即使 `ctest` 失败也会继续尝试写出 summary 与可用日志。
10. GitHub Actions：`ci-matrix` push run `22772138491`（head `8e5dcd8`）已于 `2026-03-07 00:26 HKT` 完成并 `success`；其中 `windows-x64` 与 `windows-arm64` job 均为 `success`，Windows suite artifact 链已完成实跑复核。
11. `ISSUE-008` 本轮继续补 `qemu-rvv` revision compare gate：`run_perf_compare_revs.sh` 已支持 `--threshold-soft-fail`，`scripts/ci/run_riscv64_qemu_perf_report.sh` / `.github/workflows/riscv-perf-report.yml` 已接入 `linux-riscv64-qemu-rvv-rev-smoke` profile，当前默认以 `soft` 模式产出 threshold report 并保留观察噪声。
12. `ISSUE-008` 已把 Runtime `VN_RUNTIME_PERF_FRAME_REUSE` 静态帧短路与 `VNRenderOp[]` LRU 命令缓存接入主线：`VNRunConfig.perf_flags` / `VNRunResult.perf_flags_effective|frame_reuse_hits|frame_reuse_misses|op_cache_hits|op_cache_misses` 已公开，CLI 已支持 `--perf-frame-reuse=<on|off>` 与 `--perf-op-cache=<on|off>`，前者会在稳定状态下直接复用 framebuffer，后者命中后仍按当前帧回写 `SPRITE/FADE` 动态字段，`test_runtime_api` 已覆盖两条路径与关闭路径。默认主线现已具备“整帧复用 -> 命令缓存 -> 正常构建/光栅”的分层回退链。
13. `ci-matrix` push run `22775198899`（head `1e997d5`）暴露出两处 Windows 共性问题：`src/core/runtime_cli.c` 的 `t_after_raster` 在 MSVC `/W4 /WX` 下触发 `C4701`，以及 `scripts/ci/run_windows_suite.ps1` 在 `native-command | Tee-Object` 后错误读取退出码，导致 build 失败后仍继续跑 `ctest`。两处已在主线修复，并已由后续 push run `22775799001`（head `6b60ec6`）复核 `windows-x64/windows-arm64` 全部 `success`。
14. `ISSUE-008` 已补 `Dirty-Tile` 设计/API 草案：新增 `docs/api/dirty-tile-draft.md`，明确当前最佳挂接点应位于 `runtime_build_render_ops_cached(...)` 与 `runtime_render_patch_cached_ops(...)` 之后、`renderer_submit(...)` 之前；公共接口草案采用 `renderer_submit_dirty(...)` / `submit_ops_dirty(...)` + runtime perf flag/stats 的最小增量，不改 `VNRenderOp` 语义。
15. `ISSUE-008` 第一段代码已落地：新增 `src/frontend/dirty_tiles.c/.h` 内部 planner、`VN_RUNTIME_PERF_DIRTY_TILE`、`VNRunResult` dirty 统计字段、preview JSON 回显、`test_dirty_tiles` 与 `test_runtime_api` 覆盖，以及 CLI `--perf-dirty-tile=<on|off>`。
16. `ISSUE-008` 第二段代码已接上：`vn_backend.h` / `vn_renderer.h` 新增统一 dirty submit 契约，Runtime 已在 planner 后优先尝试 `renderer_submit_dirty(...)`；`scalar` 后端已实现 clip submit 基线，随后 `avx2`、`neon`、`rvv` 也已补齐 dirty submit；其中 `rvv` 额外补了一条 `qemu-rvv` 下的 `test_renderer_dirty_submit_rvv` smoke 验证，并与现有 `test_backend_consistency_rvv` 一起收口。
17. Perf 脚本已补运行时开关入口：`tests/perf/run_perf.sh` / `run_perf_compare.sh` 现在可直接传 `--perf-frame-reuse` / `--perf-op-cache` / `--perf-dirty-tile`，并支持同一 backend 的 label 化 on/off compare。
18. Perf 工具链已开始自动记录 `host_cpu`：`perf_summary.csv`、`kernel_bench.csv`、compare markdown 与 `perf_workflow_summary.md` 都会回显当前 runner CPU 型号，便于直接排查 Linux/Windows runner 差异。
19. `ISSUE-008` / `ISSUE-014` 本轮继续补齐 CI 固化：参数化后的 `scripts/ci/run_perf_smoke_suite.sh` 现已把 native 平台 perf 统一收口到同一套 smoke compare artifact；`linux-x64` 产出 `perf-linux-x64` 并继续保留 `scalar -> avx2` hard gate，`linux-arm64` 新增 `perf-linux-arm64`（`scalar -> neon` / `neon dirty off -> on` / `scalar dynres off -> on`）并挂 `linux-arm64-scalar-neon-smoke`，`windows-x64` 新增 `perf-windows-x64` 并已把 `windows-x64-scalar-avx2-smoke` 升级成正收益 gate，`windows-arm64` 新增 `perf-windows-arm64` 并挂 `windows-arm64-scalar-neon-smoke`。同时 `linux-arm64` / `windows-arm64` 仍显式校验 `test_renderer_dirty_submit` 中的 `matched backend=neon`，确保 GitHub runner 上实际覆盖 `neon` dirty submit。
20. `ISSUE-008` 已追加一份入库 perf 证据：`docs/perf-dynres-2026-03-07.md`，记录 `97cc92a` 上 `scalar + S3 + 1200x1600` 的 `dynamic-resolution off -> on` smoke 结果，当前本地样本显示 `p95` 下降约 `9.99%`，用于证明 dynres runtime slice 已在真实 runtime 路径上生效。
21. `ISSUE-008` 已补一条 dirty runtime fast-path：对 `FADE`、op 布局变化、clear 变化等“已知必整帧”的场景，runtime 现在直接标记 `full_redraw` 并跳过 tile 构建/rect 收集，随后又补了 full-redraw shallow commit 与 partial 路径的增量 dirty-tile 计数；同时 `linux-x64` 的 dirty smoke compare 已从单次 `6s/1s` 升级到 `repeat=3` 中位数汇总，并已把场景从 `S0,S3` 调整到 `S1,S3,S10`，因为 `S0` 在主线路径上会被 frame reuse 压到约 `0.001ms`，而 `S10` 作为更重的 perf sample 用于补足压力场景。当前本地 `avx2 dirty off -> on` 聚合样本里 `S1 p95` 为 `15.228ms -> 13.507ms`（`+11.30%`），`S3` 仍有 `-3.59%` 噪声，因此该 compare 继续只作为趋势 artifact。
22. `ISSUE-008` 的 `qemu-rvv` revision compare 目前仍保留 `S0,S3` 作为 bring-up smoke，用于跨 revision 趋势留痕与 RVV 路径冒烟；它不再作为 x64 主线 gate 的代表样本，待 native RVV 设备到位后再统一评估是否也切到更重场景。
23. `bfe95d2`：继续同步 perf 文档与跟踪项，明确 `linux-x64` 的 smoke / dirty compare 基线现在统一为 `S1,S3,S10`，而 `qemu-rvv` revision compare 仍暂保留 `S0,S3`；同时 README、平台/性能文档与 dirty-tile API 文档都按这一边界更新。
24. `ISSUE-008` / `ISSUE-014` 继续把 perf 落到所有原生目标平台：`run_perf.sh` / `run_perf_compare.sh` 已支持复用已有 `vn_player`（`--runner-bin` + `--skip-build`），参数化后的 `scripts/ci/run_perf_smoke_suite.sh` 现已可在 `linux-x64`、`linux-arm64`、`windows-x64`、`windows-arm64` 复用。
25. 三套 native threshold profile 已按 GitHub runner 实测落地：`linux-arm64-scalar-neon-smoke`、`windows-arm64-scalar-neon-smoke` 与 `windows-x64-scalar-avx2-smoke` 现在都采用正收益 gate。`windows-x64` 之所以可以收紧，是因为连续两次成功 run `22795078202`（head `d6081b4`）与 `22795471059`（head `19788ce`，均为 2026-03-07）里，`scalar -> avx2` 已分别达到 `S1 +83.65% / S3 +81.51%` 与 `S1 +88.06% / S3 +80.47%`；当前门限已从 regression-envelope 改成 non-negative gain。与之相对，`linux-arm64` 的 `scalar -> neon` 为 `+13.09% / +13.36%`，`windows-arm64` 的 `scalar -> neon` 为 `+10.85% / +4.45%`，同样维持正收益 gate。
26. `ISSUE-007` / `ISSUE-008` 已新增 `docs/perf-windows-x64-2026-03-07.md`，专门记录 GitHub `windows-x64` runner 上 `avx2 < scalar` 的历史证据、根因、修正动作以及后续恢复情况：在多轮修正之后，连续两次 GitHub run `22795078202`（head `d6081b4`）与 `22795471059`（head `19788ce`）都已转为强正收益；其中后者的 `Compare D` 已把 `sprite_full_opaque` 压到 `0.620379ms`、`sprite_full_alpha180` 压到 `0.763946ms`，说明这轮 palette-apply 向量化已经在 GitHub Windows x64 runner 上直接落地。
27. `ISSUE-008` 已新增 backend kernel benchmark 工具：`tests/perf/backend_kernel_bench.c` + `tests/perf/run_kernel_bench.sh`，用于把 `clear/fade/textured` 热点拆成独立 kernel。当前本地 `x64` 样本显示：`clear_full 2.072ms -> 0.662ms`、`fade_full_alpha160 5.215ms -> 3.791ms`、`sprite_full_opaque 25.306ms -> 19.173ms`、`sprite_full_alpha180 30.249ms -> 25.215ms`，说明 `clear/fade` 已明显转正，剩余主热点继续集中在 textured full-span 路径。
28. `ISSUE-008` 已把 `tests/perf/run_kernel_compare.sh` 与 `scripts/ci/run_perf_smoke_suite.sh` 接通，native perf workflow 现在会固定产出 `Compare D = kernel scalar -> simd` artifact；同时 `backend_kernel_bench` 已补齐 `neon` / `rvv` backend 选择，避免 arm64 / riscv64 路径在复用同一套 compare API 时再额外分叉。
29. `ISSUE-007` 本轮继续压 AVX2 textured full-span 热点：`src/backend/avx2/avx2_backend.c` 已在 `vis_w > 256` 时走 row palette，并对连续重复的 `v8` 行复用 256 项 texel palette；随后 `19788ce` 又把 row-palette apply 改成 AVX2 gather/store 与 gather/blend chunk。对应 GitHub `windows-x64` run `22795471059` 的 `Compare D` 已把 `sprite_full_opaque` 压到 `5.104921ms -> 0.620379ms`（`+87.85%`），`sprite_full_alpha180` 压到 `7.490000ms -> 0.763946ms`（`+89.80%`）；整机 smoke 也达到 `S1 3.324ms -> 0.397ms`（`+88.06%`）与 `S3 3.011ms -> 0.588ms`（`+80.47%`）。这一轮已经不再是“等待 artifact 复核”，而是 GitHub 原生 runner 证据已经到位。
30. `ISSUE-008` 已继续补全 repeat compare 诊断：`tests/perf/run_perf_compare.sh --repeat N` 现在除中位数聚合外，还会导出 baseline/candidate `perf_summary_repeats.csv`、`compare/perf_repeat_variability.csv`、`compare/perf_repeat_variability.md`，并把 `Repeat Variability` 章节并入 `compare/perf_compare.md`；这条线直接服务于下一轮 `windows-x64` dirty `S3` 抖动排查，避免把短窗口噪声误判成真实 regression。
31. `ISSUE-008` / `ISSUE-014` 已把 dirty repeat variability 再往 workflow summary 抬一层：`scripts/ci/run_perf_smoke_suite.sh` 现在会在 `perf_workflow_summary.md` 里额外生成 `Dirty-Tile Repeat Variability Digest`，直接汇总 `perf_repeat_variability.csv/.md` 的 scene 级 range；这会同时落到 `linux-x64/avx2`、`linux-arm64/neon`、`windows-arm64/neon` 三条原生 perf 线，便于并行判断 dirty on/off 是真实回退还是 short-window jitter。
32. `ISSUE-014` 已按最快口径对齐路径先修 `linux-x64` perf：`.github/workflows/ci-matrix.yml` 的 Linux perf step 现在显式注入 `VN_PERF_CFLAGS=-O2 -DNDEBUG`，让 `run_perf.sh` / `run_kernel_bench.sh` 的 auto-build runner 不再落在无优化构建；同时 `scripts/ci/run_perf_smoke_suite.sh` 会把 `Perf CFLAGS` / `Perf LDFLAGS` 直接写进 `perf_workflow_summary.md`，避免后续再把 build-path mismatch 误判成 Linux runner 本身慢。
33. `ISSUE-009` 本轮继续推进 `neon`：`src/backend/neon/neon_backend.c` 已补 uniform alpha/fade row kernel，并在 `vis_w > 256` 时接入宽行 `SPRITE/TEXT` row-palette 复用与 palette-row apply；本地 `aarch64-linux-gnu-gcc + qemu-aarch64` 已复核 `test_renderer_fallback`、`test_renderer_dirty_submit`、`test_runtime_golden` 全部通过。qemu kernel bench 仅作方向性留痕：`sprite_full_opaque` 约 `269.19ms -> 38.64ms`、`sprite_full_alpha180` 约 `328.57ms -> 144.20ms`，而 `fade` 在 qemu 下的 NEON 指令模拟仍明显偏慢，因此发布级 perf 仍以 GitHub 原生 arm64 runner artifact 为准。

### 平台目标（新增约束）

1. 目标平台必须覆盖 `x64` 与 `arm64`
2. 每个目标架构都必须支持 `Linux` 与 `Windows`
3. 追加目标：`riscv64 Linux`（`rvv` 优先，失败回退 `scalar`）
4. 平台适配优先级：`x64 Linux` -> `x64 Windows` -> `arm64 Linux` -> `arm64 Windows` -> `riscv64 Linux`
5. 长线补充：在 M3 后进入 `M4-engine-ecosystem`，补齐模板、Creator Toolchain、宿主 SDK 与预览协议。

### 进行中（当前主线）

1. Runtime API 化：`vn_runtime_run(config, result)` + Session API（`create/step/destroy`）已可用
2. 输入链路：Session 输入注入 API 已落地，CLI 键盘语义已在 Linux/Windows 收口到同一套 `1-9` / `t` / `q` 规则
3. 平台路径/文件 I/O：`src/core/platform.c` 已接入 pack 与 preview 路径解析，并进一步统一 `runtime/preview` 计时入口，`test_platform_paths` 已补齐
4. 时间/休眠：`vn_platform_now_ms()` + `vn_platform_sleep_ms()` 已落地，CLI `keyboard` 调试模式可按 `dt_ms` 节奏推进
5. 编译器差异：`src/core/build_config.h` 已集中收口 OS/Arch/Compiler 探测，`avx2_backend.c` 已补齐 GCC/Clang + MSVC x64 双路径探测与编译开关
6. `ISSUE-007`（进行中）：`avx2` 后端已从桩实现升级为可运行路径（`CLEAR/SPRITE/TEXT/FADE`），`test_runtime_golden` 已固化 `S0/S1/S2/S3/S10 @ 600x800` 标量 golden CRC；支持的 SIMD 后端按 `mismatch_percent < 1%` 且 `max_channel_diff <= 8` 判定，并在出现差异或 CRC 异常时导出 `expected/actual/diff` PPM + `summary.txt`
7. 文档化：`docs/api/README.md`、`docs/api/runtime.md`、`docs/api/backend.md`、`docs/api/pack.md`、`docs/platform-matrix.md` 已建立，后续随 API 变更持续维护
8. CI/perf 追踪：`riscv-perf-report` 的 `workflow_dispatch` 首次 GitHub 端冒烟（run `22766736383`）已完成，artifact 与 step summary 已验证可用；当前 workflow 已默认接入 `linux-riscv64-qemu-rvv-rev-smoke` 的 `soft` threshold report
9. `ISSUE-008`（进行中）：性能门限文件、门限校验脚本、`linux-x64` compare gate、`qemu-rvv` revision compare soft gate，以及 Runtime `VN_RUNTIME_PERF_FRAME_REUSE`、`VNRenderOp[]` LRU 命令缓存、`Dirty-Tile` 与动态分辨率 runtime slice 均已落地；`run_perf_compare.sh --repeat N` 也已补齐 repeat variability artifact，`run_perf_smoke_suite.sh` 则继续把这组信息抬进 `perf_workflow_summary.md`，用于把 dirty/dynres 的短窗口 jitter 与真实 regression 拆开。当前剩余 P0 焦点转为 dirty regression 分析、dynres 默认值/阈值校准，以及更多热点收益固化。
10. `ISSUE-014`（进行中）：Linux x64 / Linux arm64 / Linux riscv64 qemu suite 日志与 golden artifact 归档链已落地；Windows x64 / Windows arm64 的 suite artifact 已统一收口到 `scripts/ci/run_windows_suite.ps1`，且已在 GitHub Actions push run `22772138491` 上完成实跑复核

### 下一步（短周期）

1. `ISSUE-014` 跟进：补齐 merge gate/branch protection 说明，并把 Windows suite 实跑 run `22772138491` 的 artifact 约定固化到文档。
2. `ISSUE-010` 前置准备：后端一致性基线数据沉淀（scalar 对照）与 golden/perf 证据对齐。
3. `ISSUE-011` 细化：把 `riscv64` 验证链拆成 `cross-build -> qemu-scalar -> qemu-rvv -> native`，其中 `native` 因设备缺失暂列 blocked。
4. `ISSUE-008` 第三阶段：在已有 perf threshold gate、state-hash、op cache、Dirty-Tile 与 dynres runtime slice 之上，继续做 dirty regression 分析、dynres 默认值/阈值校准，以及热点收益固化。
5. `ISSUE-008` 跟进：观察 `linux-riscv64-qemu-rvv-rev-smoke` 在 GitHub runner 上的波动，再决定是否从 `soft` 升级到 `hard`。
6. `M4-engine-ecosystem` 预研：先冻结模板/CLI/宿主 SDK/预览协议边界，避免工具链各自长歪。

## 0. 适用原则

1. 单一前后端 API：`vn_backend.h` 是唯一跨架构契约。
2. 跨架构迁移：只新增后端实现并重编译，前端源码零改动。
3. C89 强约束：运行时代码禁止 C99/C11 特性。
4. 所有性能优化都必须可开关、可回退、可对照验证。
5. 性能优先：在不破坏正确性和可维护性的前提下，尽可能提升更多性能（优先优化热路径）。

## 1. Milestone 规划

| Milestone | 周期 | 目标 | 当前判定 |
|---|---|---|---|
| `M0-core-scalar` | W1-W2 | 前后端分离骨架 + scalar 基线 + Session API/pack/perf 基础链 | 已完成，转维护态 |
| `M1-amd64-avx2` | W3-W6 | amd64 AVX2 首发性能 + 平台抽象收口（Linux/Windows） | 基本完成，剩 `ISSUE-007/008` 收口项 |
| `M2-arm64-neon` | W7-W10 | arm64 NEON 平台化 + 一致性测试 + 跨 OS CI | 基本完成，剩 `ISSUE-010/014` 尾项 |
| `M3-riscv64-rvv` | W11-W14 | riscv64 RVV 扩展 + qemu 阻塞链 + 原生 nightly/perf | 当前主线，先收口 `qemu/golden/perf`；`native` 因硬件缺失暂缓 |
| `M4-engine-ecosystem` | W15-W20 | 模板、工具链、宿主 SDK、预览协议、迁移器与生态治理 | 已前置部分文档/工具，禁止抢占 M3 阻塞资源 |

### 1.1 阶段切换规则

1. `M0` 已完成，除兼容性修复和文档补档外，不再新增基础骨架类需求。
2. `M1/M2` 当前按“维护尾项”处理，允许继续收口 golden 阈值、性能门限和 fallback 证据，但优先级低于 `M3` 发版阻塞。
3. `M3` 仍是当前主线里程碑，但在缺少原生设备时，先把可控部分收口到 `cross-build -> qemu-scalar -> qemu-rvv -> qemu perf artifact + golden/perf 门限`；`native-nightly` 作为外部依赖保留。
4. `M4` 允许继续做不打断主线的文档/协议/工具前置，但不得挤占 `qemu` 证据链收口、golden/perf 门限和平台稳定性修复的资源。

### 1.2 平台兼容矩阵（必须覆盖）

| 平台 | 后端优先级 | 当前状态 |
|---|---|---|
| Linux x64 | `avx2` -> `scalar` | 已完成（CI 全绿，perf smoke 已接入） |
| Windows x64 | `avx2` -> `scalar` | 已完成（MSVC x64 CI 全绿） |
| Linux arm64 | `neon` -> `scalar` | 已完成（arm64 Linux CI 全绿） |
| Windows arm64 | `neon` -> `scalar` | 已完成（Windows arm64 CI 全绿） |
| Linux riscv64 | `rvv` -> `scalar` | 进行中（`cross/qemu-scalar/qemu-rvv/qemu perf artifact` 已打通，待 `native-nightly` 与原生 perf 证据） |

## 2. 标签建议

- `type:epic`
- `type:feature`
- `type:perf`
- `type:infra`
- `type:test`
- `type:docs`
- `arch:scalar`
- `arch:avx2`
- `arch:neon`
- `arch:rvv`
- `priority:P0`
- `priority:P1`
- `priority:P2`
- `blocked`

## 3. 依赖拓扑

```text
ISSUE-001 -> ISSUE-002 -> ISSUE-003 -> ISSUE-007 -> ISSUE-008 -> ISSUE-012
      |          |            |
      |          |            -> ISSUE-005 -> ISSUE-007
      |          |
      -> ISSUE-006

ISSUE-007 + ISSUE-008 -> ISSUE-009 -> ISSUE-010 -> ISSUE-011 -> ISSUE-012
      |
      -> ISSUE-013 -> ISSUE-014 -> ISSUE-012

ISSUE-012 -> ISSUE-021 -> ISSUE-022 -> ISSUE-023
ISSUE-001 + ISSUE-016 + ISSUE-017 -> ISSUE-024
ISSUE-011 + ISSUE-014 -> ISSUE-020 -> ISSUE-012
ISSUE-004 + ISSUE-012 -> ISSUE-015 -> ISSUE-025
```

## 4. Issue 摘要

| ID | 标题 | Milestone | Priority | 估时 |
|---|---|---|---|---|
| ISSUE-001 | 冻结单一前后端 API（`vn_backend.h`） | M0 | P0 | 2d |
| ISSUE-002 | Frontend 输出 `VNRenderOp[]` | M0 | P0 | 2d |
| ISSUE-003 | `scalar` 后端最小可运行（600x800 + S0） | M0 | P0 | 3d |
| ISSUE-004 | 资源链路最小可用（`vnpak`+脚本编译） | M0 | P0 | 2d |
| ISSUE-005 | 基准场景 S0-S3 + S10 与 perf 采样 | M0 | P0 | 2d |
| ISSUE-006 | C89 门禁与头文件独立编译检查 | M0 | P0 | 1d |
| ISSUE-007 | `avx2` 后端实现与运行时切换 | M1 | P0 | 4d |
| ISSUE-008 | P0 性能项四件套 | M1 | P0 | 4d |
| ISSUE-009 | `neon` 后端实现与 arm64 默认启用 | M2 | P1 | 4d |
| ISSUE-010 | 后端一致性测试与 CI 矩阵 | M2 | P1 | 3d |
| ISSUE-011 | `rvv` 后端实现与回退链 | M3 | P2 | 5d |
| ISSUE-012 | 发布收口（文档/报告/迁移指南） | M3 | P1 | 2d |
| ISSUE-013 | Linux/Windows 平台抽象层与系统适配 | M1 | P0 | 3d |
| ISSUE-014 | x64/arm64 + Linux/Windows + riscv64 Linux CI 矩阵 | M2 | P0 | 4d |
| ISSUE-015 | `vnsave v1` 存档迁移器 | M4 | P2 | 3d |
| ISSUE-016 | Runtime Session API 与宿主循环对接 | M0 | P0 | 2d |
| ISSUE-017 | API 文档集维护规范 | M0 | P1 | 2d |
| ISSUE-018 | 错误码与日志可观测性升级 | M4 | P2 | 3d |
| ISSUE-019 | WebAssembly 实验性后端 | M4 | P2 | 4d |
| ISSUE-020 | riscv64 原生 runner / 开发板接入与 nightly perf 采样 | M3 | P1 | 4d |
| ISSUE-021 | 样例工程与模板仓骨架 | M4 | P1 | 2d |
| ISSUE-022 | Creator Toolchain 聚合（validate/migrate/probe） | M4 | P1 | 3d |
| ISSUE-023 | 预览协议与无 GUI 预览进程 | M4 | P2 | 4d |
| ISSUE-024 | 宿主 SDK 与版本协商文档 | M4 | P1 | 3d |
| ISSUE-025 | 扩展清单、兼容矩阵与生态治理 | M4 | P2 | 3d |

---

## 5. 滚动执行看板（当前阶段）

### 5.1 当前主线（按优先级执行）

1. `ISSUE-014`：补齐 fallback 验证日志、merge gate 说明与平台矩阵结果归档，减少“CI 绿但证据链不完整”的灰区。
2. `ISSUE-010`：把 golden 容差、差异摘要和后端一致性基线继续固化成长期门禁。
3. `ISSUE-011`：继续收口 RVV 的 qemu 侧一致性、perf 证据和可回退路径，不把 native 设备缺口混进日常开发阻塞。
4. `ISSUE-008`：在已落地 perf threshold gate、state-hash / frame reuse、op cache、Dirty-Tile 与 dynres runtime slice 的基础上，继续推进 regression 分析、默认值策略与热点收益固化。
5. `ISSUE-020`：保留为外部前置项；等有 `native-riscv64/RVV` 设备或 runner 资源后再恢复到主线最高优先级。

### 5.2 当前并行分配（建议）

| 角色 | 领取 Issue | 目标产物 |
|---|---|---|
| Owner-A（Runtime/QA） | `ISSUE-010` | golden 容差门禁、差异摘要、后端一致性基线 |
| Owner-B（SIMD/Perf） | `ISSUE-008` + `ISSUE-011` | perf threshold gate、RVV/AVX2/NEON 热路径对照 |
| Owner-C（Infra/CI） | `ISSUE-014` | fallback 日志、merge gate、artifact 归档与平台矩阵维护 |
| Owner-D（Docs/Tooling） | `ISSUE-017` + `ISSUE-022` + `ISSUE-024` | API 文档维护、Creator Toolchain 入口、宿主接入说明 |

### 5.3 建议 PR 顺序（下一阶段）

1. `PR-M3-003`：fallback 验证日志/平台矩阵归档补齐（`ISSUE-014`）
2. `PR-M3-004`：golden 容差摘要与后端一致性门禁继续收口（`ISSUE-010`）
3. `PR-M3-005`：RVV qemu 侧一致性与 perf 证据继续收口（`ISSUE-011`）
4. `PR-M3-006`：P0 四件套主路径已落地（state-hash / frame reuse + op cache + Dirty-Tile + dynres runtime slice）；下一步转向收益固化、回归分析与默认值策略（`ISSUE-008`）
5. `PR-M4-001`：Creator Toolchain stage-1（先聚合 `probe/perf`，再接 `migrate`）（`ISSUE-022`）
6. `PR-M3-X`：待 `native-riscv64/RVV` 设备到位后，再恢复 `ISSUE-020` 的 runner/nightly 计划

### 5.4 当前收口标准

当前阶段每轮结束前必须满足：

1. 影响 runtime/backend/ci 的变更，必须在 `issue.md` 或对应文档里留下可复现命令、artifact 路径或 workflow run 号。
2. 在缺少 `native-riscv64/RVV` 设备时，`ISSUE-020` 保留为 blocked；新增生态/实验性任务不得中断当前的 qemu/golden/perf 收口主线。
3. 新引入的 perf/golden 结论必须可对照、可回退、可在 CI 或脚本中复现。

---

## ISSUE-001 冻结单一前后端 API（`vn_backend.h`）

- Labels: `type:epic`, `type:feature`, `priority:P0`
- Milestone: `M0-core-scalar`
- Depends on: `-`
- Blocking: `ISSUE-002`, `ISSUE-003`, `ISSUE-006`

### 目标

定义统一 Render IR 和后端 ABI，保证跨架构只写后端。

### 交付物

- `include/vn_backend.h`
- `include/vn_renderer.h`（接口对齐）
- `docs/backend-contract.md`（可选）

### 任务清单

- [ ] 定义 `VNRenderOp`、`VNBackendCaps`、`VNRenderBackend`
- [ ] 定义 `vn_backend_register` 与 `vn_backend_select`
- [ ] 定义后端选择顺序：`avx2 -> neon -> rvv -> scalar`
- [ ] 定义强制切换参数：`--backend=...`
- [ ] 明确“前端源码零改动”写入注释与文档

### 验收命令

```bash
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -c include/vn_backend.h -o /tmp/vn_backend_h.o
```

### DoD

- [ ] Frontend 源码不依赖 ISA 私有头文件
- [ ] ABI 字段冻结，变更需走兼容策略
- [ ] 至少一个后端（scalar）可成功注册并被选择

### 回退策略

接口发生争议时保持旧字段语义不变，仅追加字段并保留旧路径。

---

## ISSUE-002 Frontend 输出 `VNRenderOp[]`

- Labels: `type:feature`, `priority:P0`
- Milestone: `M0-core-scalar`
- Depends on: `ISSUE-001`
- Blocking: `ISSUE-003`, `ISSUE-007`

### 目标

将剧情层与后端执行层解耦，Frontend 只生成 Render IR。

### 交付物

- `src/frontend/*`
- `src/core/render_ops.c`（命名按仓库实际调整）
- 单测：`tests/unit/test_render_ops_*`

### 任务清单

- [ ] 实现 `build_render_ops(...)`
- [ ] 移除 Frontend 中直接 ISA/像素缓冲调用
- [ ] 接入 `renderer_submit(op_buf, op_count)`
- [ ] 增加 `VNRenderOp` 构建单测

### 验收命令

```bash
ctest --test-dir build --output-on-failure -R render_ops
```

### DoD

- [ ] `TEXT/WAIT/GOTO/END` 路径跑通
- [ ] 切换后端无前端源码改动
- [ ] 单测覆盖主要操作类型（clear/sprite/text/fade）

### 回退策略

保留旧渲染入口适配层一版（deprecated），便于平滑切换。

---

## ISSUE-003 `scalar` 后端最小可运行（600x800 + S0）

- Labels: `type:feature`, `arch:scalar`, `priority:P0`
- Milestone: `M0-core-scalar`
- Depends on: `ISSUE-001`, `ISSUE-002`
- Blocking: `ISSUE-005`, `ISSUE-007`

### 目标

建立全平台可运行基线，作为所有 SIMD 后端的行为参考。

### 交付物

- `src/backend/scalar/*`
- `tests/golden/scalar/*`
- demo 首屏运行截图或日志

### 任务清单

- [ ] 实现 `init/begin/submit/end/shutdown`
- [ ] 支持最小操作集：`clear/sprite/text/fade`
- [ ] 接入档位 `R0/R1/R2` 的后端输出尺寸切换
- [ ] 增加初始化失败回退与日志

### 验收命令

```bash
./build/vn_player --backend=scalar --scene S0 --resolution 600x800
```

### DoD

- [ ] `S0` 在 `600x800` 可运行
- [ ] x86_64 标量路径 >=30fps
- [ ] 后端错误码和日志字段齐全

### 回退策略

若 `600x800` 未达目标，允许临时降档 `R1/R2`，但保留 `R0` 可启动能力。

---

## ISSUE-004 资源链路最小可用（`vnpak` + 脚本编译）

- Labels: `type:feature`, `type:infra`, `priority:P0`
- Milestone: `M0-core-scalar`
- Depends on: `ISSUE-001`
- Blocking: `ISSUE-003`, `ISSUE-012`

### 目标

打通资源生产到运行时加载闭环。

### 交付物

- `tools/packer/*`
- `tools/scriptc/*`
- `assets/demo/*.vnpak`

### 任务清单

- [x] `vnpak` 头表解析
- [x] 图像转换（PNG -> RGBA16/CI8/IA8）
- [x] 脚本编译（txt -> bin）
- [x] CRC32 与 `manifest.json`

### 验收命令

```bash
./tools/packer/make_demo_pack.sh
./build/vn_player --backend=scalar --pack assets/demo/demo.vnpak --scene S0
```

### DoD

- [x] Demo 资源可加载
- [x] 打包输出可复现（同输入同哈希）
- [x] 错误能映射到统一错误码

### 回退策略

工具链异常时允许使用已生成 demo 包继续验证运行时。

---

## ISSUE-005 基准场景 S0-S3 + S10 与 perf 采样

- Labels: `type:test`, `type:infra`, `priority:P0`
- Milestone: `M0-core-scalar`
- Depends on: `ISSUE-003`
- Blocking: `ISSUE-007`, `ISSUE-008`, `ISSUE-009`, `ISSUE-011`

### 目标

统一性能评估输入，避免“不同场景得出不同结论”。

### 交付物

- `tests/perf/scenes/*`
- `tests/perf/run_perf.sh`（或等效）
- `tests/perf/run_perf_compare_revs.sh`
- `tests/perf/perf_*.csv`
- `docs/perf-rvv-2026-03-06.md`
- `scripts/ci/run_riscv64_qemu_perf_report.sh`

### 任务清单

- [x] 固定场景 `S0/S1/S2/S3`，并补入更重的 `S10` perf sample
- [x] 输出字段：`scene,frame,frame_ms,vm_ms,build_ms,raster_ms,audio_ms,rss_mb`
- [x] 热身 20 秒规则
- [x] 报告模板（设备/参数/版本）
- [x] baseline/candidate 对比脚本与 markdown 汇总（`run_perf_compare.sh` / `compare_perf.sh`）
- [x] `test_runtime_api` + `test_preview_protocol` 已补 `S10` 端到端覆盖（scene string -> pack -> VM -> frontend -> result/protocol）
- [x] revision compare helper 与入库 markdown 报告（`run_perf_compare_revs.sh` / `docs/perf-rvv-2026-03-06.md`）
- [x] Linux x64 CI perf artifact（`scalar vs avx2`）
- [x] riscv64 qemu perf-report wrapper/workflow 已接入（`scripts/ci/run_riscv64_qemu_perf_report.sh` / `.github/workflows/riscv-perf-report.yml`）

### 验收命令

```bash
./tests/perf/run_perf.sh --backend scalar --scenes S0,S1,S2,S3,S10
```

### DoD

- [x] 每场景输出 CSV
- [x] 可计算 p95
- [x] 可重复执行并复现实验结论

### 回退策略

自动采样异常时，保留最小人工采样脚本，不阻塞功能验证。

---

## ISSUE-006 C89 门禁与头文件独立编译检查

- Labels: `type:infra`, `priority:P0`
- Milestone: `M0-core-scalar`
- Depends on: `ISSUE-001`
- Blocking: 全部编码类 issue

### 目标

确保“全项目 C89”不是口头约束，而是 CI 约束。

### 交付物

- CI 配置（构建参数）
- 扫描脚本（禁用 C99/C11 特征）

### 任务清单

- [ ] 接入 `-std=c89 -pedantic-errors -Wall -Wextra -Werror`
- [ ] 扫描 `stdint.h/stdbool.h/inline/for(int...)`
- [ ] 公共头文件逐个独立编译

### 验收命令

```bash
cmake -S . -B build -DCMAKE_C_STANDARD=90 -DCMAKE_C_EXTENSIONS=OFF
cmake --build build -j
```

### DoD

- [ ] Debug/Release 通过
- [ ] 无 C99/C11 特征泄漏
- [ ] 门禁接入 CI 默认分支

### 回退策略

临时白名单必须有过期时间和负责人，且仅限工具代码。

---

## ISSUE-007 `avx2` 后端实现与运行时切换

- Labels: `type:feature`, `type:perf`, `arch:avx2`, `priority:P0`
- Milestone: `M1-amd64-avx2`
- Depends on: `ISSUE-003`, `ISSUE-005`
- Blocking: `ISSUE-008`, `ISSUE-009`

### 目标

完成前期 amd64 性能主力后端，不改 Frontend。

### 交付物

- `src/backend/avx2/*`
- 差异对照产物：`test_runtime_golden_<scene>_<backend>_{expected,actual,diff}.ppm`
- 差异摘要产物：`test_runtime_golden_<scene>_<backend>_summary.txt`

### 任务清单

- [x] `fill` 核心算子（AVX2 向量写入，对齐前缀 + aligned store）
- [x] `uniform alpha/fade` 核心算子（AVX2 row kernel，避免整屏 `FADE` 逐像素走公共 `blend_rgb`）
- [x] `SPRITE/TEXT` 热路径去跨 TU 调用（backend-local `hash/sample/combine/blend` row loop）
- [x] 覆盖 `VN_OP_CLEAR/VN_OP_SPRITE/VN_OP_TEXT/VN_OP_FADE` 执行链路
- [x] `--backend=avx2` 强制切换（CPU 支持时使用 avx2，不支持时回退 scalar）
- [x] amd64 自动优先选择 `avx2`
- [x] `tex/combine` 真采样路径（共享 `pixel_pipeline`，`scalar/avx2` 同语义）
- [x] 纹理坐标热路径优化：UV LUT（减少逐像素除法）
- [x] textured full-span 热路径优化：row palette + repeated-`v8` reuse（避免重复 `sample/hash/combine`）
- [x] 与 scalar 一致性对照测试（`test_backend_consistency` CRC 对照）
- [x] 运行时 golden CRC + 像素级基线（`test_runtime_golden`: `S0/S1/S2/S3/S10 @ 600x800`，`scalar` CRC 严格固定；支持后端与其做图差对照）
- [x] golden 图差异测试（误差阈值：`mismatch_percent < 1%` 且 `max_channel_diff <= 8`；差异/CRC 异常时导出 PPM + `summary.txt`）

### 验收命令

```bash
./build/vn_player --backend=avx2 --scene S0
./tests/perf/run_perf.sh --backend avx2 --scenes S0,S1,S2,S3,S10
```

### DoD

- [ ] `R0(600x800)` 下 `S0-S3 >=60fps`（x86_64），`S10` 作为更重压力样本进入 perf 报告与趋势门禁
- [x] 差异图误差 <1%（`test_runtime_golden` 对可选后端强制执行 `mismatch_percent < 1%`，且额外限制 `max_channel_diff <= 8`）
- [x] 回退到 scalar 功能无回归（单测：`test_renderer_fallback`）

### 回退策略

AVX2 不稳定时默认切回 scalar，同时保留 `--backend=avx2` 便于诊断。

---

## ISSUE-008 P0 性能四件套

- Labels: `type:perf`, `priority:P0`
- Milestone: `M1-amd64-avx2`
- Depends on: `ISSUE-007`
- Blocking: `ISSUE-009`, `ISSUE-012`

### 目标

先建立性能回归门限文件与 CI 比对开关矩阵；当前第一批 P0 优化中的静态帧短路（state hash / frame reuse）、命令缓存与 Dirty-Tile 已落地，动态分辨率也已完成 runtime 最小 slice。下一阶段继续把 `dirty/dynres` 的 on/off compare 证据、默认值决策与跨架构长期 perf 数据补齐，并在正确性前提下尽可能挖掘可获得性能收益。

### 交付物

- 性能门限文件：`tests/perf/perf_thresholds.csv`
- 门限校验脚本：`tests/perf/check_perf_thresholds.sh`
- 性能开关实现
- 性能报告 `docs/perf-report.md`
- Kernel benchmark 工具：`tests/perf/backend_kernel_bench.c` + `tests/perf/run_kernel_bench.sh`
- Kernel compare wrapper：`tests/perf/run_kernel_compare.sh`
- Dirty-Tile 设计/API 现状文档：`docs/api/dirty-tile-draft.md`

### 任务清单

- [x] 性能门限文件（按 profile 描述 smoke/revision compare gate）
- [x] compare 门限校验脚本与 markdown 报告（`perf_threshold_metrics/results/report`）
- [x] `linux-x64` `scalar -> avx2` smoke gate 接入 `ci-matrix`
- [x] `linux-x64` `avx2 dirty off -> on` compare artifact 接入 `ci-matrix`
- [x] backend kernel benchmark 工具（`clear/fade/textured` hotspot isolation）
- [x] backend kernel compare artifact 接入 native perf workflow（`Compare D`）
- [x] AVX2 row-palette apply 向量化（gather/store + gather/blend，针对 `vis_w > 256`）
- [x] repeat compare variability artifact（`perf_summary_repeats.csv` + `perf_repeat_variability.csv/.md` + `perf_compare.md` 附加章节）
- [x] `qemu-rvv` revision compare gate 接线（已接入 `riscv-perf-report.yml`，默认 `soft` 模式产出 threshold report，待观察噪声后决定是否转阻塞）
- [x] 静态帧短路（state hash）
- [x] Dirty-Tile 设计/API 现状文档（`docs/api/dirty-tile-draft.md`）
- [x] Dirty-Tile runtime flag / result / preview / CLI 接线
- [x] Dirty-Tile 内部 planner 与统计（`src/frontend/dirty_tiles.c/.h`）
- [x] Dirty-Tile shared renderer/backend 契约 + scalar dirty submit + fallback
- [x] Dirty-Tile x64 `avx2` dirty submit
- [x] Dirty-Tile arm64 `neon` dirty submit
- [x] Dirty-Tile riscv64 `rvv` dirty submit（qemu smoke）
- [x] Dirty-Tile runtime fast-path（已知必整帧时跳过 planner 构建）
- [x] Dirty-Tile full-redraw commit fast-path（整帧帧 shallow commit + 惰性 prev_bounds 重建）
- [ ] Native RVV dirty-submit validation / perf 证据
- [x] 命令缓存（LRU）
- [x] 动态分辨率 runtime 最小闭环（controller + runtime/preview/result/CLI）
- [x] 动态分辨率 `R0/R1/R2` on/off compare artifact
- [ ] 动态分辨率默认开启决策与阈值校准
- [x] 运行时开关与日志


### Dirty-Tile 子任务（建议实施顺序）

1. Runtime：`VN_RUNTIME_PERF_DIRTY_TILE`、trace/summary/result 统计字段与 `--perf-dirty-tile=<on|off>` 已落地，当前默认保持 `off`；对 `FADE`、op 结构变化、clear 变化等“已知必整帧”场景，runtime 现已直接 short-circuit 到 `full_redraw`，并在 commit 阶段只 shallow-commit `prev_ops`，把 `prev_bounds` 重建延后到重新回到可增量帧时再做，避免 dirty-on 路径继续平白支付整帧 bounds 计算成本。
2. Frontend：dirty planner 已落地，当前比较 `last_presented_ops[]` 与当前帧最终 `VNRenderOp[]`，输出 tile 计数与 clip rect 列表，并已驱动 runtime 选择 dirty submit vs full submit；tile 计数现已改为标记时增量维护，避免 partial 路径再做一次 bitset 全表 recount。
3. Renderer/API：`renderer_submit_dirty(...)` 与 backend `submit_ops_dirty(...)` 契约已落地；未实现 dirty callback 的后端当前自动回退整帧提交。
4. Backend：`scalar` dirty submit 已作为语义基线落地，`avx2`、`neon`、`rvv` 也已补齐 x64 / arm64 / riscv64 主力路径；下一步转向 native RVV 设备上的长期验证与 perf 证据。
5. Validation：当前 `unit dirty planner`、`runtime_api`、`preview_protocol`、`test_renderer_dirty_submit`、`runtime_golden` dirty-on，以及 `qemu-rvv` 下的 `test_renderer_dirty_submit_rvv` 已覆盖最小链路；`linux-x64` CI 现已固化 `dirty-tile on/off` compare artifact，`linux-arm64` / `windows-arm64` 也会显式校验 `matched backend=neon`。本地 dirty compare 现已改用 `repeat=3` 中位数聚合，且 `run_perf_compare.sh` 会同步写出 `perf_summary_repeats.csv`、`compare/perf_repeat_variability.csv/.md` 与 `compare/perf_compare.md` 内嵌 `Repeat Variability` 章节，用 scene 级 `min/median/max/range` 辅助判断噪声；`run_perf_smoke_suite.sh` 现在也会把这组信息提炼成 `Dirty-Tile Repeat Variability Digest` 并写进 `perf_workflow_summary.md`，让 `linux-x64/avx2`、`linux-arm64/neon`、`windows-arm64/neon` 三条原生 perf 线都能直接读到 scene 级 range。`linux-x64` 的 smoke / dirty compare 也已从 `S0,S3` 切到 `S1,S3,S10`，因为 `S0` 在主线路径上会被 frame reuse 压到约 `0.001ms`，而 `S10` 作为更重的 perf sample 用于补足压力场景。`2026-03-07` 入库样本里 `S1` 得到 `15.228ms -> 13.507ms`（`+11.30% p95`），`S3` 仍有 `15.971ms -> 16.545ms`（`-3.59% p95`）的短窗口噪声；新的 variability artifact 正是下一轮 `windows-x64 dirty S3` 噪声排查的基础，因此当前结论仍是“趋势 artifact 已更有信息量、发布级结论仍需目标机长窗口”。

### Dynamic Resolution 子任务（当前阶段）

1. Runtime/API：`VN_RUNTIME_PERF_DYNAMIC_RESOLUTION`、`--perf-dynamic-resolution=<on|off>`、`VNRunResult.render_width/render_height/dynamic_resolution_tier/dynamic_resolution_switches` 与 preview `final_state` 已落地；当前默认保持 `off`。
2. Controller：当前按 `R0/R1/R2 = 100%/75%/50%` 三档工作；最近 120 帧 p95 超过 `16.67ms` 时尝试降档，最近 300 帧 p95 低于 `13.0ms` 时尝试升档。
3. Runtime integration：切档时 runtime 会重配 renderer 尺寸，并失效 frame reuse/op cache/dirty planner 依赖的旧尺寸缓存；当前实现已保留本帧 dirty 统计，避免切档瞬间把当前帧观测值抹掉。
4. Validation：`test_dynamic_resolution` 覆盖 controller 逻辑，`test_runtime_dynamic_resolution` 覆盖真实 runtime 降档/重配置路径（`scalar + S3 + 1200x1600 + hold-end`）；该测试现通过内部 dynres override 强制在小窗口内触发 downshift，避免把 GitHub runner 的绝对性能波动误当成 runtime 逻辑回归。`run_cc_suite` 已纳入这两条测试。
5. Next：继续观察不同 backend/runner 上的 dynres 噪声与收益分布，再决定是否提升为默认路径或进入更严格的 smoke workflow。

### 验收命令

```bash
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S1,S3,S10 --duration-sec 2 --warmup-sec 1 --dt-ms 16 --resolution 600x800 --threshold-file tests/perf/perf_thresholds.csv --threshold-profile linux-x64-scalar-avx2-smoke
./tests/perf/run_perf_compare.sh --baseline avx2 --baseline-label avx2_dirty_off --baseline-perf-dirty-tile off --candidate avx2 --candidate-label avx2_dirty_on --candidate-perf-dirty-tile on --scenes S1,S3,S10 --duration-sec 6 --warmup-sec 1 --dt-ms 16 --resolution 600x800 --repeat 3
./tests/perf/run_perf_compare.sh --baseline scalar --baseline-label scalar_dynres_off --baseline-perf-dynamic-resolution off --candidate scalar --candidate-label scalar_dynres_on --candidate-perf-dynamic-resolution on --scenes S3 --duration-sec 6 --warmup-sec 1 --dt-ms 16 --resolution 1200x1600
./scripts/ci/run_perf_smoke_suite.sh --out-dir /tmp/n64gal_perf_ci
./tests/perf/run_kernel_bench.sh --backend scalar --iterations 24 --warmup 6 --out-csv /tmp/n64gal_kernel_scalar.csv
./tests/perf/run_kernel_bench.sh --backend avx2 --iterations 24 --warmup 6 --out-csv /tmp/n64gal_kernel_avx2.csv
./tests/perf/run_kernel_compare.sh --baseline scalar --candidate avx2 --resolution 600x800 --iterations 24 --warmup 6 --out-dir /tmp/n64gal_kernel_compare
PERF_THRESHOLD_MODE=soft PERF_THRESHOLD_PROFILE=linux-riscv64-qemu-rvv-rev-smoke PERF_THRESHOLD_FILE=tests/perf/perf_thresholds.csv ./scripts/ci/run_riscv64_qemu_perf_report.sh
./build_ci_cc/vn_player --scene S0 --frames 32 --hold-end --perf-frame-reuse=off
./build_ci_cc/vn_player --scene S0 --frames 32 --hold-end --perf-op-cache=off
./build_ci_cc/vn_player --backend scalar --scene S3 --resolution 1200x1600 --frames 128 --dt-ms 16 --hold-end --trace --perf-dynamic-resolution=on
```

### DoD

- [x] perf compare 结果可按 profile 自动判定并输出 threshold report
- [x] `linux-x64` smoke compare 已接门限 profile
- [x] `linux-x64` `avx2 dirty off/on` smoke compare 已产出 artifact 与 summary
- [x] `linux-x64` `scalar dynres off/on` smoke compare 已产出 artifact 与 summary
- [x] `qemu-rvv` revision compare workflow 已默认产出 threshold report（`soft` mode）
- [x] Runtime `VN_RUNTIME_PERF_FRAME_REUSE` 静态帧短路已落地并可通过 `perf_flags` / CLI 开关回退
- [x] Runtime `VNRenderOp[]` 命令缓存已落地并可通过 `perf_flags` / CLI 开关回退
- [x] 动态分辨率最小 runtime slice 已落地，且可通过 `perf_flags` / CLI 开关完全回退
- [x] Dirty-Tile / Dynamic Resolution 都已固化 on/off compare artifact（dirty-tile 短窗口噪声仍在观察）
- [ ] `S0 p95 <= 12.0ms`
- [ ] `S1 p95 <= 14.0ms`
- [ ] `S2 p95 <= 15.0ms`
- [ ] `S3 p95 <= 16.2ms`
- [ ] 针对主要热路径（raster/build/vm）至少各落地 1 项可量化优化
- [ ] 优化均可单独关闭并回退

### 回退策略

门限误报时可先切换到 report-only profile 或调宽对应 profile；任一优化退化超过 10% 时默认关闭该优化。

---

## ISSUE-009 `neon` 后端实现与 arm64 默认启用

- Labels: `type:feature`, `type:perf`, `arch:neon`, `priority:P1`
- Milestone: `M2-arm64-neon`
- Depends on: `ISSUE-007`, `ISSUE-008`
- Blocking: `ISSUE-010`

### 目标

中期平台化：在同一 Frontend 上完成 arm64 后端落地。

### 交付物

- `src/backend/neon/*`
- `tests/golden/neon_vs_scalar/*`

### 任务清单

- [x] `neon` 后端最小可运行路径与注册链
- [x] 自动选择链支持 `avx2 -> neon -> rvv -> scalar`
- [x] NEON `fill` 核心算子（向量填充）
- [ ] NEON `blend/tex/combine` 核心算子（uniform blend + wide row-palette 已落地，`sample/combine` 深化向量化待续）
- [x] aarch64 交叉编译通过
- [x] arm64 Linux 原生构建与跑测（GitHub Actions `linux-arm64` 通过）
- [x] arm64 Windows 原生构建验证（GitHub Actions `windows-arm64` 通过）

### 验收命令

```bash
./build/vn_player --backend=neon --scene S0
./tests/perf/run_perf.sh --backend neon --scenes S0,S1,S2,S3,S10
```

### DoD

- [ ] Zero 2W：`S0-S2 >=45fps`, `S3 >=40fps`，`S10` 作为更重压力样本进入 perf 报告与趋势观察
- [ ] 与 scalar 差异图误差 <1%
- [ ] 内存峰值 <=64MB

### 回退策略

默认选择失败时切换 scalar 并记录 `WARN backend_fallback`.

---

## ISSUE-010 后端一致性测试与 CI 矩阵

- Labels: `type:test`, `type:infra`, `priority:P1`
- Milestone: `M2-arm64-neon`
- Depends on: `ISSUE-007`, `ISSUE-009`
- Blocking: `ISSUE-011`

### 目标

将后端一致性和跨架构编译变成强门禁。

### 交付物

- CI 矩阵 Job A-F
- 差异图自动化测试脚本

### 任务清单

- [x] GitHub Actions workflow 骨架（`.github/workflows/ci-matrix.yml`）
- [x] Job A: x86_64 + scalar（`linux-x64`）
- [x] Job B: x86_64 + avx2（`linux-x64`）
- [x] Job C: arm64 + scalar (cross/native)（`linux-arm64` / `windows-arm64`）
- [x] Job D: arm64 + neon (cross/native)（`linux-arm64` / `windows-arm64`）
- [x] Job E: riscv64 + scalar (cross)（`linux-riscv64-cross` + `linux-riscv64-qemu-scalar`）
- [x] Job F: riscv64 + rvv（已转阻塞）

### 验收命令

```bash
ctest --test-dir build --output-on-failure -R backend_consistency
```

### DoD

- [x] M1 起 Job A/B 阻塞
- [x] M2 起 Job C/D 阻塞
- [x] Job F 已转阻塞（2026-03-06，最近 12 轮 workflow 连续 success）
- [ ] 任一后端失败时 scalar 回退路径可验证

### 回退策略

非关键架构 job 可临时降告警，但需附负责人和截止日期。

---

## ISSUE-011 `rvv` 后端实现与 riscv64 回退链

- Labels: `type:feature`, `type:perf`, `arch:rvv`, `priority:P2`
- Milestone: `M3-riscv64-rvv`
- Depends on: `ISSUE-010`
- Blocking: `ISSUE-012`

### 目标

后期扩展：在单一 API 下完成 RVV 后端落地。

### 交付物

- `src/backend/rvv/*`
- `docs/riscv-toolchain.md`
- `docs/perf-rvv-2026-03-06.md`
- `tests/perf/run_perf_compare_revs.sh`
- `scripts/ci/run_riscv64_qemu_perf_report.sh`

### 任务清单

- [x] `rvv` 后端最小可运行路径与注册链
- [x] 启动选择链支持 `avx2 -> neon -> rvv -> scalar`
- [x] `riscv64-linux-gnu-gcc` 交叉编译通过
- [x] `scripts/ci/build_riscv64_cross.sh` 已覆盖 `vn_player` 与核心单测交叉构建
- [x] `scripts/ci/run_riscv64_qemu_suite.sh --skip-rvv` 已验证 `scalar`/回退链/pack/runtime/session`
- [x] `scripts/ci/run_riscv64_qemu_suite.sh --require-rvv` 已验证 `vn_player_rvv` 在 `qemu-user` 下实际落到 `backend=rvv`
- [x] `test_backend_consistency_rvv` 已验证 `scalar vs rvv` framebuffer CRC 一致
- [x] RVV `fill` 核心算子（向量填充）
- [x] RVV 统一色半透明 `fade/fill` 路径已向量化
- [x] RVV `SPRITE/TEXT` 的 varying-src alpha 写回路径已批量化并使用向量混合
- [x] RVV `SPRITE/TEXT` 的 `combine` 阶段已按行批量化并使用向量处理
- [x] RVV `tex/hash` 采样核心算子已按行批量向量化
- [x] RVV `sample -> combine` 融合，减少中间 row buffer 往返（已通过 `run_cc_suite` + `build_riscv64_cross` + `run_riscv64_qemu_suite --require-rvv` 语义对照）
- [x] RVV `alpha=255` 专用融合路径：直接 `sample -> combine -> store`，绕开中间整行回写
- [x] RVV `alpha<255` 专用融合路径：`sample -> combine -> blend/store` 单循环化（已通过 `run_cc_suite` + `build_riscv64_cross` + `run_riscv64_qemu_suite --require-rvv` 语义对照）
- [x] RVV UV LUT 已收口到 8-bit 存储，降低采样带宽与 LUT 占用
- [x] RVV `seed/checker` 常量与基础 RGB 偏置已前折叠到行级参数，减少逐 chunk 标量预处理与分支
- [x] RVV 融合优化前后补 `perf_compare` 证据（`docs/perf-rvv-2026-03-06.md`，`75ee8f9 -> ee42c39`，qemu-user smoke）
- [x] `tests/perf/run_perf_compare_revs.sh` 与 `scripts/ci/run_riscv64_qemu_perf_report.sh` 已打通 qemu RVV markdown artifact 生成链
- [x] `qemu-rvv` 已从告警提升到阻塞（2026-03-06）
- [ ] riscv64 Linux 原生运行验证
- [ ] 工具链版本固定与构建说明（`docs/riscv-toolchain.md` 持续维护）

### 验收命令

```bash
./scripts/ci/build_riscv64_cross.sh
./scripts/ci/run_riscv64_qemu_suite.sh --skip-rvv
./scripts/ci/run_riscv64_qemu_suite.sh --require-rvv
OUT_DIR=/tmp/n64gal_perf_ci_wrapper BASELINE_REV=75ee8f9 CANDIDATE_REV=HEAD ./scripts/ci/run_riscv64_qemu_perf_report.sh
./tests/perf/run_perf.sh --backend rvv --scenes S0,S1,S2,S3,S10
CC=riscv64-linux-gnu-gcc \
VN_PERF_CFLAGS='-march=rv64gcv -mabi=lp64d' \
VN_PERF_RUNNER_PREFIX='qemu-riscv64 -cpu max,v=true -L /usr/riscv64-linux-gnu' \
./tests/perf/run_perf_compare_revs.sh --baseline-rev 75ee8f9 --candidate-rev ee42c39 --backend rvv --scenes S0,S3 --duration-sec 2 --warmup-sec 1 --dt-ms 16 --resolution 600x800 --out-dir /tmp/n64gal_perf_rvv_compare
```

### DoD

- [ ] `riscv64` 验证链分层落地：`cross-build -> qemu-scalar -> qemu-rvv -> native-riscv64`
- [ ] riscv64：`S0-S2 >=35fps`, `S3 >=30fps`，`S10` 作为更重压力样本纳入原生 perf 报告
- [ ] rvv 与 scalar 差异图误差 <1%
- [ ] rvv 初始化失败自动切回 scalar

### 回退策略

功能验证先以 `riscv64 + scalar` 与 `qemu-user` 阻塞链保证主线稳定；RVV 不稳定时保留为实验特性，等待原生 riscv64 验证完成后再提升权重。

---

## ISSUE-012 发布收口（文档/报告/迁移指南）

- Labels: `type:docs`, `type:infra`, `priority:P1`
- Milestone: `M3-riscv64-rvv`
- Depends on: `ISSUE-008`, `ISSUE-009`, `ISSUE-011`
- Blocking: 发布

### 目标

完成 `v0.1.0-mvp` 发布前全部文档和证据链。

### 交付物

- `README`
- `docs/perf-report.md`
- `docs/migration.md`
- `docs/backend-porting.md`
- 发布产物（可执行 + `demo.vnpak` + 许可证）

### 任务清单

- [ ] 汇总性能报告与测试附件
- [ ] 输出后端移植指南
- [ ] 完成发布清单检查

### 验收命令

```bash
./tests/perf/run_perf.sh --backend auto --scenes S0,S1,S2,S3,S10
ctest --test-dir build --output-on-failure
```

### DoD

- [ ] `S0-S3 + S10` 全通过且无 `ERROR` 级日志
- [ ] C89 门禁与单测 100% 通过
- [ ] Demo 连续 15 分钟无崩溃
- [ ] 发布清单完整

### 回退策略

缺失文档或证据链时阻塞发版，不允许“补文档后置”。

---

## ISSUE-013 Linux/Windows 平台抽象层与系统适配

- Labels: `type:infra`, `type:feature`, `priority:P0`
- Milestone: `M1-amd64-avx2`
- Depends on: `ISSUE-001`, `ISSUE-006`
- Blocking: `ISSUE-014`

### 目标

在不改 Frontend/Backend API 的前提下，补齐 Linux/Windows 双平台系统层适配。

### 交付物

- `src/core/build_config.h`
- `src/core/platform.h`
- `src/core/platform.c`
- `tests/unit/test_platform_paths.c`
- `docs/platform-matrix.md`

### 任务清单

- [x] 统一时间/休眠接口（Linux + Windows，`vn_platform_now_ms` + `vn_platform_sleep_ms` + CLI keyboard pacing）
- [x] 统一终端输入接口（Linux/Windows CLI 键盘语义一致）
- [x] 路径与文件 I/O 兼容性收口（分隔符/二进制模式，`platform.c` + `pack.c` + `preview_cli.c` + `test_platform_paths`）
- [x] 编译器差异收口（GCC/Clang/MSVC，`build_config.h` + `avx2` GNU/MSVC + preview/platform host detect）

### 验收命令

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
./scripts/ci/run_cc_suite.sh
```

### DoD

- [x] Linux x64 构建与单测通过
- [x] Windows x64 构建与单测通过
- [x] 不引入 Frontend API 破坏性变更

### 回退策略

Windows 专属改动可短期 feature flag 化，但必须保留 Linux 主线稳定。

---

## ISSUE-014 x64/arm64 + Linux/Windows + riscv64 Linux CI 矩阵

- Labels: `type:infra`, `type:test`, `priority:P0`
- Milestone: `M2-arm64-neon`
- Depends on: `ISSUE-010`, `ISSUE-013`
- Blocking: `ISSUE-012`

### 目标

把平台目标固化到 CI：任何主线变更都必须覆盖 x64/arm64 + Linux/Windows，并逐步覆盖 riscv64 Linux。

### 交付物

- `.github/workflows/ci-matrix.yml`
- `.github/workflows/riscv-perf-report.yml`
- `scripts/ci/run_windows_suite.ps1`
- `scripts/ci/run_riscv64_qemu_perf_report.sh`
- `scripts/ci/run_perf_smoke_suite.sh`
- suite log / golden artifact 归档约定（`ci_logs/`、`ci_suite_summary.md`、`golden_artifacts/`）
- 平台矩阵结果汇总文档

### 任务清单

- [x] `.github/workflows/ci-matrix.yml` 已接入 x64/arm64/riscv64 组合
- [x] Job A: Linux x64（scalar + avx2）
- [x] Job B: Windows x64（scalar + avx2）
- [x] Job C: Linux arm64（scalar + neon）
- [x] Job D: Windows arm64（scalar + neon）
- [x] Job E0: Linux riscv64 cross-build（交叉构建）
- [x] Job E1: Linux riscv64 qemu-scalar（`scalar`/回退链/pack/runtime，阻塞）
- [x] Job F0: Linux riscv64 qemu-rvv（`rvv` 冒烟，2026-03-06 起转为阻塞）
- [x] Job F0-report: Linux riscv64 qemu-rvv perf-report（workflow_dispatch/nightly artifact）
- [x] Job F0-report 解析错误已修复（`1792d6e`，`runner.temp -> github.workspace`）
- [x] Job F0-report 首次 GitHub 端 workflow_dispatch 冒烟完成并记录 artifact / step summary（run `22766736383`，`success`）
- [x] Linux x64 / Linux arm64 native suite 日志 + summary artifact（`suite-linux-x64` / `suite-linux-arm64`）
- [x] Linux arm64 workflow 已显式校验 `test_renderer_dirty_submit` 命中 `matched backend=neon`
- [x] Linux riscv64 qemu suite 日志 + summary artifact（`suite-linux-riscv64-qemu-scalar` / `suite-linux-riscv64-qemu-rvv`）
- [x] `test_runtime_golden` 已支持通过 `VN_GOLDEN_ARTIFACT_DIR` 落入 CI artifact 目录
- [x] Windows x64 / Windows arm64 suite 日志 artifact（通过 `scripts/ci/run_windows_suite.ps1` 统一收口到 `suite-windows-x64` / `suite-windows-arm64`，并已在 GitHub Actions run `22772138491` 实跑验证，脚本在失败场景下仍会产出 summary/日志）
- [x] Windows arm64 workflow 已显式校验 `test_renderer_dirty_submit` 命中 `matched backend=neon`
- [x] 原生平台 perf artifact 已落地：`linux-x64` 固化 `scalar -> avx2` threshold report、`avx2 dirty off/on` compare summary 与 `scalar dynres off/on` compare summary；`linux-arm64` / `windows-arm64` 已新增正收益 threshold profile，`windows-x64` 也已从 regression-envelope 升级为正收益 threshold profile，三者都开始产出对应 `perf-*` smoke artifact
- [ ] Job F1: Linux riscv64 native-nightly（真机功能 + perf）
- [ ] 失败回退路径验证（每个平台至少 1 例）

### 验收命令

```bash
ctest --test-dir build --output-on-failure
pwsh -File scripts/ci/run_windows_suite.ps1 -PlatformLabel windows-x64 -CMakePlatform x64 -SuiteRoot build_ci_windows_x64
pwsh -File scripts/ci/run_windows_suite.ps1 -PlatformLabel windows-arm64 -CMakePlatform ARM64 -SuiteRoot build_ci_windows_arm64
./scripts/ci/build_riscv64_cross.sh
./scripts/ci/run_riscv64_qemu_suite.sh --skip-rvv
./scripts/ci/run_riscv64_qemu_suite.sh --require-rvv
OUT_DIR=/tmp/n64gal_perf_ci_wrapper BASELINE_REV=75ee8f9 CANDIDATE_REV=HEAD ./scripts/ci/run_riscv64_qemu_perf_report.sh
```

### DoD

- [x] x64/arm64 四象限 job 全绿
- [ ] `linux-riscv64-cross` 阻塞并稳定
- [x] `linux-riscv64-qemu-scalar` 已转阻塞
- [x] `linux-riscv64-qemu-rvv` 已转阻塞（2026-03-06，最近 12 轮 workflow 连续 success）
- [x] `linux-riscv64-qemu-rvv-perf-report` 的 GitHub 端 artifact 已完成首次 workflow_dispatch 验证（本地 wrapper 与 GitHub run `22766736383` 均已跑通）
- [x] 每个平台均有回退链验证日志（Windows suite 已在 GitHub Actions run `22772138491` 完成实跑复核）
- [ ] CI 失败阻塞 main 合并

### 回退策略

`riscv64` 必须拆成 `cross-build`、`qemu-scalar`、`qemu-rvv`、`native-nightly` 四层；单层不稳定时只允许降级当前层级，不允许把“已知不可运行”的更高层结果冒充主线稳定性证据。

---

## ISSUE-015 `vnsave v1` 存档迁移器

- Labels: `type:feature`, `type:infra`, `priority:P2`
- Milestone: `M4-engine-ecosystem`
- Depends on: `ISSUE-004`, `ISSUE-012`
- Blocking: `ISSUE-025`

### 目标

把未来对外的 `vnsave` 版本演进收口到可执行迁移链，而不是靠宿主自行兜底历史格式。

### 交付物

- `tools/migrate/`（或等效 `vnsave` 迁移入口）
- `docs/migration.md`
- `tests/fixtures/vnsave/*`

### 任务清单

- [ ] 明确 `vnsave v1` 文件头与版本探测规则
- [ ] 提供 `vnsave v0 -> v1` 迁移命令
- [ ] 为损坏/过旧存档提供结构化错误输出
- [ ] 增加至少 1 组 golden 迁移样例

### 验收命令

```bash
./tools/migrate/vnsave_migrate --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave
```

### DoD

- [ ] 历史存档升级路径可复现
- [ ] 迁移失败可定位到版本/字段层级
- [ ] release 文档附带迁移说明

### 回退策略

若迁移器尚未稳定，对外禁止承诺旧存档兼容，并明确要求重新生成存档。

---

## ISSUE-016 Runtime Session API 与宿主循环对接

- Labels: `type:feature`, `type:docs`, `priority:P0`
- Milestone: `M0-core-scalar`
- Depends on: `ISSUE-001`, `ISSUE-002`
- Blocking: `ISSUE-023`, `ISSUE-024`

### 目标

把运行时主循环从 CLI 专有路径抽出来，提供宿主可直接驱动的 Session API。

### 交付物

- `include/vn_runtime.h`
- `docs/api/runtime.md`
- `tests/unit/test_runtime_session.c`
- `tests/unit/test_runtime_input.c`

### 任务清单

- [x] `vn_runtime_session_create/step/is_done/destroy` 已落地
- [x] `vn_runtime_session_set_choice` 已接通脚本分支选择
- [x] `vn_runtime_session_inject_input` 已接通离散输入注入
- [x] CLI / preview / host 示例已复用同一套 Session API
- [x] Session API 已接入单测与文档，且 `test_runtime_api` 与 `test_preview_protocol` 已显式覆盖 `S10` 重场景

### 验收命令

```bash
ctest --test-dir build --output-on-failure -R runtime_session
ctest --test-dir build --output-on-failure -R runtime_input
```

### DoD

- [x] 宿主无需依赖 CLI 私有逻辑即可驱动运行时
- [x] choice / key 注入语义稳定
- [x] 销毁路径无资源泄漏回归

### 回退策略

若宿主 API 调整，必须保留旧调用序列一版并在文档中标注弃用窗口。

---

## ISSUE-017 API 文档集维护规范

- Labels: `type:docs`, `type:infra`, `priority:P1`
- Milestone: `M0-core-scalar`
- Depends on: `ISSUE-001`, `ISSUE-016`
- Blocking: `ISSUE-024`, `ISSUE-025`

### 目标

建立持续维护的 API 文档集，避免运行时代码和外部接入文档脱节。

### 交付物

- `docs/api/README.md`
- `docs/api/runtime.md`
- `docs/api/backend.md`
- `docs/api/pack.md`

### 任务清单

- [x] `docs/api/*` 文档集已建立
- [x] 文档入口已说明 runtime/backend/pack 三类 API 边界
- [x] Session API、backend 选择链、pack 版本约束已入文档
- [ ] 建立 API 变更日志或兼容记录模板
- [ ] 把“代码变更必须同步文档”写入提交/评审约束

### 验收命令

```bash
grep -RIn "vn_runtime_session_create\|vn_runtime_session_inject_input\|vn_backend" docs/api include
```

### DoD

- [x] 对外 API 均有落盘文档入口
- [ ] API 破坏性变更必须同步兼容记录
- [ ] 发布时可从 `docs/api/*` 直接生成接入说明

### 回退策略

文档维护压力过大时允许先保证 runtime/backend/pack 三份核心文档，但不允许回到“仅看源码”状态。

---

## ISSUE-018 错误码与日志可观测性升级

- Labels: `type:feature`, `type:infra`, `priority:P2`
- Milestone: `M4-engine-ecosystem`
- Depends on: `ISSUE-016`, `ISSUE-017`
- Blocking: `ISSUE-023`, `ISSUE-024`

### 目标

提升错误码、日志和调试输出的一致性，让宿主、预览器和 CI 报告可以共享一套可观测语义。

### 交付物

- `include/vn_error.h`（或等效）
- `docs/errors.md`
- trace id / structured log 约定

### 任务清单

- [ ] 收口公共错误码枚举与字符串映射
- [ ] 为预览协议、运行时、工具链定义统一 trace id
- [ ] 为 CI / perf / preview 输出统一机器可读错误字段
- [ ] 增加错误链路单测

### 验收命令

```bash
ctest --test-dir build --output-on-failure -R preview_protocol
```

### DoD

- [ ] 常见失败路径可定位到稳定错误码
- [ ] 日志可被宿主和 CI 机器解析
- [ ] trace id 能跨 runtime / preview / toolchain 传递

### 回退策略

若全局统一一次性成本过高，先从 preview/runtime/toolchain 三条主链共享错误码开始。

---

## ISSUE-019 WebAssembly 实验性后端

- Labels: `type:feature`, `priority:P2`
- Milestone: `M4-engine-ecosystem`
- Depends on: `ISSUE-001`, `ISSUE-013`
- Blocking: 非主线阻塞

### 目标

探索 WebAssembly 形态的最小运行闭环，但不影响当前 Linux/Windows/arm64/riscv64 主线。

### 交付物

- `wasm/`（或等效实验目录）
- 浏览器或 wasm runner 的最小 demo
- 平台限制说明文档

### 任务清单

- [ ] 收口 wasm 目标所需的平台抽象差异
- [ ] 实现最小 `scalar`/共享后端运行闭环
- [ ] 明确文件 I/O、计时、输入桥接限制
- [ ] 输出实验性接入文档

### 验收命令

```bash
emcmake cmake -S . -B build-wasm
cmake --build build-wasm
```

### DoD

- [ ] 可在 wasm 环境跑通最小场景
- [ ] 不对主线 C89 运行时代码造成平台回归
- [ ] 明确标注为实验特性

### 回退策略

若平台分歧过大，限制为独立实验分支，不进入主线发布承诺。

---

## ISSUE-020 riscv64 原生 runner / 开发板接入与 nightly perf 采样

- Labels: `type:infra`, `type:test`, `type:perf`, `arch:rvv`, `priority:P1`
- Milestone: `M3-riscv64-rvv`
- Depends on: `ISSUE-011`, `ISSUE-014`
- Blocking: `ISSUE-012`

### 目标

把 riscv64 验证从 `qemu-user` 提升到原生 runner / 开发板，建立发布前必须看的原生功能与 perf 证据链。

### 交付物

- native riscv64 runner / 开发板接入说明
- nightly workflow（或等效手动流程）
- 原生 `S0-S3 + S10` perf 报告
- `docs/riscv-toolchain.md` 原生验证补充章节

### 任务清单

- [ ] 接入至少 1 台可重复使用的 riscv64 Linux runner / 开发板（当前 blocked：暂无设备）
- [ ] 建立 native nightly 的构建、测试与 perf 采样命令
- [ ] 固化 `scalar` / `rvv` 原生功能冒烟
- [ ] 固化 `S0-S3 + S10` 原生 perf 报告与 artifact
- [ ] 覆盖至少 1 组不同 VLEN 或 CPU 能力差异说明

### 验收命令

```bash
./scripts/ci/build_riscv64_cross.sh
./tests/perf/run_perf.sh --backend rvv --scenes S0,S1,S2,S3,S10
```

### DoD

- [ ] 发布前存在原生 riscv64 功能与 perf 证据链
- [x] qemu 结果与 native 结果的定位边界清晰
- [ ] nightly 失败可追溯到 runner / toolchain / backend 具体层级

### 回退策略

原生 runner 未就绪时，保留 qemu 阻塞链与 perf artifact，但明确其不能替代发版证据。

---

## ISSUE-021 样例工程与模板仓骨架

- Labels: `type:docs`, `type:infra`, `priority:P1`
- Milestone: `M4-engine-ecosystem`
- Depends on: `ISSUE-012`
- Blocking: `ISSUE-022`, `ISSUE-023`, `ISSUE-024`

### 目标

让新用户、宿主接入方和内容作者都能从统一模板起步，而不是从仓库源码里手抄目录。

### 交付物

- `templates/minimal-vn/`
- `templates/host-embed/`
- `docs/project-layout.md`

### 任务清单

- [ ] 最小项目模板（脚本/资源/pack/运行命令齐全）
- [ ] 宿主嵌入模板（Session API + 输入桥接样例）
- [ ] 模板版本字段与升级说明
- [ ] Linux/Windows 启动命令与目录约定

### 验收命令

```bash
./tools/scriptc/build_demo_scripts.sh
./tools/packer/make_demo_pack.sh
./build/vn_player --backend=auto --scene S0
```

### DoD

- [ ] 新用户 30 分钟内可跑通首场景
- [ ] 模板不依赖仓库内私有临时路径
- [ ] 模板变更与 release 版本绑定

### 回退策略

模板不稳定时先保留 `minimal-vn` 单模板，不同时维护多套半成品模板。

---

## ISSUE-022 Creator Toolchain 聚合（`validate/migrate/probe`）

- Labels: `type:feature`, `type:infra`, `priority:P1`
- Milestone: `M4-engine-ecosystem`
- Depends on: `ISSUE-004`, `ISSUE-005`, `ISSUE-012`, `ISSUE-021`
- Blocking: `ISSUE-023`, `ISSUE-025`

### 目标

把当前分散的脚本编译、打包、校验、性能采样和迁移命令整理为 Creator Toolchain，而不是零散脚本集合。

### 交付物

- `tools/validate/`
- `tools/migrate/`
- `tools/probe/`
- `docs/toolchain.md`

### 任务清单

- [ ] `validate`：pack/script/save/version 结构校验
- [ ] `migrate`：旧版 `vnpak/vnsave/script schema` 升级命令
- [ ] `probe`：golden/perf/trace 汇总出口
- [ ] 统一 CLI 帮助、退出码和机器可读输出格式

### 验收命令

```bash
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S1,S3,S10
./scripts/check_c89.sh
```

### DoD

- [ ] Creator Toolchain 各命令帮助文本完整
- [ ] 至少 1 条迁移路径可跑通
- [x] 至少 1 条 golden/perf 报告链产出 markdown artifact

### 回退策略

聚合成本过高时允许先保留独立脚本，但必须补统一入口文档与命令约定。

---

## ISSUE-023 预览协议与无 GUI 预览进程

- Labels: `type:feature`, `type:test`, `priority:P2`
- Milestone: `M4-engine-ecosystem`
- Depends on: `ISSUE-021`, `ISSUE-022`, `ISSUE-024`
- Blocking: 外部编辑器/预览器接入

### 目标

先冻结预览协议与调试入口，再决定是否做 GUI 编辑器，避免“先有界面、后补接口”。

### 交付物

- `docs/preview-protocol.md`
- `tools/previewd/`（或等效无 GUI 入口）
- `tests/integration/test_preview_protocol_*`

### 任务清单

- [x] 定义输入参数：工程目录/场景/分辨率/backend/trace
- [x] 定义输出：首帧状态/结构化错误/日志/性能摘要
- [x] 支持控制：`reload scene`、`step frame`、`set choice`、`inject input`（`v1` 当前已实现 `choice`/`key`/`trace_toggle`/`quit`）
- [x] 初版以 CLI + 文件协议落地，后续再升级本地 IPC

### 验收命令

```bash
cat > /tmp/preview.req <<'EOF'
preview_protocol=v1
project_dir=.
scene_name=S2
frames=8
trace=1
command=set_choice:1
command=inject_input:choice:1
command=inject_input:key:t
command=step_frame:8
EOF
./build/vn_previewd --request=/tmp/preview.req --response=/tmp/preview.json
```

### DoD

- [x] CLI 与自动化脚本共用同一预览入口（`vn_previewd` / `vn_preview_run_cli`）
- [x] Linux/Windows 预览命令语义一致（统一 `argv/request file -> JSON`）
- [x] 结构化错误输出可被编辑器消费

### 回退策略

IPC 方案不稳定时先保留 CLI + 临时文件协议，不阻塞协议冻结。

### 当前实现备注

1. 入口：`include/vn_preview.h` + `src/tools/preview_cli.c` + `src/tools/previewd_main.c`
2. 文档：`docs/preview-protocol.md`
3. 测试：`tests/integration/test_preview_protocol.c`，并已接入 `run_cc_suite.sh` 与 `ctest`
4. `perf_summary` 当前是 host 侧 step 包围时间摘要，不替代正式 perf 报告
5. `inject_input` 在 `v1` 当前覆盖 `choice`/`key`/`trace_toggle`/`quit`；鼠标/触摸/文本输入仍后续扩展

---

## ISSUE-024 宿主 SDK 与版本协商文档

- Labels: `type:docs`, `type:feature`, `priority:P1`
- Milestone: `M4-engine-ecosystem`
- Depends on: `ISSUE-001`, `ISSUE-016`, `ISSUE-017`, `ISSUE-021`
- Blocking: `ISSUE-023`, 第三方宿主接入

### 目标

把“如何嵌入引擎”沉淀为稳定宿主 SDK，而不是要求外部项目直接阅读仓库源码。

### 交付物

- `docs/host-sdk.md`
- `examples/host-embed/`
- 版本协商表（`runtime/backend/script/pack/save/preview`）

### 任务清单

- [x] 明确 Session API 的宿主最小调用序列
- [x] 明确输入桥接、文件桥接、日志桥接接口
- [x] 输出版本协商矩阵与兼容规则
- [x] 最小宿主嵌入示例（`examples/host-embed/session_loop.c`）
- [x] `example_host_embed` 已接入 `CMake + ctest + run_cc_suite.sh`
- [ ] Linux/Windows 各补 1 个平台专用宿主包装层示例（窗口/输入/文件对接）

### 验收命令

```bash
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -Iinclude \
  examples/host-embed/session_loop.c \
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
  -o /tmp/n64gal_host_embed_example
/tmp/n64gal_host_embed_example
ctest --test-dir build --output-on-failure -R runtime_session
ctest --test-dir build --output-on-failure -R example_host_embed
```

### DoD

- [x] 宿主无需了解 ISA 私有后端实现
- [x] SDK 文档包含最小接入与错误处理示例
- [ ] 版本协商表与 release 文档同步

### 回退策略

若示例宿主维护成本过高，先保留单一 C 示例，但版本协商文档不得缺席。

---

## ISSUE-025 扩展清单、兼容矩阵与生态治理

- Labels: `type:docs`, `type:infra`, `priority:P2`
- Milestone: `M4-engine-ecosystem`
- Depends on: `ISSUE-012`, `ISSUE-022`, `ISSUE-024`
- Blocking: 生态发布与第三方扩展接入

### 目标

先把生态治理规则写清楚，再逐步开放导入器/导出器/校验器等扩展，避免后续 ABI 和格式失控。

### 交付物

- `docs/extension-manifest.md`
- `docs/compat-matrix.md`
- `docs/ecosystem-governance.md`

### 任务清单

- [ ] 定义扩展 `manifest` 字段与版本范围
- [ ] 明确“文件级扩展优先、运行时插件后置”的策略
- [ ] 建立 release 兼容矩阵模板
- [ ] 建立生态变更审查规则（格式变更/版本变更/迁移器要求）

### 验收命令

```bash
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S1,S3,S10
```

### DoD

- [ ] 每个发布版本都有兼容矩阵
- [ ] 新扩展接入必须声明能力位与版本范围
- [ ] 格式变更必须绑定迁移命令或拒绝加载说明

### 回退策略

生态尚未开放时至少先维护官方兼容矩阵与治理规则，不直接承诺第三方插件 ABI 稳定。

---
