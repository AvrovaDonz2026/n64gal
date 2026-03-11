# N64GAL API Docs

本目录维护 N64GAL 对外 API 文档。

## 文档索引

1. `runtime.md`
   - 运行时入口 API（`vn_runtime_run` / `vn_runtime_run_cli`）
   - 会话 API（`vn_runtime_session_create/step/is_done/set_choice/inject_input/destroy`）
   - 运行配置与结果结构
   - 调用示例与返回码约定
2. `backend.md`
   - 前后端统一契约 API（`vn_backend.h`）
   - 后端注册、选择、回退规则
   - Render IR 字段约束
3. `pack.md`
   - 资源包 API（`vn_pack.h`）
   - `vnpak` v1/v2 格式与兼容策略
   - CRC32 与一致性校验行为
4. `save.md`
   - `vnsave` probe API（`vn_save.h`）
   - `vnsave v1` header/version 探测与结构化拒绝规则
   - 当前 `pre-1.0` 边界
5. `../errors.md`
   - 公共错误码（`vn_error.h`）
   - `vn_error_name(int)` 稳定字符串映射
   - runtime / preview / CI 的统一错误语义
6. `dirty-tile-draft.md`
   - `ISSUE-008` Dirty-Tile 增量渲染现状与设计稿
   - 已落地 API / 仍待冻结项（runtime / renderer / backend）
   - 实施顺序、回退条件与验证要求
7. `../preview-protocol.md`
   - 预览协议 `v1`
   - `vn_previewd` / `vn_preview_run_cli`
   - editor / CI / automation 统一入口
8. `../host-sdk.md`
   - 宿主 SDK 接入指南
   - Session API 调用序列、输入/文件/日志桥接
   - 版本协商矩阵与嵌入边界
9. `../platform-matrix.md`
   - Linux/Windows/riscv64 平台矩阵
   - 内部平台层当前收口范围
   - 构建、测试与验证路线
10. `compat-log.md`
   - API 兼容记录模板
   - 当前公开 surface 的变化轨迹
   - `1.0.0` 前兼容说明入口

## 维护规则

1. 每次改动 `include/vn_runtime.h`、`include/vn_preview.h`、`include/vn_backend.h`、`include/vn_pack.h`、`include/vn_save.h` 或 `include/vn_error.h`，必须同步更新本目录文档；涉及宿主集成语义时还必须同步更新 `../host-sdk.md`。
2. API 语义变更必须追加“兼容性说明”，不能只改代码不改文档。
3. 当前仓库已提供 `scripts/check_api_docs_sync.sh`；公开 surface 变化时，还必须同步更新 `compat-log.md`。
4. 文档示例默认使用 C89 语法。
