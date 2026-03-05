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
