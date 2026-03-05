# N64GAL Issue Backlog

基于 [`dream.md`](./dream.md) v1.6-executable-c89-single-api 整理。  
目标是把白皮书转成可直接在 GitHub 创建的 issue 清单。

## 使用方式

1. 按“里程碑”先创建 `Milestone`。
2. 按“Issue 列表”逐条建 issue（标题建议保持一致）。
3. 每条 issue 完成后，按对应 DoD 勾选验收项。

## 里程碑定义

- `M0-core-scalar`：前后端分离骨架 + scalar 基线
- `M1-amd64-avx2`：amd64 AVX2 首发性能
- `M2-arm64-neon`：arm64 NEON 平台化
- `M3-riscv64-rvv`：riscv64 RVV 扩展

## 标签建议

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

## Issue 列表（摘要）

| ID | 标题 | Milestone | Priority | 依赖 |
|---|---|---|---|---|
| ISSUE-001 | 冻结单一前后端 API（`vn_backend.h`） | M0 | P0 | - |
| ISSUE-002 | Frontend 输出 `VNRenderOp[]`（替代直接渲染耦合） | M0 | P0 | ISSUE-001 |
| ISSUE-003 | `scalar` 后端最小可运行（600x800 + S0） | M0 | P0 | ISSUE-001, ISSUE-002 |
| ISSUE-004 | 资源链路：`vnpak` + 脚本编译器最小可用 | M0 | P0 | ISSUE-001 |
| ISSUE-005 | 基准场景 S0-S3 与 perf CSV 采样框架 | M0 | P0 | ISSUE-003 |
| ISSUE-006 | C89 门禁与公共头文件独立编译检查 | M0 | P0 | ISSUE-001 |
| ISSUE-007 | `avx2` 后端实现与 `--backend=avx2` 切换 | M1 | P0 | ISSUE-003, ISSUE-005 |
| ISSUE-008 | P0 性能项：静态帧短路 / Dirty-Tile / 命令缓存 / 动态分辨率 | M1 | P0 | ISSUE-007 |
| ISSUE-009 | `neon` 后端实现与 arm64 默认启用 | M2 | P1 | ISSUE-007, ISSUE-008 |
| ISSUE-010 | 后端一致性测试：`scalar` 对照 `avx2/neon/rvv` | M2 | P1 | ISSUE-007, ISSUE-009 |
| ISSUE-011 | `rvv` 后端实现与 riscv64 回退链 | M3 | P2 | ISSUE-010 |
| ISSUE-012 | 发布准备：文档、报告、迁移指南与 v0.1.0 清单 | M3 | P1 | ISSUE-008, ISSUE-009, ISSUE-011 |

---

## ISSUE-001 冻结单一前后端 API（`vn_backend.h`）

- `type:epic` `type:feature` `priority:P0`
- Milestone: `M0-core-scalar`
- 目标：建立唯一跨架构接口，后续新增 ISA 后端不改 Frontend 源码。

### 任务

- [ ] 定义 `VNRenderOp`、`VNBackendCaps`、`VNRenderBackend`
- [ ] 定义 `vn_backend_register` / `vn_backend_select`
- [ ] 定义后端选择策略：`avx2 -> neon -> rvv -> scalar`
- [ ] 增加 `--backend=scalar|avx2|neon|rvv` 参数规范
- [ ] 文档注明“跨架构迁移前端源码零改动”

### DoD

- [ ] Frontend 编译单元不包含 ISA 私有头文件
- [ ] 公共头文件通过 `-std=c89 -pedantic-errors`
- [ ] 接口示例在 `scalar` 后端可跑通

---

## ISSUE-002 Frontend 输出 `VNRenderOp[]`（替代直接渲染耦合）

- `type:feature` `priority:P0`
- Milestone: `M0-core-scalar`
- 依赖：`ISSUE-001`

### 任务

- [ ] 将渲染命令构建统一为 `build_render_ops(...)`
- [ ] 保障脚本 VM 仅操作剧情状态，不直接触达后端私有实现
- [ ] 输出流程适配 `renderer_submit(op_buf, op_count)`

### DoD

- [ ] `TEXT/WAIT/GOTO/END` 脚本路径可完整跑通
- [ ] 切后端时 Frontend 无源码改动
- [ ] 有最小单元测试覆盖 `VNRenderOp` 生成

---

## ISSUE-003 `scalar` 后端最小可运行（600x800 + S0）

- `type:feature` `arch:scalar` `priority:P0`
- Milestone: `M0-core-scalar`
- 依赖：`ISSUE-001`, `ISSUE-002`

### 任务

- [ ] 实现 `scalar` 的 `init/begin/submit/end/shutdown`
- [ ] 支持 `clear/sprite/text/fade` 最小操作集
- [ ] 支持分辨率档位 `R0/R1/R2` 基础切换

### DoD

- [ ] `600x800` 下 `S0` 可运行
- [ ] x86_64 标量路径达到 >=30fps（M0 目标）
- [ ] 后端初始化失败可回退并给出日志

---

## ISSUE-004 资源链路：`vnpak` + 脚本编译器最小可用

- `type:feature` `type:infra` `priority:P0`
- Milestone: `M0-core-scalar`
- 依赖：`ISSUE-001`

### 任务

- [ ] `vnpak` 读取头表与资源索引
- [ ] `PNG -> RGBA16/CI8/IA8` 离线转换
- [ ] `*.vns.txt -> *.vns.bin` 脚本编译
- [ ] 生成 `manifest.json` 与 CRC32

### DoD

- [ ] Demo 资源从外部包加载成功
- [ ] 资源包输出可复现（同输入同哈希）
- [ ] 解析错误能返回统一错误码

---

## ISSUE-005 基准场景 S0-S3 与 perf CSV 采样框架

- `type:test` `type:infra` `priority:P0`
- Milestone: `M0-core-scalar`
- 依赖：`ISSUE-003`

### 任务

- [ ] 建立 `S0/S1/S2/S3` 固定测试场景
- [ ] 采样字段落盘：`scene,frame,frame_ms,vm_ms,build_ms,raster_ms,audio_ms,rss_mb`
- [ ] 统一热身规则：前 20 秒不计入统计

### DoD

- [ ] 输出 `perf_<scene>.csv`
- [ ] 可计算 `frame_time_p95`
- [ ] 报告中包含设备、命令行、版本号

---

## ISSUE-006 C89 门禁与公共头文件独立编译检查

- `type:infra` `priority:P0`
- Milestone: `M0-core-scalar`
- 依赖：`ISSUE-001`

### 任务

- [ ] 接入 `-std=c89 -pedantic-errors -Wall -Wextra -Werror`
- [ ] 扫描禁用 C99/C11 特征
- [ ] `include/*.h` 单独编译检查

### DoD

- [ ] Debug/Release 构建通过
- [ ] 无 `stdint.h/stdbool.h` 依赖
- [ ] 无 `for (int i=...)` 与 `//` 注释

---

## ISSUE-007 `avx2` 后端实现与 `--backend=avx2` 切换

- `type:feature` `type:perf` `arch:avx2` `priority:P0`
- Milestone: `M1-amd64-avx2`
- 依赖：`ISSUE-003`, `ISSUE-005`

### 任务

- [ ] 实现 `avx2` 后端核心算子（fill/blend/tex/combine）
- [ ] 接入运行时后端选择与强制参数
- [ ] 默认 amd64 优先启用 `avx2`

### DoD

- [ ] `R0(600x800)` 下 `S0-S3` 达到 >=60fps（x86_64）
- [ ] 与 `scalar` 差异图误差 <1%
- [ ] 切到 `scalar` 时功能不回归

---

## ISSUE-008 P0 性能项：静态帧短路 / Dirty-Tile / 命令缓存 / 动态分辨率

- `type:perf` `priority:P0`
- Milestone: `M1-amd64-avx2`
- 依赖：`ISSUE-007`

### 任务

- [ ] 静态帧短路（state hash 命中直接复用）
- [ ] Dirty-Tile 增量渲染与全屏回退阈值
- [ ] 命令缓存（LRU）
- [ ] 动态分辨率 `R0/R1/R2` 自动升降档

### DoD

- [ ] `S0`: `p95 <= 12.0ms`
- [ ] `S1`: `p95 <= 14.0ms`
- [ ] `S2`: `p95 <= 15.0ms`
- [ ] `S3`: `p95 <= 16.2ms`
- [ ] 所有优化可通过运行时开关降级

---

## ISSUE-009 `neon` 后端实现与 arm64 默认启用

- `type:feature` `type:perf` `arch:neon` `priority:P1`
- Milestone: `M2-arm64-neon`
- 依赖：`ISSUE-007`, `ISSUE-008`

### 任务

- [ ] 实现 `neon` 后端与 `scalar` 同接口
- [ ] arm64 平台后端优先选择 `neon`
- [ ] 完成 Zero 2W 的场景性能采样

### DoD

- [ ] Zero 2W：`S0-S2 >=45fps`，`S3 >=40fps`（自适应档位）
- [ ] 与 `scalar` 差异图误差 <1%
- [ ] 内存峰值 <=64MB

---

## ISSUE-010 后端一致性测试：`scalar` 对照 `avx2/neon/rvv`

- `type:test` `type:infra` `priority:P1`
- Milestone: `M2-arm64-neon`
- 依赖：`ISSUE-007`, `ISSUE-009`

### 任务

- [ ] 建立后端一致性自动测试（像素对比）
- [ ] 建立 CI 矩阵 Job A~F
- [ ] 定义阻塞/告警策略随里程碑升级

### DoD

- [ ] M1 起 Job A/B 阻塞
- [ ] M2 起 Job C/D 阻塞
- [ ] M3 前 Job F 告警，M3 后转阻塞
- [ ] 任一后端失败可自动验证 `scalar` 回退

---

## ISSUE-011 `rvv` 后端实现与 riscv64 回退链

- `type:feature` `type:perf` `arch:rvv` `priority:P2`
- Milestone: `M3-riscv64-rvv`
- 依赖：`ISSUE-010`

### 任务

- [ ] 实现 `rvv` 后端并接入 `vn_backend_register`
- [ ] riscv64 平台启动选择与回退链测试
- [ ] 工具链版本固定与构建文档整理

### DoD

- [ ] riscv64：`S0-S2 >=35fps`，`S3 >=30fps`（自适应档位）
- [ ] `rvv` 与 `scalar` 差异图误差 <1%
- [ ] `rvv` 初始化失败自动切回 `scalar`

---

## ISSUE-012 发布准备：文档、报告、迁移指南与 v0.1.0 清单

- `type:docs` `type:infra` `priority:P1`
- Milestone: `M3-riscv64-rvv`
- 依赖：`ISSUE-008`, `ISSUE-009`, `ISSUE-011`

### 任务

- [ ] 整理 `README`、`docs/perf-report.md`、`docs/migration.md`
- [ ] 输出 `后端移植指南`（新增 ISA 仅实现 `vn_backend.h`）
- [ ] 准备发布产物（可执行、`demo.vnpak`、许可证、版本）

### DoD

- [ ] `S0-S3` 全通过且无 `ERROR` 级日志
- [ ] C89 门禁与单测 100% 通过
- [ ] Demo 连续 15 分钟无崩溃
- [ ] 发布清单全部完成

---

## 可选追加 Issue（建议）

- `ISSUE-013`：存档格式 `vnsave v1` 兼容迁移器
- `ISSUE-014`：错误码全链路埋点与日志可观测性
- `ISSUE-015`：WebAssembly 实验性后端（非阻塞主线）

