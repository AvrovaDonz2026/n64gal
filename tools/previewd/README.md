# vn_previewd

`vn_previewd` 是面向编辑器、CI 和自动化脚本的无 GUI 预览入口。

当前版本特性：

1. 基于 `vn_runtime_session_*`，不依赖 `vn_player`。
2. 支持直接 CLI 参数与 `key=value` 请求文件。
3. 输出结构化 JSON 响应，便于外部工具消费。
4. `inject_input` 当前支持 `choice:<n>`、`key:<c>`、`trace_toggle`、`quit`。

协议说明见 `../../docs/preview-protocol.md`。
