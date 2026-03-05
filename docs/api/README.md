# N64GAL API Docs

本目录维护 N64GAL 对外 API 文档。

## 文档索引

1. `runtime.md`
   - 运行时入口 API（`vn_runtime_run` / `vn_runtime_run_cli`）
   - 运行配置与结果结构
   - 调用示例与返回码约定
2. `backend.md`
   - 前后端统一契约 API（`vn_backend.h`）
   - 后端注册、选择、回退规则
   - Render IR 字段约束

## 维护规则

1. 每次改动 `include/vn_runtime.h` 或 `include/vn_backend.h`，必须同步更新本目录文档。
2. API 语义变更必须追加“兼容性说明”，不能只改代码不改文档。
3. 文档示例默认使用 C89 语法。
