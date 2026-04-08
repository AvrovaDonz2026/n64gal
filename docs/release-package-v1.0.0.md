# Release Package Plan: v1.0.0

## Goal

这份文档定义首个正式版 `v1.0.0` 的目标包内容和当前打包边界，避免正式发布时仍然沿用 `v0.1.0-alpha` 的物料命名和目录假设。

## Target Package Contents

正式版当前建议至少包含：

1. 源码快照
2. `README.md`
3. `LICENSE`
4. `assets/demo/demo.vnpak`
5. `docs/release-v1.0.0.md`
6. `docs/release-evidence-v1.0.0.md`
7. `docs/release-package-v1.0.0.md`
8. `docs/release-publish-v1.0.0.json`
9. `docs/compat-matrix.md`
10. `docs/vnsave-version-policy.md`
11. `docs/migration.md`
12. `release_bundle_manifest(.md/.json)`
13. `release_publish_map(.md/.json)`
14. `release_report(.md/.json)`

## Current Packaging Rule

正式版 package 当前应按 `release-spec` 驱动解释：

1. `release note` 来自 `docs/release-publish-v1.0.0.json`
2. `release evidence` 与 `release package` 文件名应由 spec 对应版本推导
3. 资产路径当前默认是 `assets/demo/demo.vnpak`
4. `bundle / export / report / publish-map / remote-summary` 必须围绕同一份 spec 工作

## Current Status

当前这份文件已经从骨架推进到真实发布包计划，而且最终 `v1.0.0` export / publish / remote 物料已经生成。

当前最终输出位于：

1. `build_release_v1_final_export/bundle/`
2. `build_release_v1_final_export/report/`
3. `build_release_v1_final_export/publish/`
4. `build_release_v1_final_export/remote/`
5. GitHub Release: `https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.0.0`

## Pre-Release Checks Still Required

最终发布后仍需长期保留并核对：

1. `docs/release-publish-v1.0.0.json` 与最终 GitHub Release 对象完全一致
2. `demo.vnpak` 仍是最终发布资产，或在 spec 中显式替换
3. `build_release_v1_final_export/...` 与最终归档位置保持一致
4. 最终包内容与 `docs/release-checklist-v1.0.0.md` 不冲突
