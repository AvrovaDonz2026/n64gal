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

失败回退：

1. 自动模式按 `avx2 -> neon -> rvv -> scalar` 顺序逐个尝试初始化。
2. 任一候选初始化失败时，必须继续尝试下一候选，最终确保 `scalar` 可回退。
3. 回退路径必须可运行并记录日志

## 4. 扩展后端实现约定

1. 新后端仅实现 `VNRenderBackend` 接口，不改 Frontend。
2. ISA 私有头文件不得泄漏到 Frontend。
3. 行为一致性以 `scalar` 为基准。

## 5. 当前实现状态（2026-03-06）

1. `scalar`：完整基线实现，可作为默认回退后端。
2. `avx2`：已实现最小可运行链路。
3. `neon`：已接入最小可运行链路，`fill` 与不透明矩形填充路径使用 NEON 向量写入，目标架构外返回 `VN_E_UNSUPPORTED`。
4. `rvv`：已接入最小可运行链路，`fill`、不透明矩形填充、统一颜色半透明 `fade/fill`，以及 `SPRITE/TEXT` 的 `tex/hash -> combine -> alpha` 路径使用 RVV 向量写入；其中 `sample -> combine` 已融合为单次行内向量流水，目标架构外返回 `VN_E_UNSUPPORTED`。`riscv64` 交叉构建、`qemu-user` 冒烟与 `scalar vs rvv` CRC 对照已在本地验证。

实现说明：

1. `avx2` 在 `init` 阶段做运行时检测（仅 CPU 支持 AVX2 时启用）。
2. 支持 `VN_OP_CLEAR/VN_OP_SPRITE/VN_OP_TEXT/VN_OP_FADE` 四类指令。
3. `CLEAR` 与不透明矩形填充使用 AVX2 向量写入；alpha 混合路径使用标量逐像素混合。
4. 当强制选择 `avx2` 但当前 CPU 不支持时，渲染器会自动回退到 `scalar`。
5. `SPRITE/TEXT` 走统一的 `tex -> combine` 采样链路（共享 `pixel_pipeline`），保证 `scalar/avx2` 输出语义一致。
6. `SPRITE/TEXT` 纹理坐标映射使用 8-bit UV LUT（每帧按可见区域构建）以减少逐像素除法开销，并进一步压低 LUT 带宽与缓存占用。
7. `rvv` 当前已将 `tex/hash` 采样与 `combine` 融合成单次行内向量流水，`alpha=255` 时直接写 framebuffer，`alpha<255` 时也已切到单循环 `sample -> combine -> blend/store`；UV LUT 也已收口到 8-bit 存储，且 `seed/checker` 常量与 layer/flag 基础偏置已前折叠到行级参数。后续优化重点转为可重复 perf 证据沉淀与更进一步的寄存器压力优化。

## 6. 后端能力位约定

1. `scalar`: `has_simd=0`, `has_lut_blend=0`, `has_tmem_cache=0`
2. `avx2`（当前阶段）: `has_simd=1`, `has_lut_blend=0`, `has_tmem_cache=0`
3. `neon`（当前阶段）: `has_simd=1`, `has_lut_blend=0`, `has_tmem_cache=0`
4. `rvv`（当前阶段）: `has_simd=1`, `has_lut_blend=0`, `has_tmem_cache=0`

## 7. 一致性验证

1. 新增 `test_backend_consistency`：同一组 `VNRenderOp` 在 `scalar` 与 `avx2` 下渲染后比较 framebuffer CRC32。
2. 新增 `test_runtime_golden`：真实 `S0-S3` 场景在 `600x800` 下固定标量 golden CRC，当前基线为 `S0=0x58C8928B`、`S1=0x80D7F175`、`S2=0x587BC5A4`、`S3=0x0BC0160F`，并在支持的平台上对照 `avx2/neon/rvv`。
3. `test_runtime_golden` 对 `scalar` 继续要求 CRC 严格命中；对支持的 SIMD 后端则按 `mismatch_percent < 1%` 且 `max_channel_diff <= 8` 判定，并在出现差异或 CRC 异常时导出 `expected/actual/diff` PPM 与 `test_runtime_golden_<scene>_<backend>_summary.txt`，便于直接定位首个差异点与阈值命中情况。若设置 `VN_GOLDEN_ARTIFACT_DIR`，这些产物会统一写入该目录；CI suite 脚本已用这条约定收集 artifact。
4. 当机器不支持某个 SIMD 后端时，相关 golden 对照会自动跳过，不把当前主机不支持的 ISA 记作失败。
5. `riscv64` 当前采用两级验证：先做交叉构建，再通过 `scripts/ci/run_riscv64_qemu_suite.sh` 在 `qemu-user` 下验证 `scalar` 回退链、`rvv` 冒烟执行，以及 `test_runtime_golden` 的 golden 容差对照 / `scalar vs rvv` CRC 一致性。
