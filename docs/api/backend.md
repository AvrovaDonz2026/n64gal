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

1. 首选后端初始化失败，必须回退 `scalar`
2. 回退路径必须可运行并记录日志

## 4. 扩展后端实现约定

1. 新后端仅实现 `VNRenderBackend` 接口，不改 Frontend。
2. ISA 私有头文件不得泄漏到 Frontend。
3. 行为一致性以 `scalar` 为基准。

## 5. 当前实现状态（2026-03-06）

1. `scalar`：完整基线实现，可作为默认回退后端。
2. `avx2`：已实现最小可运行链路。

实现说明：

1. `avx2` 在 `init` 阶段做运行时检测（仅 CPU 支持 AVX2 时启用）。
2. 支持 `VN_OP_CLEAR/VN_OP_SPRITE/VN_OP_TEXT/VN_OP_FADE` 四类指令。
3. `CLEAR` 与不透明矩形填充使用 AVX2 向量写入；alpha 混合路径使用标量逐像素混合。
4. 当强制选择 `avx2` 但当前 CPU 不支持时，渲染器会自动回退到 `scalar`。
5. `SPRITE/TEXT` 走统一的 `tex -> combine` 采样链路（共享 `pixel_pipeline`），保证 `scalar/avx2` 输出语义一致。

## 6. 后端能力位约定

1. `scalar`: `has_simd=0`, `has_lut_blend=0`, `has_tmem_cache=0`
2. `avx2`（当前阶段）: `has_simd=1`, `has_lut_blend=0`, `has_tmem_cache=0`

## 7. 一致性验证

1. 新增 `test_backend_consistency`：同一组 `VNRenderOp` 在 `scalar` 与 `avx2` 下渲染后比较 framebuffer CRC32。
2. 当机器不支持 AVX2 时，该测试会输出 `skipped (no avx2 support)` 并通过。
