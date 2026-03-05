# N64GAL Issue Backlog (Refined)

基于 [`dream.md`](./dream.md) `v1.6-executable-c89-single-api` 整理。  
目标：将白皮书转为可直接执行的 GitHub issue 工单体系。

## 0. 适用原则

1. 单一前后端 API：`vn_backend.h` 是唯一跨架构契约。
2. 跨架构迁移：只新增后端实现并重编译，前端源码零改动。
3. C89 强约束：运行时代码禁止 C99/C11 特性。
4. 所有性能优化都必须可开关、可回退、可对照验证。

## 1. Milestone 规划

| Milestone | 周期 | 目标 |
|---|---|---|
| `M0-core-scalar` | W1-W2 | 前后端分离骨架 + scalar 基线 |
| `M1-amd64-avx2` | W3-W6 | amd64 AVX2 首发性能 |
| `M2-arm64-neon` | W7-W10 | arm64 NEON 平台化 |
| `M3-riscv64-rvv` | W11-W14 | riscv64 RVV 扩展 |

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

- [ ] `vnpak` 头表解析
- [ ] 图像转换（PNG -> RGBA16/CI8/IA8）
- [ ] 脚本编译（txt -> bin）
- [ ] CRC32 与 `manifest.json`

### 验收命令

```bash
python3 tools/packer/main.py --input assets/src --output assets/demo/demo.vnpak
./build/vn_player --backend=scalar --pak assets/demo/demo.vnpak
```

### DoD

- [ ] Demo 资源可加载
- [ ] 打包输出可复现（同输入同哈希）
- [ ] 错误能映射到统一错误码

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

- [ ] 固定场景 `S0/S1/S2/S3`
- [ ] 输出字段：`scene,frame,frame_ms,vm_ms,build_ms,raster_ms,audio_ms,rss_mb`
- [ ] 热身 20 秒规则
- [ ] 报告模板（设备/参数/版本）

### 验收命令

```bash
./tests/perf/run_perf.sh --backend scalar --scenes S0,S1,S2,S3
```

### DoD

- [ ] 每场景输出 CSV
- [ ] 可计算 p95
- [ ] 可重复执行并复现实验结论

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

- [ ] `fill/blend/tex/combine` AVX2 核心算子
- [ ] `--backend=avx2` 强制切换
- [ ] amd64 自动优先选择 `avx2`
- [ ] 与 scalar 对照测试

### 验收命令

```bash
./build/vn_player --backend=avx2 --scene S0
./tests/perf/run_perf.sh --backend avx2 --scenes S0,S1,S2,S3
```

### DoD

- [ ] `R0(600x800)` 下 `S0-S3 >=60fps`（x86_64）
- [ ] 差异图误差 <1%
- [ ] 回退到 scalar 功能无回归

### 回退策略

AVX2 不稳定时默认切回 scalar，同时保留 `--backend=avx2` 便于诊断。

---

## ISSUE-008 P0 性能四件套

- Labels: `type:perf`, `priority:P0`
- Milestone: `M1-amd64-avx2`
- Depends on: `ISSUE-007`
- Blocking: `ISSUE-009`, `ISSUE-012`

### 目标

落实白皮书 P0 优化：静态帧短路、Dirty-Tile、命令缓存、动态分辨率。

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
./tests/perf/run_perf.sh --backend avx2 --scenes S0,S1,S2,S3 --compare-baseline
```

### DoD

- [ ] `S0 p95 <= 12.0ms`
- [ ] `S1 p95 <= 14.0ms`
- [ ] `S2 p95 <= 15.0ms`
- [ ] `S3 p95 <= 16.2ms`
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

- [ ] NEON 核心算子实现
- [ ] arm64 默认后端选择
- [ ] Zero 2W 场景跑测

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

- [ ] Job A: x86_64 + scalar
- [ ] Job B: x86_64 + avx2
- [ ] Job C: arm64 + scalar (cross)
- [ ] Job D: arm64 + neon (cross)
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

- [ ] RVV 后端实现并接入注册
- [ ] riscv64 启动选择与失败回退
- [ ] 工具链版本固定与构建说明

### 验收命令

```bash
./build/vn_player --backend=rvv --scene S0
./tests/perf/run_perf.sh --backend rvv --scenes S0,S1,S2,S3
```

### DoD

- [ ] riscv64：`S0-S2 >=35fps`, `S3 >=30fps`
- [ ] rvv 与 scalar 差异图误差 <1%
- [ ] rvv 初始化失败自动切回 scalar

### 回退策略

RVV 不稳定时先发布 `riscv64 + scalar`，RVV 标记实验特性。

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

## 5. 可选追加 Issue（建议）

- `ISSUE-013`：`vnsave v1` 存档迁移器
- `ISSUE-014`：错误码与日志可观测性升级（统一 trace id）
- `ISSUE-015`：WebAssembly 实验性后端（非主线阻塞）
