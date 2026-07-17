# Release Package Plan: v1.1.0

## Status

`v1.1.0` 尚未发布；本文件定义候选包结构和最终审计输入。

## Package Contents

1. `README.md`、`CHANGELOG.md`、源码快照与许可证。
2. `docs/release-v1.1.0.md`
3. `docs/release-evidence-v1.1.0.md`
4. `docs/release-package-v1.1.0.md`
5. `docs/release-checklist-v1.1.0.md`
6. `docs/release-publish-v1.1.0.json`
7. Legacy benchmark demo: `assets/demo/demo.vnpak`
8. Real-content demo: `assets/demo/content-demo.vnpak`
9. release bundle manifest、report、publish map 与可选 remote summary。

## Packaging Rules

1. 四份发布文档从 spec 的显式字段读取，不再依赖固定版本文件名。
2. `assets[]` 中每个资产都必须存在、进入 bundle manifest、生成 SHA256，并在远端校验时逐一匹配。
3. 旧 spec 的单个 `asset` 和缺省派生文档路径继续兼容，用于重放 v0.1/v1.0 发布证据。
4. 构建发布候选不会自动创建 tag 或 GitHub Release。
