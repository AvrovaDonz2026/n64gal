# N64GAL

N64GAL 是一个面向 Galgame/VN 的实验性引擎原型，核心目标是：

1. 前后端分离。
2. 单一前后端 API 契约。
3. C89 严格兼容。
4. 便于跨架构后端扩展（amd64/AVX2 -> arm64/NEON -> riscv64/RVV）。

当前代码以“库优先”方式组织：核心能力通过 `vn_runtime.h` 暴露，预览协议入口通过 `vn_preview.h` 暴露，`vn_player` 仅作为可选 CLI 包装。

## 项目状态（2026-03-07）

1. 已完成：
   - `scalar` 后端最小闭环（默认 `600x800`，场景 `S0-S3`）。
   - Frontend 输出统一 `VNRenderOp[]`。
   - `vn_runtime_run(config, result)` 结构化运行入口。
   - Session API：`create/step/is_done/set_choice/inject_input/destroy`。
   - `vn_previewd` 与 `preview protocol v1` 已落地，可供 editor/CI 复用。
2. 进行中:
   - Golden 基线收口：`test_runtime_golden` 已固化 `S0-S3 @ 600x800` 标量 CRC；支持的 SIMD 后端按 `mismatch_percent < 1%` 且 `max_channel_diff <= 8` 判定，并在出现差异或 CRC 异常时导出 `expected/actual/diff` PPM 与 `summary.txt`。
   - `ISSUE-008` 已继续落地：`qemu-rvv` revision compare soft gate 已接到 perf workflow；Runtime `VN_RUNTIME_PERF_FRAME_REUSE` 静态帧短路与 `VNRenderOp[]` LRU 命令缓存均已接入，前者在稳定状态下直接复用 framebuffer，后者继续按当前帧回写 `SPRITE/FADE` 动态字段；`Dirty-Tile` 的设计/API 草案已冻结到 `docs/api/dirty-tile-draft.md`，当前规划为“frame reuse miss -> 当前帧最终 ops -> dirty plan -> renderer dirty submit”；动态分辨率仍排在其后。
   - x64/arm64 + Linux/Windows CI 矩阵已全绿。
   - `neon` 最小后端已接入，arm64 Linux/Windows CI 已通过；下一步转向补算子、golden 阈值与性能门限。
   - `rvv` 最小后端已接入，`tex/hash -> combine -> alpha` 热路径已向量化，`sample -> combine` 已融合，且 `alpha=255` / `alpha<255` 都已收口到更短的写回路径；UV LUT 已压到 8-bit，`seed/checker` 常量和基础偏置也已前折叠。`riscv64` 的 `cross-build / qemu-scalar / qemu-rvv / qemu perf artifact` 已验证，当前按 `qemu-first` 收口；原生 `native-riscv64/RVV` 设备未就绪前，原生 nightly 与发布级 perf 证据暂保留为外部阻塞项。
   - 输入抽象层进一步统一（键盘输入与脚本化输入）。

详细路线图见 [issue.md](./issue.md) 与 [dream.md](./dream.md)。`Dirty-Tile` 设计/API 草案见 [docs/api/dirty-tile-draft.md](./docs/api/dirty-tile-draft.md)。

## 目标平台矩阵

当前项目将平台支持目标明确为：

1. `x64 + Linux`
2. `x64 + Windows`
3. `arm64 + Linux`
4. `arm64 + Windows`
5. `riscv64 + Linux`

后端优先级策略：

1. x64: `avx2` -> `scalar`
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
3. 后端选择支持自动与强制模式，失败自动回退到 `scalar`。

## 仓库结构

```text
include/        对外头文件（backend/runtime/frontend/types/error）
src/core/       运行时、渲染器、包格式、VM
src/frontend/   Frontend -> RenderOp 构建
src/backend/    各后端实现（scalar/avx2/...）
src/tools/      无 GUI 工具入口（如 `vn_previewd`）
tools/scriptc/  场景脚本编译器
tools/packer/   vnpak 打包工具
tools/previewd/ 预览协议说明与示例
tests/unit/     单元测试
tests/integration/ 预览协议等集成测试
tests/perf/     基准脚本与 CSV 输出
examples/       宿主嵌入与集成示例
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
  src/core/backend_registry.c \
  src/core/renderer.c \
  src/core/vm.c \
  src/core/pack.c \
  src/core/platform.c \
  src/core/runtime_cli.c \
  src/frontend/render_ops.c \
  src/backend/common/pixel_pipeline.c \
  src/backend/avx2/avx2_backend.c \
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
  src/core/backend_registry.c \
  src/core/renderer.c \
  src/core/vm.c \
  src/core/pack.c \
  src/core/platform.c \
  src/core/runtime_cli.c \
  src/frontend/render_ops.c \
  src/backend/common/pixel_pipeline.c \
  src/backend/avx2/avx2_backend.c \
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

1. `--scene=S0|S1|S2|S3`
2. `--backend=auto|scalar|avx2|neon|rvv`
3. `--resolution=600x800`
4. `--frames=<N>`
5. `--dt-ms=<N>`
6. `--choice-index=<N>`
7. `--choice-seq=0,1,0`
8. `--trace`
9. `--keyboard`（Linux TTY / Windows console 调试模式，按 `--dt-ms` 节奏推进）
10. `--quiet`

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

单 backend 采样：

```bash
./tests/perf/run_perf.sh --backend scalar --scenes S0,S1,S2,S3 --duration-sec 120 --warmup-sec 20 --dt-ms 16 --resolution 600x800
```

baseline/candidate 对照：

```bash
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S0,S1,S2,S3 --duration-sec 120 --warmup-sec 20 --dt-ms 16 --resolution 600x800 --threshold-file tests/perf/perf_thresholds.csv --threshold-profile linux-x64-scalar-avx2-smoke --out-dir /tmp/n64gal_perf_compare
```

输出包括：

1. `perf_<scene>.csv`
2. `perf_summary.csv`（含每场景 `p95_frame_ms`）
3. `compare/perf_compare.csv`
4. `compare/perf_compare.md`
5. `compare/perf_threshold_metrics.csv` / `compare/perf_threshold_results.csv` / `compare/perf_threshold_report.md`（启用门限 profile 时）
6. `perf_report_template.md`

完整流程见 [`docs/perf-report.md`](./docs/perf-report.md)。当前 perf 脚本默认保留 `VN_RUNTIME_PERF_FRAME_REUSE + VN_RUNTIME_PERF_OP_CACHE`，因此测的是主线路径的整机收益；若要拆开归因，可直接用 `vn_player --trace` 分别验证：`--perf-frame-reuse=off`、`--perf-op-cache=off`。当前已固化一份 RVV 提交前后 smoke 报告：[`docs/perf-rvv-2026-03-06.md`](./docs/perf-rvv-2026-03-06.md)。

平台矩阵、路径/文件 I/O 收口与验证路线见 [`docs/platform-matrix.md`](./docs/platform-matrix.md)。

预览协议与无 GUI 预览入口见 [`docs/preview-protocol.md`](./docs/preview-protocol.md) 与 [`tools/previewd/README.md`](./tools/previewd/README.md)。

## 后端支持状态

1. `scalar`：可用，作为行为基线与回退目标。
2. `avx2`：可运行实现已接入（`CLEAR/SPRITE/TEXT/FADE` + `tex/combine` 采样），CPU 不支持时自动回退 `scalar`。
3. `neon`：最小可运行后端已接入，`fill` SIMD 算子已落地，`aarch64` 交叉编译已通过，当前待补原生 arm64 验证与其余核心算子。
4. `rvv`：最小可运行后端已接入，统一色 `fill`、半透明 `fade/fill`，以及 `SPRITE/TEXT` 的 `tex/hash -> combine -> alpha` 路径已向量化；其中 `sample -> combine` 已融合，`alpha=255` 已可直接写 framebuffer，`alpha<255` 也已收口到单循环 `blend/store`，UV LUT 已降到 8-bit 存储，`seed/checker` 常量和基础偏置也已前折叠。当前已验证 `riscv64` 交叉构建、`qemu-user` 功能冒烟、`scalar vs rvv` CRC 一致性，以及 `riscv-perf-report` 的 GitHub artifact 流程；在缺少原生 `riscv64/RVV` 设备时，项目阶段策略按 `qemu-first` 收口，原生验证与发布级 perf 证据后置。

## CI

1. GitHub Actions 主矩阵工作流：`.github/workflows/ci-matrix.yml`
2. GitHub Actions RVV perf 报告工作流：`.github/workflows/riscv-perf-report.yml`
3. Linux 原生 C89 套件脚本：`scripts/ci/run_cc_suite.sh`
4. Windows 原生套件脚本：`scripts/ci/run_windows_suite.ps1`
5. riscv64 交叉构建脚本：`scripts/ci/build_riscv64_cross.sh`
6. riscv64 qemu 冒烟脚本：`scripts/ci/run_riscv64_qemu_suite.sh`
7. riscv64 qemu perf 报告脚本：`scripts/ci/run_riscv64_qemu_perf_report.sh`
8. workflow 已接入 `linux-riscv64-qemu-scalar` 与 `linux-riscv64-qemu-rvv` 两个阻塞 job
9. `linux-x64` 与 `linux-arm64` 会产出 `suite-linux-x64` / `suite-linux-arm64` artifact，内容包括 `ci_logs/`、`ci_suite_summary.md` 与 `golden_artifacts/`
10. `windows-x64` 与 `windows-arm64` 统一通过 `scripts/ci/run_windows_suite.ps1` 产出 `suite-windows-x64` / `suite-windows-arm64` artifact，收纳 `configure/build/ctest` 日志以及 `test_renderer_fallback`、`test_runtime_api`、`test_runtime_golden` 复跑证据；脚本会在失败场景下尽量保留 summary 与已生成日志，且该链路已在 GitHub Actions push run `22772138491`（`2026-03-07 00:26 HKT`）完成实跑验证
11. `linux-x64` 还会产出 `perf-linux-x64` artifact（`scalar vs avx2` 对照 + perf threshold report）
12. `linux-riscv64-qemu-scalar` 与 `linux-riscv64-qemu-rvv` 会产出对应 suite artifact，收纳 qemu smoke logs、fallback 证据与 golden artifact 目录
13. `linux-riscv64-qemu-rvv-perf-report` 会在 `workflow_dispatch` / nightly 下产出 `perf-riscv64-qemu-rvv` artifact（`rvv` revision compare markdown + 可选 threshold report）；当前默认接入 `linux-riscv64-qemu-rvv-rev-smoke` 的 `soft` gate，首次 dispatch run `22766736383` 已验证成功
14. 当前 `riscv64` 策略为 `qemu-first`：先收口 `cross/qemu/golden/perf artifact`，原生 nightly 待设备到位后恢复
15. RISC-V 工具链与验证路线：[`docs/riscv-toolchain.md`](./docs/riscv-toolchain.md)
16. 性能报告流程：[`docs/perf-report.md`](./docs/perf-report.md)

## API 文档

1. Runtime API：[`docs/api/runtime.md`](./docs/api/runtime.md)
2. Host SDK 指南：[`docs/host-sdk.md`](./docs/host-sdk.md)
3. 宿主嵌入示例：[`examples/host-embed/README.md`](./examples/host-embed/README.md)
4. Backend 契约：[`docs/api/backend.md`](./docs/api/backend.md)
5. Pack API：[`docs/api/pack.md`](./docs/api/pack.md)
6. API 索引：[`docs/api/README.md`](./docs/api/README.md)
7. Dirty-Tile 设计/API 草案（draft）：[`docs/api/dirty-tile-draft.md`](./docs/api/dirty-tile-draft.md)

## 开发约束

1. 运行时代码遵循 C89（禁止 C99/C11 语法特性）。
2. API 变更需同步更新 `docs/api/*` 与 `issue.md`。
3. 性能优化必须可开关、可回退、可对照验证。
