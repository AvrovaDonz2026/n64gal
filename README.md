# N64GAL

N64GAL 是一个面向 Galgame/VN 的实验性引擎原型，核心目标是：

1. 前后端分离。
2. 单一前后端 API 契约。
3. C89 严格兼容。
4. 便于跨架构后端扩展（amd64/AVX2 -> arm64/NEON -> riscv64/RVV）。

当前代码以“库优先”方式组织：核心能力通过 `vn_runtime.h` 暴露，`vn_player` 仅作为可选 CLI 包装。

## 项目状态（2026-03-06）

1. 已完成：
   - `scalar` 后端最小闭环（默认 `600x800`，场景 `S0-S3`）。
   - Frontend 输出统一 `VNRenderOp[]`。
   - `vn_runtime_run(config, result)` 结构化运行入口。
   - Session API：`create/step/is_done/set_choice/destroy`。
2. 进行中：
   - AVX2 从桩实现升级为真实算子路径。
   - 输入抽象层进一步统一（键盘输入与脚本化输入）。

详细路线图见 [issue.md](./issue.md) 与 [dream.md](./dream.md)。

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
tools/scriptc/  场景脚本编译器
tools/packer/   vnpak 打包工具
tests/unit/     单元测试
tests/perf/     基准脚本与 CSV 输出
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

默认仅构建 `vn_core` 与测试目标。  
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
  src/core/runtime_cli.c \
  src/frontend/render_ops.c \
  src/backend/avx2/avx2_backend.c \
  src/backend/scalar/scalar_backend.c \
  -o /tmp/vn_player
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
9. `--keyboard`（类 Unix TTY）
10. `--quiet`

键盘模式按键：

1. `1-9` 设置分支索引
2. `t` 切换 trace
3. `q` 退出循环

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

```bash
./tests/perf/run_perf.sh --backend scalar --scenes S0,S1,S2,S3 --duration-sec 120 --warmup-sec 20 --dt-ms 16 --resolution 600x800
```

会在 `tests/perf/` 下生成：

1. `perf_<scene>.csv`
2. `perf_summary.csv`（含每场景 `p95_frame_ms`）
3. `perf_report_template.md`

## 后端支持状态

1. `scalar`：可用，作为行为基线与回退目标。
2. `avx2`：最小可运行实现已接入（`CLEAR/SPRITE/TEXT/FADE`），CPU 不支持时自动回退 `scalar`。
3. `neon`：预留。
4. `rvv`：预留。

## API 文档

1. Runtime API：[`docs/api/runtime.md`](./docs/api/runtime.md)
2. Backend 契约：[`docs/api/backend.md`](./docs/api/backend.md)
3. Pack API：[`docs/api/pack.md`](./docs/api/pack.md)
4. API 索引：[`docs/api/README.md`](./docs/api/README.md)

## 开发约束

1. 运行时代码遵循 C89（禁止 C99/C11 语法特性）。
2. API 变更需同步更新 `docs/api/*` 与 `issue.md`。
3. 性能优化必须可开关、可回退、可对照验证。
