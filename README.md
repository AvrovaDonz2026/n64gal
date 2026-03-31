# N64GAL

N64GAL 是一个面向 Galgame/VN 的实验性引擎原型，核心目标是：

1. 前后端分离。
2. 单一前后端 API 契约。
3. C89 严格兼容。
4. 便于跨架构后端扩展（amd64/AVX2 -> arm64/NEON -> riscv64/RVV）。

当前代码以“库优先”方式组织：核心能力通过 `vn_runtime.h` 暴露，预览协议入口通过 `vn_preview.h` 暴露，`vn_player` 仅作为可选 CLI 包装。

## 代码优先说明

这个仓库当前应按“代码与测试优先”理解：文档可能阶段性落后于实现。

若文档、README 与实现冲突，请优先以这些位置为准：

1. `include/*.h` 的公开头文件
2. `examples/host-embed/*` 的最小接入示例
3. `tests/unit/test_runtime_api.c` 与 `tests/unit/test_runtime_session.c`
4. `tests/integration/test_preview_protocol.c`
5. `tools/toolchain.py`

当前代码里已经稳定落地并有回归覆盖的主路径包括：

1. `vn_runtime_run(...)`
2. Session API：`create/step/is_done/set_choice/inject_input/destroy`
3. Session snapshot API：`capture_snapshot/create_from_snapshot`
4. Session file save/load API：`save_to_file/load_from_file`
5. `vn_previewd` / `vn_preview_run_cli(...)`
6. `migrate-vnsave`
7. `probe-vnsave`

当前 `vn_runtime.h` 也已经提供 `vn_runtime_query_build_info(...)`，可直接查询当前 build 的 runtime/preview/pack/save 版本边界与 host 元信息。

当前 `scene_name` 也不是任意字符串，而是代码里明确解析的固定集合：`S0/S1/S2/S3/S10`；未知值会被拒绝。

## 项目状态（2026-03-15）

- 当前对外版本状态：`v0.1.0-alpha` 已发布
- 当前后续目标：`v0.1.0-mvp`
- 当前 `v1.0.0` 范围决策：**先不包含 RVV / riscv64 native 承诺**

1. 已完成：
   - `scalar` 后端最小闭环（默认 `600x800`，场景 `S0-S3`，并新增更重的 `S10` perf sample）。
   - Frontend 输出统一 `VNRenderOp[]`。
   - `vn_runtime_run(config, result)` 结构化运行入口。
   - Session API：`create/step/is_done/set_choice/inject_input/destroy`。
   - `vn_previewd` 与 `preview protocol v1` 已落地，可供 editor/CI 复用。
   - `Creator Toolchain` 已形成可执行门禁层：`validate-all`、`release-gate`、`release-soak`、`release-preflight`、`release-host-sdk-smoke`、`release-platform-evidence`、`release-preview-evidence`、`release-bundle`、`release-report`、`release-publish-map`、`release-export`、`validate-release-remote-state` 与 `release-remote-summary` 已落地；当前 `1.0.0` 的主要 release contract / API / platform / perf / template / migration 文档面都已有对应 validator 和统一入口，release-facing 证据也开始统一收口到 markdown/json 双摘要、canonical release spec 与远端 prerelease 对齐摘要。
2. 进行中:
   - Golden 基线收口：`test_runtime_golden` 已固化 `S0/S1/S2/S3/S10 @ 600x800` 标量 CRC；支持的 SIMD 后端按 `mismatch_percent < 1%` 且 `max_channel_diff <= 8` 判定，并在出现差异或 CRC 异常时导出 `expected/actual/diff` PPM 与 `summary.txt`。
   - `ISSUE-008` 已继续落地：`qemu-rvv` revision compare soft gate 已接到 perf workflow；Runtime `VN_RUNTIME_PERF_FRAME_REUSE` 静态帧短路与 `VNRenderOp[]` LRU 命令缓存均已接入，前者在稳定状态下直接复用 framebuffer，后者继续按当前帧回写 `SPRITE/FADE` 动态字段；`Dirty-Tile` 已继续推进到第二阶段：`VN_RUNTIME_PERF_DIRTY_TILE`、`VNRunResult` 统计字段、CLI 开关与内部 dirty planner 已接入 runtime/preview；共享 `renderer_submit_dirty(...)` / `submit_ops_dirty(...)` 契约也已落地，`scalar`、`avx2`、`neon`、`rvv` 都已实现 clip 提交；runtime 现在还会在“已知必整帧”场景同时跳过 planner build 与 full-redraw commit 的 bounds 计算，并在重新回到可增量帧时惰性重建 `prev_bounds`。其中 `rvv` 已按 `qemu-first` 路线补齐 smoke 验证。动态分辨率最小 runtime slice 也已落地：`VN_RUNTIME_PERF_DYNAMIC_RESOLUTION`、`--perf-dynamic-resolution=<on|off>`、`VNRunResult.render_width/render_height/dynamic_resolution_*` 与 preview `final_state` 已可观测，当前策略保持默认 `off`，先通过 on/off compare 累积证据再决定是否默认开启。
   - x64/arm64 + Linux/Windows CI 矩阵已全绿。
   - `neon` 最小后端已接入，arm64 Linux/Windows CI 已通过；最近一轮又补了 uniform fade 常量外提、palette-row no-stack 写回、更保守的 row-palette 启发式，并把 textured-row 的 `row params + local sample/combine/blend` 收回 TU 内，随后继续接上 4-lane NEON `sample/hash -> combine` chunk 内核；当前 direct row opaque/blend、row-palette build，以及 textured alpha 的 packed-channel 向量 blend 都已共用同一条主线，row 级 `seed/checker/base_rgb` 常量也已前折叠。本地已再次通过 `qemu-aarch64` 的 `fallback / dirty_submit / runtime_golden` 复核；下一步转向 `row-palette gather/apply` 降开销与更宽 chunk 评估；其中 `u_lut` lane-load 已先切到 `vld1_u8 + vmovl` 并补 `u_lut` 专用尾部 padding（`v_lut` 侧 `tail_pad=0` 以避免越界写），alpha repeated-v8 row-cache 构建也已改成 4-lane chunk helper，`RB/G` gather 已合并到单 helper，而 `row-palette` 的 full/RB/G palette gather 以及 direct textured row 的 `u` 索引 chunk 都开始复用 `u_lut_u32` 展开缓存与单次 `vld1q_u32` 索引载入；这轮又把 `u_lut` 与 `u_lut_u32` 的生成收口到同一遍循环里，把 alpha row-palette 全面切到“只生成 `RB/G`，不再生成 full palette”，删除了冗余的 `g_neon_u_lut(u8)` 缓存，并把 row-palette / direct row / palette build / fill-fade 的几个 4px 热循环统一改成 8px unroll；最新一轮又新增了 `sample_combine` 预加载常量版 helper，让 hottest loop 不再每个 chunk 反复 `vld1q` 行常量，同时把 non-palette direct row 的 `params` 初始化也收口到按 `v8` 复用。
   - `rvv` 最小后端已接入，`tex/hash -> combine -> alpha` 热路径已向量化，`sample -> combine` 已融合，且 `alpha=255` / `alpha<255` 都已收口到更短的写回路径；UV LUT 已压到 8-bit，`seed/checker` 常量和基础偏置也已前折叠。`riscv64` 的 `cross-build / qemu-scalar / qemu-rvv / qemu perf artifact` 已验证，当前按 `qemu-first` 收口；原生 `native-riscv64/RVV` 设备未就绪前，原生 nightly 与发布级 perf 证据暂保留为外部阻塞项。
   - 输入抽象层进一步统一（键盘输入与脚本化输入）。

详细路线图见 [issue.md](./issue.md) 与 [dream.md](./dream.md)。`Dirty-Tile` 设计/API 现状与当前第二阶段实现状态见 [docs/api/dirty-tile-draft.md](./docs/api/dirty-tile-draft.md)。当前已入库的 perf 证据见 [docs/perf-rvv-2026-03-06.md](./docs/perf-rvv-2026-03-06.md)、[docs/perf-dirty-2026-03-07.md](./docs/perf-dirty-2026-03-07.md)、[docs/perf-dynres-2026-03-07.md](./docs/perf-dynres-2026-03-07.md)、[docs/perf-windows-x64-2026-03-07.md](./docs/perf-windows-x64-2026-03-07.md) 与 [docs/perf-x64-hosts-2026-03-09.md](./docs/perf-x64-hosts-2026-03-09.md)。当前 `linux-x64` perf smoke / dirty compare 已统一收口到 `S1,S3,S10`；其中 `S10` 作为更重的 perf sample，用于补足主线路径压力。`S0` 在 shipped `frame reuse + op cache` 路径上会被压到约 `0.001ms`，因此只保留在全量 sweep 与 `qemu-rvv` bring-up smoke 中。关于是否引入 `JIT`，当前项目立场已单独写成 [`docs/jit-strategy.md`](./docs/jit-strategy.md)：短期不把它作为主线阻塞项，只接受“`x64-only + VM-only + 默认关闭`”的实验路线。首个对外预发布版本说明见 [`docs/release-v0.1.0-alpha.md`](./docs/release-v0.1.0-alpha.md)，测试与性能证据汇总见 [`docs/release-evidence-v0.1.0-alpha.md`](./docs/release-evidence-v0.1.0-alpha.md)，`v0.1.0-mvp` 当前差距见 [`docs/release-gap-v0.1.0-mvp.md`](./docs/release-gap-v0.1.0-mvp.md)，版本变更摘要见 [`CHANGELOG.md`](./CHANGELOG.md)，`1.0.0` 范围决策见 [`docs/release-roadmap-1.0.0.md`](./docs/release-roadmap-1.0.0.md)，正式版 checklist 见 [`docs/release-checklist-v1.0.0.md`](./docs/release-checklist-v1.0.0.md)，兼容矩阵见 [`docs/compat-matrix.md`](./docs/compat-matrix.md)，生态治理见 [`docs/extension-manifest.md`](./docs/extension-manifest.md) 与 [`docs/ecosystem-governance.md`](./docs/ecosystem-governance.md)，公共错误面见 [`docs/errors.md`](./docs/errors.md)，`vnsave` 版本策略见 [`docs/vnsave-version-policy.md`](./docs/vnsave-version-policy.md)。首个对外发布映射规格见 [`docs/release-publish-v0.1.0-alpha.json`](./docs/release-publish-v0.1.0-alpha.json)。当前 GitHub prerelease：[`v0.1.0-alpha`](https://github.com/AvrovaDonz2026/n64gal/releases/tag/v0.1.0-alpha)。本地正式版 gate 当前也已有统一入口：`python3 tools/toolchain.py validate-all`、`python3 tools/toolchain.py release-gate`、`python3 tools/toolchain.py release-host-sdk-smoke`、`python3 tools/toolchain.py release-platform-evidence`、`python3 tools/toolchain.py release-preview-evidence`、`python3 tools/toolchain.py release-soak`、`python3 tools/toolchain.py release-bundle`、`python3 tools/toolchain.py release-report`、`python3 tools/toolchain.py release-publish-map`、`python3 tools/toolchain.py release-export`、`python3 tools/toolchain.py validate-release-remote-state` 与 `python3 tools/toolchain.py release-remote-summary`。

若只想快速判断“首个 `v1.0.0` 还差什么”，当前执行分层见 [`docs/release-triage-v1.0.0.md`](./docs/release-triage-v1.0.0.md)。

## 目标平台矩阵

当前项目将长期平台支持目标明确为：

1. `x64 + Linux`
2. `x64 + Windows`
3. `arm64 + Linux`
4. `arm64 + Windows`
5. `riscv64 + Linux`

首个 `v1.0.0` 正式版当前只承诺前四项；`riscv64 + Linux` 继续保留在长期路线图中，但按 `post-1.0` 处理。

后端优先级策略：

1. x64: `avx2` -> `scalar`（`avx2_asm` 当前是实验性 force-only 后端，不参与 auto 优先级）
2. arm64: `neon` -> `scalar`
3. riscv64: `rvv` -> `scalar`

## 架构总览

```
VM/Runtime State
    -> Frontend (build_render_ops)
    -> VNRenderOp[] (统一渲染 IR)
    -> Backend (scalar / avx2 / neon / rvv)
```

关键点：

1. Frontend 不依赖任何 ISA 私有实现。
2. 跨架构迁移只需要新增后端并重编译，前端源码不变。
3. 后端选择支持自动与强制模式，失败自动回退到 `scalar`；其中 `avx2_asm` 当前仅支持强制选择，用于实验性 x64 ASM fill 路径验证。

## 仓库结构

```text
include/        对外头文件（backend/runtime/frontend/types/error）
src/core/       运行时、渲染器、包格式、VM
src/frontend/   Frontend -> RenderOp 构建
src/backend/    各后端实现（scalar/avx2/...）
src/tools/      无 GUI 工具入口（如 `vn_previewd`）
tools/scriptc/  场景脚本编译器
tools/packer/   vnpak 打包工具
tools/migrate/  vnsave 迁移工具
tools/probe/    probe/探测工具
tools/validate/ 扩展 manifest 校验工具
tools/previewd/ 预览协议说明与示例
tests/unit/     单元测试
tests/integration/ 预览协议等集成测试
tests/perf/     基准脚本与 CSV 输出
examples/       宿主嵌入与集成示例
templates/      最小内容项目与宿主模板
docs/api/       API 文档
```

## 依赖

1. C 编译器（`cc`，需支持 `-std=c89 -pedantic-errors`）。
2. Python 3（用于脚本编译与打包工具）。
3. Bash（执行仓库脚本）。
4. 可选：CMake（用于标准化构建/测试流程）。

## 资源生成（Demo Pack）

```bash
./tools/scriptc/build_demo_scripts.sh
./tools/packer/make_demo_pack.sh
```

输出默认为：

1. `assets/demo/demo.vnpak`
2. `assets/demo/manifest.json`
3. 可选图片配置：`assets/demo/images/images.json`

当前打包器支持：

1. PNG -> `RGBA16`
2. PNG -> `CI8`
3. PNG -> `IA8`

## 构建

### 方式 A：CMake（推荐）

```bash
cmake -S . -B build
cmake --build build
```

默认构建 `vn_core`、`vn_preview`、`vn_previewd` 与测试目标。
如果需要可执行包装器 `vn_player`：

```bash
cmake -S . -B build -DVN_BUILD_PLAYER=ON
cmake --build build
```

### 方式 B：直接用 `cc`（无 CMake 环境）

```bash
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -Iinclude \
  src/main.c \
  src/core/error.c \
  src/core/backend_registry.c \
  src/core/renderer.c \
  src/core/save.c \
  src/core/vm.c \
  src/core/pack.c \
  src/core/platform.c \
  src/core/runtime_cli.c \
  src/core/runtime_persist.c \
  src/core/dynamic_resolution.c \
  src/frontend/render_ops.c \
  src/frontend/dirty_tiles.c \
  src/backend/common/pixel_pipeline.c \
  src/backend/avx2/avx2_backend.c \
  src/backend/avx2/avx2_fill_fade.c \
  src/backend/avx2/avx2_textured.c \
  src/backend/neon/neon_backend.c \
  src/backend/rvv/rvv_backend.c \
  src/backend/scalar/scalar_backend.c \
  -o /tmp/vn_player
```

如需单独编译无 GUI 预览入口：

```bash
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -Iinclude \
  src/tools/previewd_main.c \
  src/tools/preview_cli.c \
  src/tools/preview_report.c \
  src/core/error.c \
  src/core/backend_registry.c \
  src/core/renderer.c \
  src/core/save.c \
  src/core/vm.c \
  src/core/pack.c \
  src/core/platform.c \
  src/core/runtime_cli.c \
  src/core/runtime_persist.c \
  src/core/dynamic_resolution.c \
  src/frontend/render_ops.c \
  src/frontend/dirty_tiles.c \
  src/backend/common/pixel_pipeline.c \
  src/backend/avx2/avx2_backend.c \
  src/backend/avx2/avx2_fill_fade.c \
  src/backend/avx2/avx2_textured.c \
  src/backend/neon/neon_backend.c \
  src/backend/rvv/rvv_backend.c \
  src/backend/scalar/scalar_backend.c \
  -o /tmp/vn_previewd
```

## 运行示例（CLI）

```bash
/tmp/vn_player --scene=S0 --backend=scalar --resolution=600x800 --frames=120 --dt-ms=16
```

常用参数：

1. `--scene=S0|S1|S2|S3|S10`
2. `--backend=auto|scalar|avx2|avx2_asm|neon|rvv`
3. `--resolution=600x800`
4. `--frames=<N>`
5. `--dt-ms=<N>`
6. `--choice-index=<N>`
7. `--choice-seq=0,1,0`
8. `--trace`
9. `--keyboard`（Linux TTY / Windows console 调试模式，按 `--dt-ms` 节奏推进）
10. `--quiet`
11. `--load-save=<save.vnsave>`（从 runtime session save/load draft 文件恢复继续运行；当前不与 `--scene/--pack/--backend/--frames/...` 混用）
12. `--save-out=<save.vnsave>`（在本次 CLI run 结束后写出 runtime session save/load draft 文件；可与 `--load-save` 组合）
13. `--save-slot=<N>`（配合 `--save-out` 写入 `vnsave` header 的 `slot_id`）
14. `--save-timestamp=<N>`（配合 `--save-out` 写入 `vnsave` header 的 `timestamp_s`）

## 迁移工具（当前最小 save 迁移）

当前已提供最小 `vnsave` 离线迁移入口：

```bash
python3 tools/toolchain.py migrate-vnsave --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave
./build/vnsave_migrate --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave
```

说明：

1. `python3 tools/toolchain.py migrate-vnsave ...` 是推荐入口，会自动编译 helper
2. `./build/vnsave_migrate` 对应 CMake 构建产物
3. `tools/migrate/vnsave_migrate.c` 是源码入口，不是仓库内现成可执行文件

当前范围：

1. 仅支持 repo 内定义的 legacy `v0` 样例迁到 `v1`
2. 未知、过新、损坏或已是 `v1` 的输入都会被结构化拒绝
3. 详细规则见 [docs/api/save.md](./docs/api/save.md) 与 [docs/vnsave-version-policy.md](./docs/vnsave-version-policy.md)

## 校验工具（当前最小 manifest validate）

```bash
python3 tools/validate/validate_manifest.py tests/fixtures/tool_manifest/valid/vnsave_migrate.json
```

当前范围：

1. 校验扩展 manifest 的最小字段、版本范围和能力位
2. 成功/失败都会输出 machine-readable `trace_id`
3. 详细规则见 [docs/extension-manifest.md](./docs/extension-manifest.md) 与 [docs/toolchain.md](./docs/toolchain.md)

## 探测工具（当前最小 vnsave probe）

```bash
python3 tools/toolchain.py probe-vnsave --in tests/fixtures/vnsave/v1/sample.vnsave
./build/vnsave_probe --in tests/fixtures/vnsave/v1/sample.vnsave
```

说明：

1. `python3 tools/toolchain.py probe-vnsave ...` 是推荐入口，会自动编译 helper
2. `./build/vnsave_probe` 对应 CMake 构建产物
3. `tools/probe/vnsave_probe.c` 是源码入口，不是仓库内现成可执行文件

当前范围：

1. 读取 `vnsave` header 并输出结构化探测结果
2. 成功/失败都输出 machine-readable `trace_id`
3. 详细规则见 [docs/api/save.md](./docs/api/save.md) 与 [docs/toolchain.md](./docs/toolchain.md)

## 模板骨架

当前已提供：

1. `templates/minimal-vn/`
2. `templates/host-embed/`

当前模板约定：

1. 模板源码保留在 `assets/` / `src/`
2. 生成产物统一写回 `templates/minimal-vn/build/`

目录说明见 [docs/project-layout.md](./docs/project-layout.md)。

## 统一 Toolchain 入口

```bash
python3 tools/toolchain.py --help
python3 tools/toolchain.py validate-all
python3 tools/toolchain.py validate-release-audit --allow-dirty
python3 tools/toolchain.py validate-release-docs
python3 tools/toolchain.py validate-manifest tests/fixtures/tool_manifest/valid/vnsave_migrate.json
python3 tools/toolchain.py validate-release-contracts
python3 tools/toolchain.py validate-toolchain-contracts
python3 tools/toolchain.py validate-backend-contracts
python3 tools/toolchain.py validate-api-index-contracts
python3 tools/toolchain.py validate-compat-matrix
python3 tools/toolchain.py validate-ecosystem-contracts
python3 tools/toolchain.py validate-error-contracts
python3 tools/toolchain.py validate-host-sdk-contracts
python3 tools/toolchain.py validate-migration-contracts
python3 tools/toolchain.py validate-pack-contracts
python3 tools/toolchain.py validate-platform-contracts
python3 tools/toolchain.py validate-preview-contracts
python3 tools/toolchain.py validate-perf-contracts
python3 tools/toolchain.py validate-porting-contracts
python3 tools/toolchain.py validate-runtime-contracts
python3 tools/toolchain.py validate-save-contracts
python3 tools/toolchain.py validate-template-contracts
python3 tools/toolchain.py probe-vnsave --in tests/fixtures/vnsave/v1/sample.vnsave
python3 tools/toolchain.py probe-trace-summary tests/fixtures/runtime_trace/sample_trace.log
python3 tools/toolchain.py probe-preview --scene=S2 --frames=2 --command=step_frame:2
python3 tools/toolchain.py probe-perf-summary tests/fixtures/perf_summary/sample_perf_summary.csv
python3 tools/toolchain.py probe-perf-compare tests/fixtures/perf_compare/sample_perf_compare.csv
python3 tools/toolchain.py probe-kernel-bench tests/fixtures/kernel_bench/sample_kernel_bench.csv
python3 tools/toolchain.py probe-kernel-compare tests/fixtures/kernel_compare/sample_kernel_compare.csv
python3 tools/toolchain.py migrate-vnsave --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave
bash scripts/release/run_release_gate.sh --allow-dirty --skip-cc-suite
python3 tools/toolchain.py release-gate --allow-dirty --skip-cc-suite
python3 tools/toolchain.py release-gate --allow-dirty --skip-cc-suite --summary-json-out build_release_gate/release_gate_summary.json
python3 tools/toolchain.py release-host-sdk-smoke --summary-out build_release_host_sdk/host_sdk_smoke_summary.md
python3 tools/toolchain.py release-host-sdk-smoke --summary-json-out build_release_host_sdk/host_sdk_smoke_summary.json
python3 tools/toolchain.py release-platform-evidence --out-dir build_release_platform
python3 tools/toolchain.py release-platform-evidence --summary-json-out build_release_platform/platform_evidence_summary.json
python3 tools/toolchain.py release-preview-evidence --summary-out build_release_preview/preview_evidence_summary.md
python3 tools/toolchain.py release-preview-evidence --summary-json-out build_release_preview/preview_evidence_summary.json
bash scripts/release/run_demo_soak.sh --frames-per-scene 600 --scenes S0,S1,S2,S3,S10
python3 tools/toolchain.py release-soak --frames-per-scene 600 --scenes S0,S1,S2,S3,S10
python3 tools/toolchain.py release-soak --summary-json-out build_release_soak/demo_soak_summary.json --frames-per-scene 600 --scenes S0,S1,S2,S3,S10
python3 tools/toolchain.py release-soak --skip-build --runner-bin build_release_soak/vn_player --frames-per-scene 600 --scenes S0,S1,S2,S3,S10
python3 tools/toolchain.py release-preflight --allow-dirty --skip-cc-suite --out-dir build_release_preflight --soak-frames-per-scene 120 --soak-scenes S0,S1,S2,S3,S10 --remote-release-json tests/fixtures/release_api/github_release_v0.1.0-alpha.json
python3 tools/toolchain.py release-gate --allow-dirty --skip-cc-suite --with-soak --soak-frames-per-scene 600 --soak-scenes S0,S1,S2,S3,S10
python3 tools/toolchain.py release-gate --allow-dirty --skip-cc-suite --with-soak --soak-skip-build --soak-skip-pack --soak-runner-bin build_release_soak/vn_player --soak-frames-per-scene 600 --soak-scenes S0,S1,S2,S3,S10
python3 tools/toolchain.py release-bundle --out-dir build_release_bundle
python3 tools/toolchain.py release-report --out-dir build_release_report
python3 tools/toolchain.py release-publish-map --release-spec docs/release-publish-v0.1.0-alpha.json --out-dir build_release_publish
python3 tools/toolchain.py release-export --release-spec docs/release-publish-v0.1.0-alpha.json --out-dir build_release_export
python3 tools/toolchain.py validate-release-remote-state --release-json tests/fixtures/release_api/github_release_v0.1.0-alpha.json
python3 tools/toolchain.py validate-release-remote-state --release-json-url https://api.github.com/repos/AvrovaDonz2026/n64gal/releases/tags/v0.1.0-alpha
python3 tools/toolchain.py validate-release-remote-state --github-repo AvrovaDonz2026/n64gal --tag v0.1.0-alpha
python3 tools/toolchain.py release-remote-summary --release-json tests/fixtures/release_api/github_release_v0.1.0-alpha.json --out-dir build_release_remote
python3 tools/toolchain.py release-remote-summary --release-json-url https://api.github.com/repos/AvrovaDonz2026/n64gal/releases/tags/v0.1.0-alpha --out-dir build_release_remote
python3 tools/toolchain.py release-remote-summary --github-repo AvrovaDonz2026/n64gal --tag v0.1.0-alpha --out-dir build_release_remote
python3 tools/toolchain.py release-gate --allow-dirty --skip-cc-suite --with-soak --with-export --soak-skip-build --soak-skip-pack --soak-runner-bin build_release_soak/vn_player --export-out-dir build_release_export
python3 tools/toolchain.py release-gate --allow-dirty --skip-cc-suite --with-soak --with-bundle --soak-skip-build --soak-skip-pack --soak-runner-bin build_release_soak/vn_player --bundle-out-dir build_release_bundle
```

当前 release-facing 默认产物：

1. `release-gate` -> `build_release_gate/release_gate_summary.md` + `build_release_gate/release_gate_summary.json`
2. `release-soak` -> `build_release_soak/demo_soak_summary.md` + `build_release_soak/demo_soak_summary.json`
3. `release-host-sdk-smoke` -> `build_release_host_sdk/host_sdk_smoke_summary.md` + `build_release_host_sdk/host_sdk_smoke_summary.json`
4. `release-platform-evidence` -> `build_release_platform/platform_evidence_summary.md` + `build_release_platform/platform_evidence_summary.json`
5. `release-preview-evidence` -> `build_release_preview/preview_evidence_summary.md` + `build_release_preview/preview_evidence_summary.json`
6. `release-bundle` -> `build_release_bundle/release_bundle_index.md` + `build_release_bundle/release_bundle_index.json`
7. `release-bundle` -> `build_release_bundle/release_bundle_manifest.md` + `build_release_bundle/release_bundle_manifest.json`
8. `release-report` -> `build_release_report/release_report.md` + `build_release_report/release_report.json`
9. `release-publish-map` -> `build_release_publish/release_publish_map.md` + `build_release_publish/release_publish_map.json`
10. `release-export` -> `build_release_export/release_export_summary.md` + `build_release_export/release_export_summary.json`
11. `release-remote-summary` -> `build_release_remote/release_remote_summary.md` + `build_release_remote/release_remote_summary.json`
12. `release-preflight` -> `build_release_preflight/release_preflight_summary.md` + `build_release_preflight/release_preflight_summary.json`

当前统一入口已经覆盖：

1. `validate-all`
2. `validate-release-audit`
3. `validate-release-docs`
4. `validate-manifest`
5. `validate-release-contracts`
6. `validate-toolchain-contracts`
7. `validate-backend-contracts`
8. `validate-api-index-contracts`
9. `validate-compat-matrix`
10. `validate-ecosystem-contracts`
11. `validate-error-contracts`
12. `validate-host-sdk-contracts`
13. `validate-migration-contracts`
14. `validate-pack-contracts`
15. `validate-platform-contracts`
16. `validate-preview-contracts`
17. `validate-perf-contracts`
18. `validate-porting-contracts`
19. `validate-runtime-contracts`
20. `validate-save-contracts`
21. `validate-template-contracts`
22. `probe-vnsave`
23. `probe-trace-summary`
24. `probe-preview`
25. `probe-perf-summary`
26. `probe-perf-compare`
27. `probe-kernel-bench`
28. `probe-kernel-compare`
29. `migrate-vnsave`
30. `release gate script`
31. `release-gate`
32. `host sdk smoke`
33. `platform evidence`
34. `preview evidence`
35. `demo soak script`
36. `release-soak`
37. `release-gate --with-soak`
38. `release-soak --runner-bin`
39. `release-gate --soak-runner-bin`
40. `release-bundle`
41. `release-report`
42. `release-gate --with-bundle`

当前建议：

1. 发布前优先用 `python3 tools/toolchain.py release-gate --with-soak ...`
2. 若已有 release-like `vn_player`，优先用 `python3 tools/toolchain.py release-soak --runner-bin <path> ...`
3. 若想一条命令完成 gate + soak，优先用 `python3 tools/toolchain.py release-gate --with-soak --soak-runner-bin <path> ...`
4. 若想把 gate / soak / bundle 一次跑完，优先用 `python3 tools/toolchain.py release-gate --with-soak --with-bundle ...`
5. 若要补宿主 SDK 发布级证据，继续跑 `python3 tools/toolchain.py release-host-sdk-smoke ...`
6. 若要补平台发布级证据，继续跑 `python3 tools/toolchain.py release-platform-evidence --out-dir <dir>`
7. 若要补 preview 发布级证据，继续跑 `python3 tools/toolchain.py release-preview-evidence ...`
8. 若要生成单一发布报告，继续跑 `python3 tools/toolchain.py release-report --out-dir <dir>`
9. 若只想收口正式版证据目录，继续跑 `python3 tools/toolchain.py release-bundle --out-dir <dir>`；当前它会打包 `gate/soak/ci` 与 `host-sdk/platform/preview` 摘要，并生成带 SHA256 的 manifest
10. 若想一条命令把 `gate + soak + host-sdk/platform/preview evidence + bundle` 全跑完，优先用 `python3 tools/toolchain.py release-gate --with-soak --with-bundle ...`
11. `release-gate`、`release-soak`、`release-host-sdk-smoke`、`release-platform-evidence`、`release-preview-evidence`、`release-bundle` 与 `release-report` 当前都默认同时产出 markdown + json 摘要
12. 若要固定 `tag / release note / asset / bundle / report` 的映射，继续跑 `python3 tools/toolchain.py release-publish-map --out-dir <dir>`
13. 若要一条命令直接导出 `bundle + report + publish-map`，继续跑 `python3 tools/toolchain.py release-export --out-dir <dir>`
14. 若要把本地 canonical spec 和远端 GitHub prerelease 做机检比对，继续跑 `python3 tools/toolchain.py validate-release-remote-state --release-json <path>`；若已能访问 GitHub API，也可直接用 `--github-repo <owner/repo> --tag <tag>` 或 `--release-json-url <url>`
15. 若要生成远端 prerelease 摘要，继续跑 `python3 tools/toolchain.py release-remote-summary --release-json <path> --out-dir <dir>`；若已能访问 GitHub API，可直接用 `--github-repo <owner/repo> --tag <tag>` 或 `--release-json-url <url>`
16. `release-export` 当前在带 remote 输入时，会把 `release_remote_summary.{md,json}` 一起收进 final bundle
17. 若想用一条命令做正式版前预检，继续跑 `python3 tools/toolchain.py release-preflight ...`
18. 若想在 gate 之后直接产出 `bundle + report + publish-map`，继续跑 `python3 tools/toolchain.py release-gate --with-soak --with-export ...`
18. `release-report` 当前会显式引用 `release_bundle_manifest.json`，便于后续把证据链直接挂到 release asset
19. 这样会把 contract gate、`cc` suite、platform/host SDK/preview 证据和 soak 留痕合并成一份可引用摘要

键盘模式按键：

1. `1-9` 设置分支索引
2. `t` 切换 trace
3. `q` 退出循环

## 预览协议与无 GUI 预览

默认构建产物包含 `vn_previewd`，用于 editor/CI/脚本驱动的结构化预览。

```bash
./build/vn_previewd \
  --project-dir=. \
  --scene=S2 \
  --resolution=600x800 \
  --frames=8 \
  --trace \
  --command=set_choice:1 \
  --command=inject_input:choice:1 \
  --command=inject_input:key:t \
  --command=step_frame:8
```

也可以使用请求文件：

```bash
cat >/tmp/preview.req <<'EOF'
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

完整协议见 [`docs/preview-protocol.md`](./docs/preview-protocol.md)。

## 库 API 使用

### 一次性运行 API

```c
#include "vn_runtime.h"

int run_once(void) {
    VNRunConfig cfg;
    VNRunResult res;
    int rc;

    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 120u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_run(&cfg, &res);
    return rc;
}
```

### Session API（宿主循环）

```c
#include "vn_runtime.h"

int run_session(void) {
    VNRunConfig cfg;
    VNRunResult res;
    VNRuntimeSession* s;
    int rc;

    vn_run_config_init(&cfg);
    cfg.frames = 300u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_session_create(&cfg, &s);
    if (rc != 0) {
        return rc;
    }

    while (vn_runtime_session_is_done(s) == 0) {
        rc = vn_runtime_session_step(s, &res);
        if (rc != 0) {
            break;
        }
    }

    (void)vn_runtime_session_destroy(s);
    return rc;
}
```

## 测试与质量门禁

### C89 规则检查

```bash
./scripts/check_c89.sh
```

### CMake 测试

```bash
ctest --test-dir build --output-on-failure
```

### Perf 脚本

单 backend 全量采样示例（非 CI smoke gate）：

```bash
./tests/perf/run_perf.sh --backend scalar --scenes S0,S1,S2,S3,S10 --duration-sec 120 --warmup-sec 20 --dt-ms 16 --resolution 600x800
```

baseline/candidate 全量 sweep 示例（非 CI smoke gate）：

```bash
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S0,S1,S2,S3,S10 --duration-sec 120 --warmup-sec 20 --dt-ms 16 --resolution 600x800 --out-dir /tmp/n64gal_perf_compare
```

当前 GitHub Actions 的 `linux-x64` smoke gate 默认使用 `S1,S3,S10`，而不是上面的 `S0,S1,S2,S3,S10` 全量 sweep；原因是 `S0` 在 shipped 主路径上会被 `frame reuse` 压到约 `0.001ms`，已经失去有效测量价值，而 `S10` 则作为更重的 perf sample 补进 smoke coverage。

开发者理解 `S1/S3/S10` 时，可以直接把它们当成三类典型 VN 画面：

1. `S1`：标准循环对话场景。脚本层面是 `BGM + 两段 TEXT + 周期性 FADE/WAIT + SE` 的 loop；渲染层面对应 `clear + 128x128 不透明角色 sprite + 320x36 文本框 + 全屏 fade`，适合作为“常规对话吞吐”样本。
2. `S3`：短节奏转场/收束场景。脚本层面是更短的 `TEXT -> FADE -> WAIT -> SE -> TEXT -> WAIT -> END`；渲染层面仍是 4-op 中等负载，但节奏更紧，当前也更适合观察 `dirty-tile` 和短窗口 jitter。
3. `S10`：重演出压力场景。脚本层面是 `BGM + 三段 TEXT + FADE/WAIT loop`；渲染层面是 `clear + 基础角色 sprite + 448x48 宽文本框 + 两层大号半透明 overlay sprite(504x332 / 432x208) + fade`，当前是最接近“多层 CG/立绘叠画”的重样本。

若要复现当前 CI gate，请改用：

```bash
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S1,S3,S10 --duration-sec 2 --warmup-sec 1 --dt-ms 16 --resolution 600x800 --threshold-file tests/perf/perf_thresholds.csv --threshold-profile linux-x64-scalar-avx2-smoke --out-dir /tmp/n64gal_perf_smoke
```

输出包括：

1. `perf_<scene>.csv`
2. `perf_summary.csv`（含每场景 `p95_frame_ms`）
3. `compare/perf_compare.csv`
4. `compare/perf_compare.md`
5. `compare/perf_threshold_metrics.csv` / `compare/perf_threshold_results.csv` / `compare/perf_threshold_report.md`（启用门限 profile 时）
6. `perf_report_template.md`
7. 当 `--repeat > 1` 时，baseline/candidate 目录还会额外生成 `perf_summary_repeats.csv` 与 `perf_repeat_aggregate.md`，`compare/` 下还会生成 `perf_repeat_variability.csv` / `perf_repeat_variability.md`，并把 `Repeat Variability` 章节直接追加进 `compare/perf_compare.md`

完整流程见 [`docs/perf-report.md`](./docs/perf-report.md)。perf 工具链现在还会自动把当前 runner 的 `host_cpu` 写入 `perf_summary.csv` / `kernel_bench.csv` 与 compare artifact，便于直接对照 Linux/Windows runner 的 CPU 型号；`run_perf.sh` / compare artifact 也会同时记录 `requested_backend` 与 `actual_backend`，避免静默 fallback 仍被误记成请求 backend；`perf_workflow_summary.md` 也会直接回显 `VN_PERF_CFLAGS` / `VN_PERF_LDFLAGS`，用来判断这次 perf 是否走了优化构建口径。当前 perf 脚本默认保留 `VN_RUNTIME_PERF_FRAME_REUSE + VN_RUNTIME_PERF_OP_CACHE`，因此测的是主线路径的整机收益；`VN_RUNTIME_PERF_DIRTY_TILE` 与 `VN_RUNTIME_PERF_DYNAMIC_RESOLUTION` 都可通过 `--perf-dirty-tile=<on|off>` / `--perf-dynamic-resolution=<on|off>` 显式开关，其中二者当前默认都保持 `off`。`dirty tile` 开启后会实际驱动 partial submit，且目前 `scalar`、`avx2`、`neon`、`rvv` 都支持该路径；当前 runtime 对“已知必整帧”场景还会跳过 planner build 与 full-redraw commit bounds，planner 也已把 dirty tile 计数改成标记时增量维护，避免 partial 路径再做一次 bitset 全表 recount。`dynamic resolution` 开启后则允许 runtime 在一次 run 内切换 `R0/R1/R2`，实际渲染尺寸与切档次数可直接从 `vn_player --trace` 或 preview `final_state` 里的 `render_width/render_height/dynamic_resolution_*` 读取。若要拆开归因，可直接用 `vn_player --trace` 分别验证：`--perf-frame-reuse=off`、`--perf-op-cache=off`、`--perf-dirty-tile=on`、`--perf-dynamic-resolution=on`；也可以直接用 `tests/perf/run_perf_compare.sh` 对同一 backend 做 dirty on/off 或 dynres on/off compare。GitHub Actions 现在已把同一套 `scripts/ci/run_perf_smoke_suite.sh` 落到全部原生目标平台：`linux-x64` 与 `linux-arm64` 都会走独立的优化 perf 构建（当前显式注入 `VN_PERF_CFLAGS=-O2 -DNDEBUG`），避免把 `run_cc_suite.sh` 的未优化测试二进制误拿来做 smoke gate；`linux-x64` 使用 `scalar -> avx2` / `avx2 dirty off -> on` / `scalar dynres off -> on`，并额外保留一条 `kernel avx2 -> avx2_asm` 的 report-only compare；`linux-arm64` 与 `windows-arm64` 使用对应的 `neon` compare，`windows-x64` 使用 `avx2` compare，两个 Windows 平台继续复用各自的 CMake `Release` 可执行文件。四个平台都会产出 `perf_workflow_summary.md` 和 compare artifact；其中 `scripts/ci/run_perf_smoke_suite.sh` 现在额外固定生成 `Compare D = kernel scalar -> simd`，而 Linux x64 还会补一条 `Compare E = kernel avx2 -> avx2_asm`，目录为 `kernel_<baseline>_vs_<candidate>/compare/kernel_compare.md`；dirty compare 的 `perf_repeat_variability.csv/.md` 摘要也会继续提升进 `perf_workflow_summary.md`，方便直接在 `linux-x64/avx2`、`linux-arm64/neon`、`windows-arm64/neon` 的 step summary 里判断 jitter。并已各自挂上 `Compare A` threshold profile。当前门限策略分三档：`linux-x64` 继续使用 `linux-x64-scalar-avx2-smoke` hard gate，`linux-arm64`、`windows-arm64` 与 `windows-x64` 现在都使用正收益 gate；其中 `windows-x64-scalar-avx2-smoke` 已根据连续两次 GitHub 原生成功 run `22795078202`（head `d6081b4`）与 `22795471059`（head `19788ce`，均为 2026-03-07）的结果收紧为“非负收益”门限。最新一轮又给 `linux-x64` / `windows-x64` 两个 smoke profile 补了 `S10` 的独立 `candidate_p95_ms` 门限，并给 `kernel scalar -> avx2` 的 `sprite_full_opaque` / `sprite_full_alpha180` 补了 `soft/report-only` threshold。该 run 的 `Compare D` 也表明 `sprite_full_opaque` / `sprite_full_alpha180` 仍是最大的剩余绝对 kernel。仓库当前已经补上 `uniform alpha/fade` AVX2 row kernel、对齐后的 `fill_u32`，并把 `SPRITE/TEXT` 的 `sample/hash -> combine -> blend` 热循环收回 AVX2 TU；专项调查与后续复测计划见 [`docs/perf-windows-x64-2026-03-07.md`](./docs/perf-windows-x64-2026-03-07.md)。`linux-x64` 的 `scalar -> avx2` 与 dirty compare 已从 `S0,S3` 调整到 `S1,S3,S10`，因为 `S0` 在主线路径上会被 frame reuse 压到 `0.001ms`，不再具备有效测量价值，而 `S10` 则作为更重的 perf sample 用于补足 smoke coverage。dirty compare 现已固定为 `6s/1s + repeat=3`，结果按中位数汇总后再写 compare artifact；同时 `run_perf_compare.sh` 会额外产出 `perf_summary_repeats.csv`、`compare/perf_repeat_variability.csv`、`compare/perf_repeat_variability.md`，并把 `Repeat Variability` 章节直接并入 `compare/perf_compare.md`，用每个 scene 的 `min/median/max/range` 帮助区分真实回退与短窗口 jitter，尤其用于继续观察 `windows-x64` dirty `S3` 噪声。当前仍应按趋势证据解读而不是发布级结论。当前已固化四份 perf 证据：[`docs/perf-rvv-2026-03-06.md`](./docs/perf-rvv-2026-03-06.md)（RVV qemu smoke）、[`docs/perf-dirty-2026-03-07.md`](./docs/perf-dirty-2026-03-07.md)（dirty-tile 本地 repeat-median compare）、[`docs/perf-dynres-2026-03-07.md`](./docs/perf-dynres-2026-03-07.md)（dynamic-resolution 本地 smoke）与 [`docs/perf-windows-x64-2026-03-07.md`](./docs/perf-windows-x64-2026-03-07.md)（GitHub Windows x64 AVX2 调查与修正）。 最新又补了一份 [`docs/perf-x64-hosts-2026-03-09.md`](./docs/perf-x64-hosts-2026-03-09.md)，直接对照 `perf-linux-x64` 与 `perf-windows-x64` 的 runner CPU、scene compare 和 kernel compare，作为后续 x64 threshold/host-shape 调参依据。 若要进一步把整机 smoke 拆到后端热点，可直接用 `./tests/perf/run_kernel_compare.sh --baseline scalar --candidate avx2 --resolution 600x800 --iterations 24 --warmup 6 --out-dir /tmp/n64gal_kernel_compare` 生成 compare artifact，或用 `./tests/perf/run_kernel_bench.sh --backend scalar|avx2|avx2_asm|neon|rvv --iterations 24 --warmup 6 --out-csv /tmp/kernel.csv` 单独导出 `CLEAR/FADE/scene/full-span textured` 的 kernel 级 CSV；当前本地样本显示 `clear/fade` 已明显转正，剩余更大的收益面继续集中在 textured full-span 路径。

平台矩阵、路径/文件 I/O 收口与验证路线见 [`docs/platform-matrix.md`](./docs/platform-matrix.md)。

预览协议与无 GUI 预览入口见 [`docs/preview-protocol.md`](./docs/preview-protocol.md) 与 [`tools/previewd/README.md`](./tools/previewd/README.md)。

## 后端支持状态

1. `scalar`：可用，作为行为基线与回退目标。
2. `avx2`：可运行实现已接入（`CLEAR/SPRITE/TEXT/FADE` + `tex/combine` 采样），CPU 不支持时自动回退 `scalar`；当前阶段判断为“已稳定，转维护优化”，其中 x64 CI 的 `matched backend=avx2` 显式断言、perf artifact 的 `requested_backend/actual_backend` 记账、`avx2_dirty` golden 对照、`256/257` 边界 fast-path parity 单测，以及 `force scalar / force avx2 / SIMD auto` 的真实后端优先级单测都已经补齐。最近一轮又把 row-palette build 收口到 8-lane chunk 生成，并把 heuristic 先策略化成 `vn_avx2_should_use_row_palette(...)` helper；结构上也新增了 `avx2_internal.h` / `avx2_textured.c` / `avx2_fill_fade.c`，把 textured 热路径与 fill-fade 路径都从 backend glue 里拆离。`windows-x64 dirty S3` 现在也已经有 `10s/2s + repeat=5` 的长窗口 report-only 任务持续留痕；Linux/Windows x64 的 host CPU 与收益形状差异则已固定进 [`docs/perf-x64-hosts-2026-03-09.md`](./docs/perf-x64-hosts-2026-03-09.md)。剩余重点转向 `Clang/MSVC` 兼容补证；其中 Linux x64 现已接入 `CC=clang` suite，Windows x64 现已补 `MSVC Debug compile-only`，并继续尝试 `ClangCL Debug compile-only`，但“无 AVX2 Windows x64 机器上的安全回退”仍是已知缺口。
3. `avx2_asm`：实验性 force-only 变体，仅在 GNU x64 下允许初始化；当前仍只把 `clear/fill(alpha=255)` 切到 x64 ASM 热路径，其他路径继续复用 `avx2` 的现有实现，其它平台或 toolchain 上强制请求会回退 `scalar`。这条线现已接入 `test_renderer_fallback`、`test_backend_priority`、`test_renderer_dirty_submit`、`test_backend_consistency` 与 `test_runtime_golden` 的最小覆盖，本地 `run_cc_suite` 已复核通过。最新一轮稳定 `kernel avx2 -> avx2_asm` 样本里，`clear_full p95 0.797ms -> 0.105ms`，而非 fill 热点基本持平，说明当前主要收益仍集中在 fill/clear。
4. `neon`：最小可运行后端已接入，`fill` SIMD、uniform alpha/fade row kernel，以及宽行 `SPRITE/TEXT` 的 row-palette 复用/写回已落地；最近一轮又把 textured-row 的 `row params + local sample/combine/blend` 收回 `neon_backend.c` TU 内，并继续接上 4-lane NEON `sample/hash -> combine` chunk 内核，当前 direct row opaque/blend、row-palette build，以及 textured alpha 的 packed-channel 向量 blend 已复用同一核心，row 级 `seed/checker/base_rgb` 常量也已前折叠。最新一轮又把 row-palette alpha path 改成预打包 `full/RB/G` palette、给 opaque row-palette 补了整行缓存复用，并进一步给 repeated-`v8` 的半透明 row-palette 补了 `RB/G` 行缓存复用；`uniform blend/fade` row kernel 也已收口到 packed `RB/G` 两路版本。`aarch64` 交叉编译与 GitHub `arm64 Linux/Windows` 原生 CI 已通过，且本地已再次用 `qemu-aarch64` 复核 `fallback`、`dirty submit` 与 `runtime golden`。当前剩余重点转向 row-palette gather/apply 的进一步降开销与更宽 chunk 评估；其中 `u_lut` lane-load 已先切到 `vld1_u8 + vmovl` 并补 `u_lut` 专用尾部 padding（`v_lut` 侧 `tail_pad=0` 以避免越界写），alpha repeated-v8 row-cache 构建也已改成 4-lane chunk helper，而 `row-palette` 的 full/RB/G palette gather 以及 direct textured row 的 `u` 索引 chunk 都开始复用 `u_lut_u32` 展开缓存与单次 `vld1q_u32` 索引载入；这轮又把 `u_lut` 与 `u_lut_u32` 的生成收口到同一遍循环里，把 alpha row-palette 全面切到“只生成 `RB/G`，不再生成 full palette”，删除了冗余的 `g_neon_u_lut(u8)` 缓存，并把 row-palette / direct row / palette build / fill-fade 的几个 4px 热循环统一改成 8px unroll；最新一轮又新增了 `sample_combine` 预加载常量版 helper，让 hottest loop 不再每个 chunk 反复 `vld1q` 行常量，同时把 non-palette direct row 的 `params` 初始化也收口到按 `v8` 复用，后续继续看 native perf 证据。
5. `rvv`：最小可运行后端已接入，统一色 `fill`、半透明 `fade/fill`，以及 `SPRITE/TEXT` 的 `tex/hash -> combine -> alpha` 路径已向量化；其中 `sample -> combine` 已融合，`alpha=255` 已可直接写 framebuffer，`alpha<255` 也已收口到单循环 `blend/store`，UV LUT 已降到 8-bit 存储，`seed/checker` 常量和基础偏置也已前折叠。当前已验证 `riscv64` 交叉构建、`qemu-user` 功能冒烟、`scalar vs rvv` CRC 一致性，以及 `riscv-perf-report` 的 GitHub artifact 流程；在缺少原生 `riscv64/RVV` 设备时，项目阶段策略按 `qemu-first` 收口，原生验证与发布级 perf 证据后置。

## CI

1. GitHub Actions 主矩阵工作流：`.github/workflows/ci-matrix.yml`
2. GitHub Actions RVV perf 报告工作流：`.github/workflows/riscv-perf-report.yml`
3. Linux 原生 C89 套件脚本：`scripts/ci/run_cc_suite.sh`
4. Windows 原生套件脚本：`scripts/ci/run_windows_suite.ps1`
5. riscv64 交叉构建脚本：`scripts/ci/build_riscv64_cross.sh`
6. riscv64 qemu 冒烟脚本：`scripts/ci/run_riscv64_qemu_suite.sh`
7. riscv64 qemu perf 报告脚本：`scripts/ci/run_riscv64_qemu_perf_report.sh`
8. x64 perf smoke wrapper：`scripts/ci/run_perf_smoke_suite.sh`
9. workflow 已接入 `linux-riscv64-qemu-scalar` 与 `linux-riscv64-qemu-rvv` 两个阻塞 job
10. `linux-x64` 与 `linux-arm64` 会产出 `suite-linux-x64` / `suite-linux-arm64` artifact，内容包括 `ci_logs/`、`ci_suite_summary.md` 与 `golden_artifacts/`；其中 `ci_suite_summary.md` 会回显 `test_renderer_dirty_submit` 的 matched backend 列表，`linux-arm64` 会在 workflow 内显式校验 `matched backend=neon`，`linux-x64` 现也会显式校验 `matched backend=avx2`
11. `windows-x64` 与 `windows-arm64` 统一通过 `scripts/ci/run_windows_suite.ps1` 产出 `suite-windows-x64` / `suite-windows-arm64` artifact，收纳 `configure/build/ctest` 日志以及 `test_renderer_fallback`、`test_renderer_dirty_submit`、`test_runtime_api`、`test_runtime_golden` 复跑证据；脚本会在失败场景下尽量保留 summary 与已生成日志，且该链路已在 GitHub Actions push run `22772138491`（`2026-03-07 00:26 HKT`）完成实跑验证。`windows-arm64` 现也会在 workflow 内显式校验 `matched backend=neon`，`windows-x64` 现也会显式校验 `matched backend=avx2`
12. `linux-x64` 还会产出 `perf-linux-x64` artifact（当前 smoke scenes 为 `S1,S3,S10`；其中 `S10` 为更重的 perf sample；内容包括 `scalar vs avx2` 门限报告 + `avx2 dirty off/on` compare + `avx2 dirty repeat variability` 摘要/报告 + `scalar dynres off/on` compare + `kernel scalar vs avx2` compare + `perf_workflow_summary.md`）
13. `linux-arm64` 现也会产出 `perf-linux-arm64` artifact（`scalar vs neon` compare + `neon dirty off/on` compare + `neon dirty repeat variability` 摘要/报告 + `scalar dynres off/on` compare + `kernel scalar vs neon` compare + `perf_workflow_summary.md`；当前 `Compare A` 已挂 `linux-arm64-scalar-neon-smoke` threshold profile，且 CI 已改为独立 `-O2 -DNDEBUG` perf 构建，不再复用 `build_ci_cc/vn_player` 这类未优化测试二进制）
14. `windows-x64` 与 `windows-arm64` 现会分别产出 `perf-windows-x64` / `perf-windows-arm64` artifact；前者跑 `avx2` compare 并挂 `windows-x64-scalar-avx2-smoke` 正收益 gate，后者跑 `neon` compare 并挂 `windows-arm64-scalar-neon-smoke` 正收益 gate；两者也都会产出对应的 `kernel scalar vs simd` compare，且 dirty compare 现在都会把 repeat variability 摘要抬进 `perf_workflow_summary.md`。其中 `windows-x64` 又额外补了一条 `S3 + 10s/2s + repeat=5` 的长窗口 dirty report-only 任务，用于把短窗口 jitter 和真实回退拆开。
15. `linux-riscv64-qemu-scalar` 与 `linux-riscv64-qemu-rvv` 会产出对应 suite artifact，收纳 qemu smoke logs、fallback 证据与 golden artifact 目录
16. `linux-riscv64-qemu-rvv-perf-report` 会在 `workflow_dispatch` / nightly 下产出 `perf-riscv64-qemu-rvv` artifact（`rvv` revision compare markdown + 可选 threshold report）；当前默认接入 `linux-riscv64-qemu-rvv-rev-smoke` 的 `soft` gate，首次 dispatch run `22766736383` 已验证成功
17. 当前 `riscv64` 策略为 `qemu-first`：先收口 `cross/qemu/golden/perf artifact`，原生 nightly 待设备到位后恢复
16. RISC-V 工具链与验证路线：[`docs/riscv-toolchain.md`](./docs/riscv-toolchain.md)
17. 性能报告流程：[`docs/perf-report.md`](./docs/perf-report.md)

## API 文档

1. Runtime API：[`docs/api/runtime.md`](./docs/api/runtime.md)
2. Host SDK 指南：[`docs/host-sdk.md`](./docs/host-sdk.md)
3. 宿主嵌入示例：[`examples/host-embed/README.md`](./examples/host-embed/README.md)
4. Backend 契约：[`docs/api/backend.md`](./docs/api/backend.md)
5. Pack API：[`docs/api/pack.md`](./docs/api/pack.md)
6. API 索引：[`docs/api/README.md`](./docs/api/README.md)
7. Dirty-Tile 设计/API 现状（draft + landed slices）：[`docs/api/dirty-tile-draft.md`](./docs/api/dirty-tile-draft.md)
8. 后端移植指南：[`docs/backend-porting.md`](./docs/backend-porting.md)

## 开发约束

1. 运行时代码遵循 C89（禁止 C99/C11 语法特性）。
2. API 变更需同步更新 `docs/api/*` 与 `issue.md`。
3. 性能优化必须可开关、可回退、可对照验证。
