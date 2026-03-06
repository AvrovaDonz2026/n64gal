# N64GAL

N64GAL 是一个面向 Galgame/VN 的实验性引擎原型，核心目标是：

1. 前后端分离。
2. 单一前后端 API 契约。
3. C89 严格兼容。
4. 便于跨架构后端扩展（amd64/AVX2 -> arm64/NEON -> riscv64/RVV）。

当前代码以“库优先”方式组织：核心能力通过 `vn_runtime.h` 暴露，预览协议入口通过 `vn_preview.h` 暴露，`vn_player` 仅作为可选 CLI 包装。

## 项目状态（2026-03-06）

1. 已完成：
   - `scalar` 后端最小闭环（默认 `600x800`，场景 `S0-S3`）。
   - Frontend 输出统一 `VNRenderOp[]`。
   - `vn_runtime_run(config, result)` 结构化运行入口。
   - Session API：`create/step/is_done/set_choice/inject_input/destroy`。
   - `vn_previewd` 与 `preview protocol v1` 已落地，可供 editor/CI 复用。
2. 进行中:
   - AVX2 收口：已固化 `test_runtime_golden` 的 `S0-S3 @ 600x800` 标量 CRC 基线与 `avx2` 对照，下一步补差异图误差阈值。
   - x64/arm64 + Linux/Windows CI 矩阵已全绿。
   - `neon` 最小后端已接入，待原生 arm64 进一步补算子与压测。
   - `rvv` 最小后端已接入，`tex/hash + combine + alpha` 热路径已开始向量化，`riscv64` 交叉构建与 `qemu-user` 冒烟已验证，待原生平台验证与进一步融合优化。
   - 输入抽象层进一步统一（键盘输入与脚本化输入）。

详细路线图见 [issue.md](./issue.md) 与 [dream.md](./dream.md)。

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
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S0,S1,S2,S3 --duration-sec 120 --warmup-sec 20 --dt-ms 16 --resolution 600x800 --out-dir /tmp/n64gal_perf_compare
```

输出包括：

1. `perf_<scene>.csv`
2. `perf_summary.csv`（含每场景 `p95_frame_ms`）
3. `compare/perf_compare.csv`
4. `compare/perf_compare.md`
5. `perf_report_template.md`

完整流程见 [`docs/perf-report.md`](./docs/perf-report.md)。

平台矩阵、路径/文件 I/O 收口与验证路线见 [`docs/platform-matrix.md`](./docs/platform-matrix.md)。

预览协议与无 GUI 预览入口见 [`docs/preview-protocol.md`](./docs/preview-protocol.md) 与 [`tools/previewd/README.md`](./tools/previewd/README.md)。

## 后端支持状态

1. `scalar`：可用，作为行为基线与回退目标。
2. `avx2`：可运行实现已接入（`CLEAR/SPRITE/TEXT/FADE` + `tex/combine` 采样），CPU 不支持时自动回退 `scalar`。
3. `neon`：最小可运行后端已接入，`fill` SIMD 算子已落地，`aarch64` 交叉编译已通过，当前待补原生 arm64 验证与其余核心算子。
4. `rvv`：最小可运行后端已接入，统一色 `fill`、半透明 `fade/fill`，以及 `SPRITE/TEXT` 的 `tex/hash + combine + alpha` 路径已向量化；当前已验证 `riscv64` 交叉构建、`qemu-user` 功能冒烟以及 `scalar vs rvv` CRC 一致性，待补原生 riscv64 Linux 验证、性能采样与进一步融合优化。

## CI

1. GitHub Actions 矩阵工作流：`.github/workflows/ci-matrix.yml`
2. Linux 原生 C89 套件脚本：`scripts/ci/run_cc_suite.sh`
3. riscv64 交叉构建脚本：`scripts/ci/build_riscv64_cross.sh`
4. riscv64 qemu 冒烟脚本：`scripts/ci/run_riscv64_qemu_suite.sh`
5. workflow 已接入 `linux-riscv64-qemu-scalar` 阻塞 job 与 `linux-riscv64-qemu-rvv` 告警 job
6. `linux-x64` 会产出 `perf-linux-x64` artifact（`scalar vs avx2` 对照）
7. RISC-V 工具链与验证路线：[`docs/riscv-toolchain.md`](./docs/riscv-toolchain.md)
8. 性能报告流程：[`docs/perf-report.md`](./docs/perf-report.md)

## API 文档

1. Runtime API：[`docs/api/runtime.md`](./docs/api/runtime.md)
2. Host SDK 指南：[`docs/host-sdk.md`](./docs/host-sdk.md)
3. 宿主嵌入示例：[`examples/host-embed/README.md`](./examples/host-embed/README.md)
4. Backend 契约：[`docs/api/backend.md`](./docs/api/backend.md)
5. Pack API：[`docs/api/pack.md`](./docs/api/pack.md)
6. API 索引：[`docs/api/README.md`](./docs/api/README.md)

## 开发约束

1. 运行时代码遵循 C89（禁止 C99/C11 语法特性）。
2. API 变更需同步更新 `docs/api/*` 与 `issue.md`。
3. 性能优化必须可开关、可回退、可对照验证。
