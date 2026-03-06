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
4. `dirty-tile-draft.md`
   - `ISSUE-008` Dirty-Tile 增量渲染设计稿
   - 最小 API 变更草案（runtime / renderer / backend）
   - 实施顺序、回退条件与验证要求
5. `../preview-protocol.md`
   - 预览协议 `v1`
   - `vn_previewd` / `vn_preview_run_cli`
   - editor / CI / automation 统一入口
6. `../host-sdk.md`
   - 宿主 SDK 接入指南
   - Session API 调用序列、输入/文件/日志桥接
   - 版本协商矩阵与嵌入边界
7. `../platform-matrix.md`
   - Linux/Windows/riscv64 平台矩阵
   - 内部平台层当前收口范围
   - 构建、测试与验证路线

## 维护规则

1. 每次改动 `include/vn_runtime.h`、`include/vn_preview.h`、`include/vn_backend.h` 或 `include/vn_pack.h`，必须同步更新本目录文档；涉及宿主集成语义时还必须同步更新 `../host-sdk.md`。
2. API 语义变更必须追加“兼容性说明”，不能只改代码不改文档。
3. 文档示例默认使用 C89 语法。
