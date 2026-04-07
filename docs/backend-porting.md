# Backend Porting Guide

## 1. 目标

这份文档面向新后端实现者，说明如何在不修改 Frontend 语义的前提下，为 N64GAL 增加新的渲染后端。

核心原则：

1. Frontend 只输出 `VNRenderOp[]`
2. 后端只消费统一的 `vn_backend.h` 契约
3. 行为一致性以 `scalar` 为基准
4. 所有优化必须可回退、可验证、可比较
5. 运行时代码保持 `C89`

## 2. 最小接入面

新后端最少需要补这几层：

1. `include/vn_backend.h`
   - 若需要新架构标签，补 `VN_ARCH_*` 与 mask
2. `include/vn_renderer.h`
   - 若需要 force 入口，补 `VN_RENDERER_FLAG_FORCE_*`
3. `src/core/backend_registry.c`
   - 接入后端选择逻辑
4. `src/core/renderer.c`
   - 注册函数声明
   - 初始化顺序
   - force fallback 语义
5. `src/core/runtime_parse.c` + `src/core/runtime_cli.c`
   - `--backend=<name>` 解析
   - runtime CLI 执行路径
6. `src/tools/preview_parse.c` + `src/tools/preview_cli.c`
   - preview usage / request backend 名称
7. `src/backend/<arch>/...`
   - 真正的后端实现

## 3. 后端契约

每个后端都实现一份 `VNRenderBackend`：

1. `init`
2. `shutdown`
3. `begin_frame`
4. `submit_ops`
5. `end_frame`
6. `query_caps`
7. 可选 `submit_ops_dirty`

当前最小操作集必须支持：

1. `VN_OP_CLEAR`
2. `VN_OP_SPRITE`
3. `VN_OP_TEXT`
4. `VN_OP_FADE`

## 4. 推荐实现顺序

### 第 1 阶段：最小可运行

先做：

1. framebuffer 分配 / shutdown
2. `clear`
3. `fade`
4. `sprite/text` 的标量或半向量路径
5. `renderer_backend_name()` 可正确返回
6. force fallback 正常

不要一上来就做：

1. 复杂汇编
2. heuristic 调参
3. 多条实验路径并存

### 第 2 阶段：一致性

新后端接入后，至少要过：

1. `test_renderer_fallback`
2. `test_renderer_dirty_submit`
3. `test_backend_consistency`
4. `test_runtime_golden`
5. 如涉及 fast path 边界，再补 targeted parity test

### 第 3 阶段：性能

先做 report-only 证据，再考虑门限：

1. `tests/perf/run_perf_compare.sh`
2. `tests/perf/run_kernel_compare.sh`
3. GitHub runner artifact
4. host CPU 留痕

## 5. 代码组织建议

建议按这三层拆：

1. `backend glue`
   - init / shutdown / registration / dirty dispatch
2. `fill/fade`
   - clear / fill / uniform blend
3. `textured`
   - uv lut
   - sample/combine
   - row-palette
   - direct row

当前仓库里的参考：

1. `scalar`
   - 简单基线实现
2. `avx2`
   - 最成熟的 x64 主后端
3. `neon`
   - 当前热点优化最积极的一条线
4. `rvv`
   - qemu-first 的功能 bring-up 参考

## 6. 常见设计边界

### 6.1 自动选择 vs force-only

如果新后端仍在实验阶段：

1. 允许 `--backend=<new_backend>`
2. 不要纳入 `VN_ARCH_MASK_ALL`
3. 只走 force-only
4. init 失败时回退 `scalar`

这是当前 `avx2_asm` 的做法。

### 6.2 dirty submit

只要后端进入主线，最终都应该支持：

1. `submit_ops_dirty`
2. `full_redraw` 回退
3. rect loop clip 提交

### 6.3 golden 容差

SIMD 后端默认不要求完全按 CRC 等于 `scalar`，但要求：

1. `mismatch_percent < 1%`
2. `max_channel_diff <= 8`

如果某后端能做到 exact match，更好，但不要为了看起来干净而牺牲主线收益。

## 7. 最小验证清单

新增后端后，至少手工跑一遍：

```bash
./scripts/check_c89.sh
./scripts/ci/run_cc_suite.sh
./tests/perf/run_kernel_bench.sh --backend <backend> --iterations 8 --warmup 2 --out-csv /tmp/kernel.csv
./tests/perf/run_perf_compare.sh --baseline scalar --candidate <backend> --scenes S1,S3,S10 --duration-sec 2 --warmup-sec 1 --dt-ms 16 --resolution 600x800 --out-dir /tmp/perf_compare
```

若是跨架构后端，还应补：

1. 目标交叉编译
2. 原生 runner 或 qemu 路线
3. CI artifact 留痕

## 8. 当前建议

对未来新后端或实验后端：

1. 先走 C 路径 bring-up
2. 再做 SIMD
3. 最后才考虑汇编或更激进的实验分支

当前仓库经验已经很明确：

1. 稳定主线收益主要来自 `avx2` / `neon` C 后端
2. 汇编后端只适合 force-only 实验入口
3. 没有持续 artifact 证据前，不要把实验后端提到 auto 优先级
