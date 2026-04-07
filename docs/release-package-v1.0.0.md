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

当前这份文件已经从骨架推进到可用包内容计划，而且本地 `v1.0.0` rehearsal 已经能生成对应 bundle/export/report/publish-map 物料。

当前 rehearsal 输出位于：

1. `build_release_v1_rehearsal/bundle/`
2. `build_release_v1_rehearsal/export/bundle/`
3. `build_release_v1_rehearsal/export/report/`
4. `build_release_v1_rehearsal/export/publish/`
5. `build_release_v1_rehearsal/export/remote/`

## Pre-Release Checks Still Required

正式发布前仍需确认：

1. `docs/release-publish-v1.0.0.json` 与最终 GitHub Release 对象完全一致
2. `demo.vnpak` 仍是最终发布资产，或在 spec 中显式替换
3. 当前 rehearsal 生成物迁移到最终归档位置
4. 最终包内容与 `docs/release-checklist-v1.0.0.md` 不冲突
