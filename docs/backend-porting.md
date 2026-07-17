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
5. `src/core/runtime_input.c` + `src/core/runtime_parse.c` + `src/core/runtime_cli.c` + `src/core/runtime_session_support.c`
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
7. `get_framebuffer`
8. 可选 `submit_ops_dirty`

当前最小操作集必须支持：

1. `VN_OP_CLEAR`
2. `VN_OP_SPRITE`
3. `VN_OP_TEXT`
4. `VN_OP_FADE`

`VN_OP_SPRITE` / `VN_OP_TEXT` 还必须处理带 `VN_OP_FLAG_RESOURCE_TEXTURE` 的真实图片 op。后端通过已绑定的 `VNTextureLookupFn` 取得 `RGBA16`、`CI8` 或 `IA8` view；可直接复用 `src/backend/common/pixel_pipeline.c` 的最近邻采样和 alpha 合成。`get_framebuffer` 返回当前只读 ARGB8888 framebuffer，供 renderer frame view 使用。

资源背景 crossfade 是一个不可拆分的相邻 op pair：

1. FROM 使用 `VN_OP_FLAG_RESOURCE_CROSSFADE_FROM`（`0x40u`），TO 使用 `VN_OP_FLAG_RESOURCE_CROSSFADE_TO`（`0x20u`）。
2. FROM/TO 都必须是带 `VN_OP_FLAG_RESOURCE_TEXTURE` 的 `VN_OP_SPRITE`，且 `layer/x/y/w/h` 完全相同。
3. FROM 后必须立即是 TO，二者 `alpha` 互补且和为 255；FROM 在数组末尾、独立 TO、额外 flag 或几何不一致都属于 malformed pair。
4. full submit 和 dirty submit 都必须调用公共 `vn_pp_draw_resource_crossfade(...)` 一次并跳过已消费的 TO。该 helper 负责透明感知的 premultiplied linear crossfade 和 clip；后端不得把 pair 当作两个普通 resource op 分别绘制。
5. malformed pair 必须返回 `VN_E_FORMAT`。内建后端和任何新后端都不得静默忽略、重新排序或降级执行。

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
5. `test_resource_texture_backend`
6. 如涉及 fast path 边界，再补 targeted parity test

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
   - resource crossfade pair 统一转发到 `vn_pp_draw_resource_crossfade(...)`

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

### 6.4 resource crossfade pair

接入新后端时，先在 submit loop 中识别 FROM/TO pair，再进入普通 resource texture 分支。识别逻辑必须同时覆盖 full 与 dirty 路径，并把 clip rect 原样交给 `vn_pp_draw_resource_crossfade(...)`。公共 helper 返回的 `VN_E_FORMAT` 或资源错误必须直接向上传播；不要在 ISA 私有路径复制一套透明混合公式，否则 opaque、RGBA16 1-bit alpha、CI8 palette alpha 与 IA8 alpha 很容易发生语义漂移。

最低限度要验证：相同 opaque 图在过渡中点不变暗、透明 texel 按 premultiplied linear 规则连续变化、FROM/TO 端点正确、dirty/full framebuffer 一致，以及缺失 TO、独立 TO、flag/几何/alpha 不匹配都返回 `VN_E_FORMAT`。

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
