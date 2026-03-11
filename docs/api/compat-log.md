# API Compatibility Log

## 1. 目标

这份文档记录公开 surface 的兼容性变化，避免 release note、README 和 API 文档只能描述“当前状态”，却没有变化轨迹。

当前记录范围：

1. `vn_runtime.h`
2. `vn_preview.h` / `preview protocol`
3. `vn_pack.h`
4. `vn_save.h`
5. `vn_error.h`
6. `vn_backend.h` 的公开边界说明

## 2. 记录规则

每条记录至少包含：

1. 日期
2. 阶段或版本
3. surface
4. type
   - `additive`
   - `behavior`
   - `compat-note`
   - `breaking`
5. summary
6. required action

## 3. 当前记录

### 2026-03-11 / `pre-v1.0.0`

1. Surface: `vn_error.h`
   - Type: `additive`
   - Summary: 新增 `vn_error_name(int)`，统一公开错误码字符串映射
   - Required action: 宿主和工具侧不再维护私有错误名表

2. Surface: `preview protocol v1`
   - Type: `additive`
   - Summary: 顶层响应与 `events[]` 开始输出稳定 `trace_id`
   - Required action: 外部工具优先读取 `trace_id + error_code + error_name`

3. Surface: `vn_runtime_run_cli`
   - Type: `behavior`
   - Summary: 参数错误与运行失败开始输出 machine-readable `trace_id/error_code/error_name`
   - Required action: 解析 CLI 输出时不要依赖自然语言 message

4. Surface: `vn_save.h`
   - Type: `additive`
   - Summary: 新增 `vnsave` probe API 与最小 `v0 -> v1` 迁移函数
   - Required action: 当前只能把它视为 probe/migrate 工具面，不是完整 save/load ABI

5. Surface: `vnsave` version policy
   - Type: `compat-note`
   - Summary: `v0.x` 不承诺 save 兼容；`v1.0.0` 才允许首次公开 `vnsave v1`
   - Required action: release note 必须明确写出支持/迁移/拒绝策略
