# N64GAL

N64GAL 是一个面向 Galgame / VN 的 C89 引擎原型。

当前主线重点：

1. 库优先：核心能力通过 `vn_runtime.h` 暴露，`vn_player` 只是可选 CLI 包装。
2. 单一渲染契约：Frontend 输出统一 `VNRenderOp[]`，后端按 ISA 执行。
3. 跨架构主线：`x64/avx2`、`arm64/neon`、`riscv64/rvv(qemu-first)`。
4. 可验证：runtime / preview / pack / save / toolchain 都有测试和 validator。

## 当前范围

当前仓库已经稳定落地并有回归覆盖的主路径：

1. `vn_runtime_run(...)`
2. Session API：`create/step/is_done/set_choice/inject_input/destroy`
3. Runtime build-info / snapshot / file save-load draft API
4. `vn_previewd` / `vn_preview_run_cli(...)`
5. `vnsave` 的 `probe` / `migrate`
6. `tools/toolchain.py validate-all`

当前场景名不是任意字符串，而是固定集合：`S0/S1/S2/S3/S10`。

首个 `v1.0.0` 当前只承诺：

1. `x64 Linux`
2. `x64 Windows`
3. `arm64 Linux`
4. `arm64 Windows`
5. `scalar`
6. `avx2`
7. `neon`

不纳入首个 `v1.0.0` 默认承诺：

1. `riscv64 native / RVV` 发布级支持
2. `avx2_asm` 默认优先级
3. `JIT`
4. `SSE2` / `x64 no-AVX2` 优化路径

详细发布边界见 [docs/release-roadmap-1.0.0.md](./docs/release-roadmap-1.0.0.md) 和 [docs/compat-matrix.md](./docs/compat-matrix.md)。

## 代码优先

如果 README、文档与实现冲突，请优先看：

1. `include/*.h`
2. `examples/host-embed/*`
3. `tests/unit/test_runtime_api.c`
4. `tests/unit/test_runtime_session.c`
5. `tests/integration/test_preview_protocol.c`
6. `tools/toolchain.py`

## 仓库结构

```text
include/        对外头文件
src/core/       runtime / renderer / vm / save / pack
src/frontend/   Frontend -> RenderOp
src/backend/    scalar / avx2 / neon / rvv
src/tools/      preview 等无 GUI 工具入口
tools/          scriptc / packer / migrate / probe / validate / toolchain
tests/          unit / integration / perf
examples/       宿主嵌入示例
templates/      最小内容模板与宿主模板
docs/           API / perf / release / migration / governance
```

当前实现层也已经按职责拆开：

1. Runtime：`runtime_parse.c + runtime_cli.c + runtime_persist.c`
2. Preview：`preview_parse.c + preview_cli.c + preview_report.c`

## 快速开始

### 1. 生成 demo 资源

```bash
./tools/scriptc/build_demo_scripts.sh
./tools/packer/make_demo_pack.sh
```

输出默认写到：

1. `assets/demo/demo.vnpak`
2. `assets/demo/manifest.json`

### 2. 构建

推荐：

```bash
cmake -S . -B build -DVN_BUILD_PLAYER=ON
cmake --build build
```

直接用 `cc` 也可以：

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
  src/core/runtime_parse.c \
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

### 3. 运行

```bash
/tmp/vn_player --scene=S0 --backend=scalar --resolution=600x800 --frames=120 --dt-ms=16
```

常用参数：

1. `--scene=S0|S1|S2|S3|S10`
2. `--backend=auto|scalar|avx2|avx2_asm|neon|rvv`
3. `--resolution=600x800`
4. `--frames=<N>`
5. `--dt-ms=<N>`
6. `--trace`
7. `--keyboard`
8. `--quiet`
9. `--load-save=<save.vnsave>`
10. `--save-out=<save.vnsave>`
11. `--save-slot=<N>`
12. `--save-timestamp=<N>`

## Preview

`vn_previewd` 用于 editor / CI / 脚本驱动的结构化预览。

```bash
./build/vn_previewd \
  --project-dir=. \
  --scene=S2 \
  --resolution=600x800 \
  --frames=8 \
  --trace \
  --command=set_choice:1 \
  --command=inject_input:choice:1 \
  --command=step_frame:8
```

协议说明见 [docs/preview-protocol.md](./docs/preview-protocol.md)。

## Runtime API

一次性运行：

```c
#include "vn_runtime.h"

int run_once(void) {
    VNRunConfig cfg;
    VNRunResult res;

    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 120u;
    cfg.emit_logs = 0u;
    return vn_runtime_run(&cfg, &res);
}
```

Session 模式：

```c
#include "vn_runtime.h"

int run_session(void) {
    VNRunConfig cfg;
    VNRuntimeSession* s;
    VNRunResult res;
    int rc;

    vn_run_config_init(&cfg);
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

`vn_runtime.h` 当前还提供：

1. `vn_runtime_query_build_info(...)`
2. `capture_snapshot/create_from_snapshot`
3. `save_to_file/load_from_file`

这些 API 当前仍应按 `public v1-draft (pre-1.0)` 理解，正式边界见 [docs/api/runtime.md](./docs/api/runtime.md)。

## Save / Probe / Migrate

推荐统一从 toolchain 入口走：

```bash
python3 tools/toolchain.py migrate-vnsave --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave
python3 tools/toolchain.py probe-vnsave --in tests/fixtures/vnsave/v1/sample.vnsave
python3 tools/toolchain.py validate-all
```

如果已经用 CMake 构建，也可以直接运行：

```bash
./build/vnsave_migrate --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave
./build/vnsave_probe --in tests/fixtures/vnsave/v1/sample.vnsave
```

## 质量门禁

最常用的本地门禁：

```bash
./scripts/check_c89.sh
bash scripts/check_api_docs_sync.sh
python3 tools/toolchain.py validate-all
ctest --test-dir build --output-on-failure
```

perf 工具：

```bash
./tests/perf/run_perf.sh --backend scalar --scenes S0,S1,S2,S3,S10 --duration-sec 120 --warmup-sec 20 --dt-ms 16 --resolution 600x800
./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S1,S3,S10 --duration-sec 2 --warmup-sec 1 --dt-ms 16 --resolution 600x800 --out-dir /tmp/n64gal_perf_smoke
```

## 进一步阅读

1. API：`docs/api/*`
2. Runtime： [docs/api/runtime.md](./docs/api/runtime.md)
3. Backend： [docs/api/backend.md](./docs/api/backend.md)
4. Save： [docs/api/save.md](./docs/api/save.md)
5. Perf： [docs/perf-report.md](./docs/perf-report.md)
6. Release： [docs/release-roadmap-1.0.0.md](./docs/release-roadmap-1.0.0.md)
7. 兼容矩阵： [docs/compat-matrix.md](./docs/compat-matrix.md)
8. 宿主接入： [docs/host-sdk.md](./docs/host-sdk.md)
9. 项目跟踪： [issue.md](./issue.md)

`README` 只保留入口信息；里程碑、候选路线、长篇进度和细粒度 backlog 统一留在 [issue.md](./issue.md)。
