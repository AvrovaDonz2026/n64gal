# Backend API (`vn_backend.h`)

## 1. 目标

统一前后端契约，确保跨架构迁移时 Frontend 代码不变。

## 2. 核心类型

### `VNRenderOp`

Render IR 单条指令。

字段约束：

1. `op`: `clear/sprite/text/fade`（当前最小集）
2. `layer`: 图层索引
3. `tex_id`: 纹理/字图索引
4. `x/y/w/h`: 位置与尺寸
5. `alpha`: 透明度
6. `flags`: 扩展语义位

### `VNBackendCaps`

后端能力位：

1. `has_simd`
2. `has_lut_blend`
3. `has_tmem_cache`

### `VNRenderBackend`

后端函数表：

1. `init`
2. `shutdown`
3. `begin_frame`
4. `submit_ops`
5. `end_frame`
6. `query_caps`

## 3. 后端注册与选择

函数：

1. `vn_backend_register`
2. `vn_backend_select`
3. `vn_backend_get_active`
4. `vn_backend_reset_registry`

默认优先级：

1. `avx2`
2. `neon`
3. `rvv`
4. `scalar`

补充说明：

1. `avx2_asm` 当前不属于默认优先级链，也不包含在 `VN_ARCH_MASK_ALL` 中。
2. `avx2_asm` 仅能通过 `VN_RENDERER_FLAG_FORCE_AVX2_ASM` 或 CLI `--backend=avx2_asm` 显式请求。

失败回退：

1. 自动模式按 `avx2 -> neon -> rvv -> scalar` 顺序逐个尝试初始化。
2. 任一候选初始化失败时，必须继续尝试下一候选，最终确保 `scalar` 可回退。
3. 强制模式下若所请求的 SIMD 后端初始化失败，也必须回退到 `scalar`。
4. `avx2_asm` 当前视为实验性 force-only 后端；即使在 x64 平台上已注册，也不得被 auto 路径选中。
5. 回退路径必须可运行并记录日志。

## 4. 扩展后端实现约定

1. 新后端仅实现 `VNRenderBackend` 接口，不改 Frontend。
2. ISA 私有头文件不得泄漏到 Frontend。
3. 行为一致性以 `scalar` 为基准。

## 5. 当前实现状态（2026-03-09）

1. `scalar`：完整基线实现，可作为默认回退后端。
2. `avx2`：已实现最小可运行链路；当前工程判断已进入“稳定后维护优化”阶段。最近一轮已把 direct textured row 的 non-palette opaque/blend 路径接到 8-lane chunk `sample/hash -> combine -> blend` 主线，又把 row-palette build 收口到 8-lane chunk 生成，并把 `use_row_palette` 先策略化成独立 helper；同时补上 x64 suite workflow 的 `matched backend=avx2` 显式校验、perf artifact 的 `requested_backend/actual_backend` 记账、`avx2_dirty` golden 对照、`256/257` 边界 fast-path parity 单测，以及 `test_backend_priority` 对真实后端选择路径的直接断言。结构上也已新增 `avx2_internal.h` / `avx2_textured.c` / `avx2_fill_fade.c`，并同步到 `CMake/ci/perf` 构建入口，把 textured hot path 与 fill/fade 路径从 backend glue 中拆出；剩余重点转向 `S10`/kernel perf threshold 之后的维护覆盖与 duplicated pixel-pipeline 语义的长期防漂移；Linux x64 侧也已开始补 `CC=clang` 的 suite 证据；Windows x64 侧则新增了 `MSVC Debug compile-only`，并继续尝试补 `ClangCL Debug compile-only` 证据，但非 AVX2 x64 主机上的安全回退仍需额外实机证明。
3. `avx2_asm`：实验性 force-only 变体，当前与 `avx2` 共享同一套 framebuffer / textured / dirty-submit 主路径，但只在 GNU x64 且 ASM fill 可用时允许初始化；否则强制请求也会按现有 forced backend 语义回退到 `scalar`。当前阶段它稳定覆盖的是 `clear/fill(alpha=255)` 热路径，本地最新稳定 `kernel avx2 -> avx2_asm` 样本里 `clear_full p95 0.797ms -> 0.105ms`，而其它非 fill kernel 基本持平，因此它暂时仍是“高风险、高收益但覆盖面很窄”的实验后端。auto 选择链故意不纳入 `avx2_asm`。
4. `neon`：已接入最小可运行链路；`fill`、uniform alpha/fade，以及宽行 `SPRITE/TEXT` 的 row-palette 写回路径已使用 NEON/NEON-assisted 实现；最近一轮已补热循环常量外提、`div255` 预加载、palette-row no-stack lane pack、更保守的大矩形 row-palette 启发式，以及与 AVX2 对齐的 `row params + local sample/combine/blend` textured-row 热路径收口；随后又把 `sample/hash -> combine` 做成 4-lane chunk 内核，把 textured alpha blend 收口到 packed-channel 向量 helper，并把 row 级 `seed/checker/base_rgb/text_blue_bias` 常量前折叠到 params。最新一轮又把 row-palette alpha path 切到预打包 `full/RB/G` palette、给 opaque row-palette 补了整行缓存复用，并继续把 repeated-`v8` 的半透明 row-palette 接上 `RB/G` 行缓存复用；`uniform blend/fade` row kernel 也已收口到 packed `RB/G` 两路版本，降低大矩形 apply 与全屏 fade 的重复成本；当前 direct row opaque/blend 与 row-palette build 已复用同一核心，最近又把 `u_lut` lane-load 先切到 `vld1_u8 + vmovl` 并补 `u_lut` 专用尾部 padding（`v_lut` 侧 `tail_pad=0` 以避免越界写），alpha repeated-v8 row-cache 构建也已改成 4-lane chunk helper，且 `RB/G` gather 已合并到单 helper；后续重点继续落在 row-palette gather/apply 与更宽 chunk 评估，目标架构外返回 `VN_E_UNSUPPORTED`。
5. `rvv`：已接入最小可运行链路，`fill`、不透明矩形填充、统一颜色半透明 `fade/fill`，以及 `SPRITE/TEXT` 的 `tex/hash -> combine -> alpha` 路径使用 RVV 向量写入；其中 `sample -> combine` 已融合为单次行内向量流水，目标架构外返回 `VN_E_UNSUPPORTED`。`riscv64` 交叉构建、`qemu-user` 冒烟与 `scalar vs rvv` CRC 对照已在本地验证。

实现说明：

1. `avx2` 在 `init` 阶段做运行时检测（仅 CPU 支持 AVX2 时启用）。
2. 支持 `VN_OP_CLEAR/VN_OP_SPRITE/VN_OP_TEXT/VN_OP_FADE` 四类指令。
3. `CLEAR` 与不透明矩形填充使用 AVX2 向量写入；alpha 混合路径使用标量逐像素混合。
4. 当强制选择 `avx2` 但当前 CPU 不支持时，渲染器会自动回退到 `scalar`。
5. 当强制选择 `avx2_asm` 时，当前实现会先要求 GNU x64 ASM fill 真正可用；若该条件不满足，则初始化失败并按现有 forced backend 规则回退到 `scalar`，而 auto 模式仍继续选择普通 `avx2`。
6. `SPRITE/TEXT` 走统一的 `tex -> combine` 采样链路（共享 `pixel_pipeline`），保证 `scalar/avx2` 输出语义一致。
7. `SPRITE/TEXT` 纹理坐标映射使用 8-bit UV LUT（每帧按可见区域构建）以减少逐像素除法开销，并进一步压低 LUT 带宽与缓存占用。
8. `rvv` 当前已将 `tex/hash` 采样与 `combine` 融合成单次行内向量流水，`alpha=255` 时直接写 framebuffer，`alpha<255` 时也已切到单循环 `sample -> combine -> blend/store`；UV LUT 也已收口到 8-bit 存储，且 `seed/checker` 常量与 layer/flag 基础偏置已前折叠到行级参数。后续优化重点转为可重复 perf 证据沉淀与更进一步的寄存器压力优化。

## 6. 后端能力位约定

1. `scalar`: `has_simd=0`, `has_lut_blend=0`, `has_tmem_cache=0`
2. `avx2`（当前阶段）: `has_simd=1`, `has_lut_blend=0`, `has_tmem_cache=0`
3. `neon`（当前阶段）: `has_simd=1`, `has_lut_blend=0`, `has_tmem_cache=0`
4. `rvv`（当前阶段）: `has_simd=1`, `has_lut_blend=0`, `has_tmem_cache=0`

## 7. 一致性验证

1. 新增 `test_backend_consistency`：同一组 `VNRenderOp` 在 `scalar` 与 `avx2` 下渲染后比较 framebuffer CRC32。
2. 新增 `test_renderer_dirty_submit`：对同一组前后帧，校验 `scalar`、`avx2`、`neon`、`rvv` 的 dirty submit 都与整帧提交 CRC 一致；当前主机不支持的 ISA 自动跳过，`riscv64 qemu` smoke 还会额外执行 RVV 版二进制。
3. 新增 `test_runtime_golden`：真实 `S0/S1/S2/S3/S10` 场景在 `600x800` 下固定标量 golden CRC，当前基线为 `S0=0x58C8928B`、`S1=0x80D7F175`、`S2=0x587BC5A4`、`S3=0x0BC0160F`、`S10=0xC9A161B9`，并在支持的平台上对照 `avx2/neon/rvv`。
4. `test_runtime_golden` 对 `scalar` 继续要求 CRC 严格命中；对支持的 SIMD 后端则按 `mismatch_percent < 1%` 且 `max_channel_diff <= 8` 判定，并在出现差异或 CRC 异常时导出 `expected/actual/diff` PPM 与 `test_runtime_golden_<scene>_<backend>_summary.txt`，便于直接定位首个差异点与阈值命中情况。最近一轮又给 `avx2` 补了 `VN_RUNTIME_PERF_DIRTY_TILE` 下的 `avx2_dirty` golden 对照。若设置 `VN_GOLDEN_ARTIFACT_DIR`，这些产物会统一写入该目录；CI suite 脚本已用这条约定收集 artifact。
5. 新增 `test_avx2_fastpath_parity`：用 targeted render-op case 直接比较 `scalar` 与 AVX2 的 fast path framebuffer，当前已覆盖 `SPRITE/TEXT`、opaque/alpha，以及 `256/257` 的 row-palette 边界。
6. 新增 `test_backend_priority` 与 `test_renderer_fallback` 的最小 `avx2_asm` 覆盖：前者断言 `avx2_asm` 只能被 force 选中且 `SIMD auto` 不得选到它，后者断言强制 `avx2_asm` 在不支持主机上仍会回退到 `scalar`。
7. `avx2_asm` 现已额外接入 `test_renderer_dirty_submit`、`test_backend_consistency` 与 `test_runtime_golden`：当前本地 `run_cc_suite` 已复核 `avx2_asm`、`avx2_asm_dirty` 对 `S0/S1/S2/S3/S10` 全部与 scalar 对齐。
8. `test_runtime_api` 与 `test_preview_protocol` 现也补入 `S10` 端到端覆盖，直接校验 `scene_name -> pack -> VM -> frontend` 路径会落到 6-op 压力场景，并在库结果与外部协议输出里保留 `scene_name/op_count/bgm_id` 等关键字段。
9. 当机器不支持某个 SIMD 后端时，相关 golden 对照会自动跳过，不把当前主机不支持的 ISA 记作失败。
10. `riscv64` 当前采用两级验证：先做交叉构建，再通过 `scripts/ci/run_riscv64_qemu_suite.sh` 在 `qemu-user` 下验证 `scalar` 回退链、`rvv` 冒烟执行，以及 `test_runtime_golden` 的 golden 容差对照 / `scalar vs rvv` CRC 一致性。

## 8. Dirty-Tile 扩展现状

1. `vn_backend.h` 已新增统一的 `VNRenderRect` / `VNRenderDirtySubmit` 与可选 `submit_ops_dirty` 回调，前后端仍保持“一份 API，多后端实现”。
2. 当前 `scalar`、`avx2`、`neon`、`rvv` 已实现 dirty submit，分别覆盖语义基线、x64 主力路径、arm64 主力路径与 `riscv64` 向量后端。
3. 共享设计与后续分阶段推进仍见 [`dirty-tile-draft.md`](./dirty-tile-draft.md)。
