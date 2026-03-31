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

### 2026-03-31 / `pre-v1.0.0`

1. Surface: `vn_runtime.h`
   - Type: `additive`
   - Summary: 新增 `VNRuntimeBuildInfo` 与 `vn_runtime_query_build_info(...)`，把 runtime/preview/pack/save/host 的版本协商从纯文档推进成可查询公开面
   - Required action: 宿主与工具若需要协商当前 build 边界，应优先读取该 API，而不是只依赖 README

2. Surface: `vn_pack.h`
   - Type: `additive`
   - Summary: 新增 `VNPAK_VERSION_1/2`、`VNPAK_READ_MIN_VERSION`、`VNPAK_READ_MAX_VERSION` 与 `VNPAK_WRITE_DEFAULT_VERSION`
   - Required action: 运行时版本协商或宿主诊断不再需要私自复制 `vnpak` 版本常量

3. Surface: `vn_preview.h`
   - Type: `additive`
   - Summary: 新增 `VN_PREVIEW_PROTOCOL_VERSION`
   - Required action: 预览工具可直接复用公开协议版本常量

4. Surface: `vn_save.h`
   - Type: `additive`
   - Summary: 新增 `VNSAVE_API_STABILITY`
   - Required action: 宿主可直接读取当前 save surface 的稳定级别，不再只靠 release 文档推断

5. Surface: `vn_runtime.h`
   - Type: `additive`
   - Summary: 新增 `VNRuntimeSessionSnapshot`、`vn_runtime_session_capture_snapshot(...)` 与 `vn_runtime_session_create_from_snapshot(...)`，公开最小会话恢复 ABI
   - Required action: 宿主若要实现 quick-save / quick-load 原型，应优先通过 snapshot API 捕获/恢复 live session，而不是直接读取内部 `VNState`

6. Surface: `vn_runtime.h`
   - Type: `additive`
   - Summary: 新增 `vn_runtime_session_save_to_file(...)` 与 `vn_runtime_session_load_from_file(...)`，把最小文件级会话恢复接到 `vnsave v1` 外壳上
   - Required action: 若宿主需要最小文件级 quick-save / quick-load，应优先复用这组 API，而不是自行定义另一套临时存档封装

7. Surface: `vn_runtime_run_cli`
   - Type: `additive`
   - Summary: 新增 `--load-save=<path>`，允许直接从 runtime session save/load draft 文件恢复继续运行
   - Required action: CLI 工具和自动化脚本若需要恢复运行，应优先使用 `--load-save`；同时不要把它与 `--scene/--pack/--backend/--frames/...` 之类运行配置混用

8. Surface: `vn_runtime_run_cli`
   - Type: `additive`
   - Summary: 新增 `--save-out=<path>`，允许在一次 CLI run 结束后直接写出 runtime session save/load draft 文件
   - Required action: 若 CLI 需要输出可恢复会话，应优先使用 `--save-out`；若与 `--load-save` 组合，可形成“读一个 save，继续推进，再写回一个 save”的最小工作流
