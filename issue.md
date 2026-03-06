# N64GAL Issue Backlog (Refined)

基于 [`dream.md`](./dream.md) `v1.6-executable-c89-single-api` 整理。  
目标：将白皮书转为可直接执行的 GitHub issue 工单体系。

## 0.1 落地状态快照（2026-03-06）

### 已完成（代码已合入 main）

1. `ISSUE-001`：前后端单一 API 基本冻结（`vn_backend.h` + 注册/选择链）
2. `ISSUE-002`：Frontend 输出 `VNRenderOp[]`，且 VM -> IR 主路径已接通
3. `ISSUE-003`：`scalar` 最小可运行闭环（`600x800`、`S0-S3`）
4. `ISSUE-004`：`vnpak v2`（CRC32）+ `manifest.json` + 资源一致性校验 + `PNG -> RGBA16/CI8/IA8` 已落地
5. `ISSUE-005`：`S0-S3` perf CSV + 热身窗口 + p95 汇总链路已落地
6. `ISSUE-006`（部分）：C89 门禁脚本 + 头文件独立编译检查

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
6. `ISSUE-007`（进行中）：`avx2` 后端已从桩实现升级为可运行路径（`CLEAR/SPRITE/TEXT/FADE`），`test_runtime_golden` 已固化 `S0-S3 @ 600x800` 标量 golden CRC，并对支持的 SIMD 后端做逐像素 exact compare；失败时导出 `expected/actual/diff` PPM
7. 文档化：`docs/api/README.md`、`docs/api/runtime.md`、`docs/api/backend.md`、`docs/api/pack.md`、`docs/platform-matrix.md` 已建立，后续随 API 变更持续维护

### 下一步（短周期）

1. `ISSUE-007` 收口：补差异图误差阈值与可视化 golden 对照
2. `ISSUE-010` 前置准备：后端一致性基线数据沉淀（scalar 对照）
3. `ISSUE-008` 前置：建立性能回归基线门限文件
4. `ISSUE-014` 跟进：观察 Windows x64/arm64 上 `build_config.h` + MSVC AVX2 路径的 CI 结果
5. `ISSUE-014` 收口：`linux-riscv64-qemu-scalar` 已接入并待远端稳定观察，继续推进 `qemu-rvv` 告警链
6. `ISSUE-011` 细化：把 `riscv64` 验证链拆成 `cross-build -> qemu-scalar -> qemu-rvv -> native`
7. `ISSUE-011` 跟进：为 RVV 融合与 opaque 直写补 `perf_compare` 证据，并继续推进 `alpha<255` 单循环化
8. `M4-engine-ecosystem` 预研：先冻结模板/CLI/宿主 SDK/预览协议边界，避免工具链各自长歪

## 0. 适用原则

1. 单一前后端 API：`vn_backend.h` 是唯一跨架构契约。
2. 跨架构迁移：只新增后端实现并重编译，前端源码零改动。
3. C89 强约束：运行时代码禁止 C99/C11 特性。
4. 所有性能优化都必须可开关、可回退、可对照验证。
5. 性能优先：在不破坏正确性和可维护性的前提下，尽可能提升更多性能（优先优化热路径）。

## 1. Milestone 规划

| Milestone | 周期 | 目标 |
|---|---|---|
| `M0-core-scalar` | W1-W2 | 前后端分离骨架 + scalar 基线（Linux/Windows） |
| `M1-amd64-avx2` | W3-W6 | amd64 AVX2 首发性能（Linux/Windows） |
| `M2-arm64-neon` | W7-W10 | arm64 NEON 平台化（Linux/Windows） |
| `M3-riscv64-rvv` | W11-W14 | riscv64 RVV 扩展 |
| `M4-engine-ecosystem` | W15-W20 | 模板、工具链、宿主 SDK、预览协议与兼容矩阵 |

### 1.1 平台兼容矩阵（必须覆盖）

| 平台 | 后端优先级 | 当前状态 |
|---|---|---|
| Linux x64 | `avx2` -> `scalar` | 已完成（CI 全绿，perf smoke 已接入） |
| Windows x64 | `avx2` -> `scalar` | 已完成（MSVC x64 CI 全绿） |
| Linux arm64 | `neon` -> `scalar` | 已完成（arm64 Linux CI 全绿） |
| Windows arm64 | `neon` -> `scalar` | 已完成（Windows arm64 CI 全绿） |
| Linux riscv64 | `rvv` -> `scalar` | 进行中（`rvv` 后端最小路径已接入，交叉编译与 `qemu-user` 冒烟通过，待原生验证） |

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
ISSUE-001 + ISSUE-017 -> ISSUE-024
ISSUE-004 + ISSUE-012 -> ISSUE-025
```

## 4. Issue 摘要

| ID | 标题 | Milestone | Priority | 估时 |
|---|---|---|---|---|
| ISSUE-001 | 冻结单一前后端 API（`vn_backend.h`） | M0 | P0 | 2d |
| ISSUE-002 | Frontend 输出 `VNRenderOp[]` | M0 | P0 | 2d |
| ISSUE-003 | `scalar` 后端最小可运行（600x800 + S0） | M0 | P0 | 3d |
| ISSUE-004 | 资源链路最小可用（`vnpak`+脚本编译） | M0 | P0 | 2d |
| ISSUE-005 | 基准场景 S0-S3 与 perf 采样 | M0 | P0 | 2d |
| ISSUE-006 | C89 门禁与头文件独立编译检查 | M0 | P0 | 1d |
| ISSUE-007 | `avx2` 后端实现与运行时切换 | M1 | P0 | 4d |
| ISSUE-008 | P0 性能项四件套 | M1 | P0 | 4d |
| ISSUE-009 | `neon` 后端实现与 arm64 默认启用 | M2 | P1 | 4d |
| ISSUE-010 | 后端一致性测试与 CI 矩阵 | M2 | P1 | 3d |
| ISSUE-011 | `rvv` 后端实现与回退链 | M3 | P2 | 5d |
| ISSUE-012 | 发布收口（文档/报告/迁移指南） | M3 | P1 | 2d |
| ISSUE-013 | Linux/Windows 平台抽象层与系统适配 | M1 | P0 | 3d |
| ISSUE-014 | x64/arm64 + Linux/Windows + riscv64 Linux CI 矩阵 | M2 | P0 | 4d |
| ISSUE-021 | 样例工程与模板仓骨架 | M4 | P1 | 2d |
| ISSUE-022 | Creator Toolchain 聚合（validate/migrate/probe） | M4 | P1 | 3d |
| ISSUE-023 | 预览协议与无 GUI 预览进程 | M4 | P2 | 4d |
| ISSUE-024 | 宿主 SDK 与版本协商文档 | M4 | P1 | 3d |
| ISSUE-025 | 扩展清单、兼容矩阵与生态治理 | M4 | P2 | 3d |

---

## 5. 开工看板（W1 可直接执行）

### 5.1 今天就做（Day0）

1. 创建 Milestone：`M0-core-scalar` 到 `M3-riscv64-rvv`
2. 创建标签：`type:*`、`arch:*`、`priority:*`
3. 新建 Issue：至少先建 `ISSUE-001` 到 `ISSUE-006`
4. 建立项目看板列：`Todo / In Progress / Review / Done / Blocked`
5. 把 `ISSUE-001`、`ISSUE-002`、`ISSUE-003` 拖入 `In Progress`

### 5.2 首批并行分配（建议）

| 角色 | 领取 Issue | 目标产物 |
|---|---|---|
| Owner-A（Frontend） | ISSUE-002 | `build_render_ops` 可跑通 |
| Owner-B（Backend） | ISSUE-001 + ISSUE-003 | `vn_backend.h` + scalar 首帧 |
| Owner-C（Tools） | ISSUE-004 | `demo.vnpak` 可生成 |
| Owner-D（QA/CI） | ISSUE-005 + ISSUE-006 | `perf.csv` + C89 门禁 |

### 5.3 首周 PR 顺序（建议）

1. `PR-001`：接口冻结（ISSUE-001）
2. `PR-002`：Frontend IR 输出（ISSUE-002）
3. `PR-003`：Scalar 后端（ISSUE-003）
4. `PR-004`：资源链路（ISSUE-004）
5. `PR-005`：Perf + C89 门禁（ISSUE-005/006）
6. `PR-006`：AVX2 原型（ISSUE-007）

### 5.4 每日收口标准

每天结束前必须满足：

1. 至少 1 条 issue 状态前进（Todo -> In Progress 或 In Progress -> Review）
2. 至少 1 条可复现命令写入 issue 评论
3. Blocker 在当日明确 owner 和截止时间

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

## ISSUE-005 基准场景 S0-S3 与 perf 采样

- Labels: `type:test`, `type:infra`, `priority:P0`
- Milestone: `M0-core-scalar`
- Depends on: `ISSUE-003`
- Blocking: `ISSUE-007`, `ISSUE-008`, `ISSUE-009`, `ISSUE-011`

### 目标

统一性能评估输入，避免“不同场景得出不同结论”。

### 交付物

- `tests/perf/scenes/*`
- `tests/perf/run_perf.sh`（或等效）
- `tests/perf/perf_*.csv`

### 任务清单

- [x] 固定场景 `S0/S1/S2/S3`
- [x] 输出字段：`scene,frame,frame_ms,vm_ms,build_ms,raster_ms,audio_ms,rss_mb`
- [x] 热身 20 秒规则
- [x] 报告模板（设备/参数/版本）
- [x] baseline/candidate 对比脚本与 markdown 汇总（`run_perf_compare.sh` / `compare_perf.sh`）
- [x] Linux x64 CI perf artifact（`scalar vs avx2`）

### 验收命令

```bash
./tests/perf/run_perf.sh --backend scalar --scenes S0,S1,S2,S3
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
- 差异图对照数据：`tests/golden/avx2_vs_scalar/*`

### 任务清单

- [x] `fill` 核心算子（AVX2 向量写入）与 `blend` 标量回退路径
- [x] 覆盖 `VN_OP_CLEAR/VN_OP_SPRITE/VN_OP_TEXT/VN_OP_FADE` 执行链路
- [x] `--backend=avx2` 强制切换（CPU 支持时使用 avx2，不支持时回退 scalar）
- [x] amd64 自动优先选择 `avx2`
- [x] `tex/combine` 真采样路径（共享 `pixel_pipeline`，`scalar/avx2` 同语义）
- [x] 纹理坐标热路径优化：UV LUT（减少逐像素除法）
- [x] 与 scalar 一致性对照测试（`test_backend_consistency` CRC 对照）
- [x] 运行时 golden CRC + 像素级基线（`test_runtime_golden`: `S0-S3 @ 600x800`，支持后端逐像素 exact compare，失败导出 PPM）
- [ ] golden 图差异测试（误差阈值）

### 验收命令

```bash
./build/vn_player --backend=avx2 --scene S0
./tests/perf/run_perf.sh --backend avx2 --scenes S0,S1,S2,S3
```

### DoD

- [ ] `R0(600x800)` 下 `S0-S3 >=60fps`（x86_64）
- [ ] 差异图误差 <1%
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

落实白皮书 P0 优化：静态帧短路、Dirty-Tile、命令缓存、动态分辨率；在正确性前提下尽可能挖掘可获得性能收益。

### 交付物

- 性能开关实现
- 性能报告 `docs/perf-report.md`

### 任务清单

- [ ] 静态帧短路（state hash）
- [ ] Dirty-Tile 增量渲染
- [ ] 命令缓存（LRU）
- [ ] 动态分辨率 `R0/R1/R2` 自动升降档
- [ ] 运行时开关与日志

### 验收命令

```bash
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S0,S1,S2,S3
```

### DoD

- [ ] `S0 p95 <= 12.0ms`
- [ ] `S1 p95 <= 14.0ms`
- [ ] `S2 p95 <= 15.0ms`
- [ ] `S3 p95 <= 16.2ms`
- [ ] 针对主要热路径（raster/build/vm）至少各落地 1 项可量化优化
- [ ] 优化均可单独关闭并回退

### 回退策略

任一优化退化超过 10% 时默认关闭该优化。

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
- [ ] NEON `blend/tex/combine` 核心算子
- [x] aarch64 交叉编译通过
- [ ] arm64 Linux 原生构建与跑测
- [ ] arm64 Windows 原生构建验证

### 验收命令

```bash
./build/vn_player --backend=neon --scene S0
./tests/perf/run_perf.sh --backend neon --scenes S0,S1,S2,S3
```

### DoD

- [ ] Zero 2W：`S0-S2 >=45fps`, `S3 >=40fps`
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
- [ ] Job A: x86_64 + scalar
- [ ] Job B: x86_64 + avx2
- [ ] Job C: arm64 + scalar (cross/native)
- [ ] Job D: arm64 + neon (cross/native)
- [ ] Job E: riscv64 + scalar (cross)
- [ ] Job F: riscv64 + rvv（M3 前告警）

### 验收命令

```bash
ctest --test-dir build --output-on-failure -R backend_consistency
```

### DoD

- [ ] M1 起 Job A/B 阻塞
- [ ] M2 起 Job C/D 阻塞
- [ ] M3 前 Job F 告警，M3 后转阻塞
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
- [ ] RVV `alpha<255` 专用融合路径：`sample -> combine -> blend/store` 单循环化（下一性能热点）
- [ ] RVV `8-bit` UV LUT / seed 热路径继续瘦身，进一步降低采样带宽
- [ ] RVV 融合优化前后补 `perf_compare` 证据（至少 `rvv before/after` 一组 markdown 报告）
- [ ] 将 `qemu-rvv` 从告警提升到阻塞前的稳定性采样
- [ ] riscv64 Linux 原生运行验证
- [ ] 工具链版本固定与构建说明（`docs/riscv-toolchain.md` 持续维护）

### 验收命令

```bash
./scripts/ci/build_riscv64_cross.sh
./scripts/ci/run_riscv64_qemu_suite.sh --skip-rvv
./scripts/ci/run_riscv64_qemu_suite.sh --require-rvv
./tests/perf/run_perf.sh --backend rvv --scenes S0,S1,S2,S3
```

### DoD

- [ ] `riscv64` 验证链分层落地：`cross-build -> qemu-scalar -> qemu-rvv -> native-riscv64`
- [ ] riscv64：`S0-S2 >=35fps`, `S3 >=30fps`
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
./tests/perf/run_perf.sh --backend auto --scenes S0,S1,S2,S3
ctest --test-dir build --output-on-failure
```

### DoD

- [ ] `S0-S3` 全通过且无 `ERROR` 级日志
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

- [ ] Linux x64 构建与单测通过
- [ ] Windows x64 构建与单测通过
- [ ] 不引入 Frontend API 破坏性变更

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
- 平台矩阵结果汇总文档

### 任务清单

- [x] `.github/workflows/ci-matrix.yml` 已接入 x64/arm64/riscv64 组合
- [x] Job A: Linux x64（scalar + avx2）
- [x] Job B: Windows x64（scalar + avx2）
- [x] Job C: Linux arm64（scalar + neon）
- [x] Job D: Windows arm64（scalar + neon）
- [x] Job E0: Linux riscv64 cross-build（交叉构建）
- [x] Job E1: Linux riscv64 qemu-scalar（`scalar`/回退链/pack/runtime，阻塞）
- [x] Job F0: Linux riscv64 qemu-rvv（`rvv` 冒烟，`M3` 前告警，workflow 已接入 `continue-on-error`）
- [ ] Job F1: Linux riscv64 native-nightly（真机功能 + perf）
- [ ] 失败回退路径验证（每个平台至少 1 例）

### 验收命令

```bash
ctest --test-dir build --output-on-failure
./scripts/ci/build_riscv64_cross.sh
./scripts/ci/run_riscv64_qemu_suite.sh --skip-rvv
./scripts/ci/run_riscv64_qemu_suite.sh --require-rvv
```

### DoD

- [x] x64/arm64 四象限 job 全绿
- [ ] `linux-riscv64-cross` 阻塞并稳定
- [ ] `linux-riscv64-qemu-scalar` 转阻塞
- [ ] `linux-riscv64-qemu-rvv` 在 `M3` 前为告警，M3 后转阻塞
- [ ] 每个平台均有回退链验证日志
- [ ] CI 失败阻塞 main 合并

### 回退策略

`riscv64` 必须拆成 `cross-build`、`qemu-scalar`、`qemu-rvv`、`native-nightly` 四层；单层不稳定时只允许降级当前层级，不允许把“已知不可运行”的更高层结果冒充主线稳定性证据。

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
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S0,S3
./scripts/check_c89.sh
```

### DoD

- [ ] Creator Toolchain 各命令帮助文本完整
- [ ] 至少 1 条迁移路径可跑通
- [ ] 至少 1 条 golden/perf 报告链产出 markdown artifact

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
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S0,S3
```

### DoD

- [ ] 每个发布版本都有兼容矩阵
- [ ] 新扩展接入必须声明能力位与版本范围
- [ ] 格式变更必须绑定迁移命令或拒绝加载说明

### 回退策略

生态尚未开放时至少先维护官方兼容矩阵与治理规则，不直接承诺第三方插件 ABI 稳定。

---

## 6. 可选追加 Issue（建议）

- `ISSUE-015`：`vnsave v1` 存档迁移器
- `ISSUE-016`（已完成，2026-03-06）：Runtime Session API（`create/step/destroy`）与宿主循环对接
- `ISSUE-017`：API 文档集维护规范（`docs/api/*` + 变更日志）
- `ISSUE-018`：错误码与日志可观测性升级（统一 trace id）
- `ISSUE-019`：WebAssembly 实验性后端（非主线阻塞）
- `ISSUE-020`：riscv64 原生 runner / 开发板接入与 nightly perf 采样
