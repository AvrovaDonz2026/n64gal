# Release Package Plan: v1.1.0

## Status

`v1.1.0` 已于 2026-07-17 发布。本文件记录最终包结构、资产摘要和审计规则。

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

## Published Assets

| Asset | Source | Bytes | SHA256 |
|---|---|---:|---|
| `demo.vnpak` | `assets/demo/demo.vnpak` | 1853 | `7e6e2488496d738f98b3b6dca494fc93d37799685cebb43e7a3850e955d000fa` |
| `content-demo.vnpak` | `assets/demo/content-demo.vnpak` | 1771 | `4205e36162566072ddd88c2f34e856c603e96d984eb58cda67c747744553c3ea` |

## Packaging Rules

1. 四份发布文档从 spec 的显式字段读取，不再依赖固定版本文件名。
2. `assets[]` 中每个资产都必须存在、进入 bundle manifest、生成 SHA256，并在远端校验时逐一匹配。
3. 旧 spec 的单个 `asset` 和缺省派生文档路径继续兼容，用于重放 v0.1/v1.0 发布证据。
4. 构建发布候选不会自动创建 tag 或 GitHub Release；tag、draft upload、下载校验和公开发布是显式的远端步骤。

## Final Outputs

1. `build_release_gate/release_gate_summary.md` 与 `.json`
2. `build_release_export/bundle/release_bundle_manifest.md` 与 `.json`
3. `build_release_export/report/release_report.md` 与 `.json`
4. `build_release_export/publish/release_publish_map.md` 与 `.json`
5. `build_release_remote/release_remote_summary.md` 与 `.json`
6. GitHub Release：`https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.1.0`

远端对象公开前先作为 draft 上传两个资产，并下载回本地逐字节核对 SHA256；现有 remote-state validator 的名称/大小检查不能替代该步骤。
