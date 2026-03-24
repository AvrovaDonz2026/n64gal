# Release Package Plan: v0.1.0-alpha

## 1. 目标

这份文档把 `v0.1.0-alpha` 的“发布产物”从口头列表收口成实际包内容与当前缺口说明。

## 2. 当前建议包内容

建议对外发布包至少包含：

1. 源码快照
2. `README.md`
3. `assets/demo/demo.vnpak`
4. `docs/release-v0.1.0-alpha.md`
5. `docs/release-evidence-v0.1.0-alpha.md`
6. `docs/backend-porting.md`
7. `docs/migration.md`
8. `build_release_bundle/release_bundle_manifest.md`
9. `build_release_bundle/release_bundle_manifest.json`
10. `build_release_publish/release_publish_map.md`
11. `build_release_publish/release_publish_map.json`
12. `docs/release-publish-v0.1.0-alpha.json`

## 3. 当前已具备

- [x] `README.md`
- [x] `LICENSE`
- [x] `assets/demo/demo.vnpak`
- [x] `docs/release-v0.1.0-alpha.md`
- [x] `docs/release-evidence-v0.1.0-alpha.md`
- [x] `docs/backend-porting.md`
- [x] `docs/migration.md`
- [x] `release_bundle_manifest(.md/.json)` 生成入口
- [x] `release_publish_map(.md/.json)` 生成入口
- [x] `docs/release-publish-v0.1.0-alpha.json`

## 4. 当前缺口

当前最明确的缺口已经不再是 alpha 发布动作本身，而是后续版本收口：

1. 固定 `v0.1.0-mvp` 差距表
2. 固定 `v1.0.0` checklist
3. 补齐正式版前的版本策略与兼容边界

## 5. 推荐动作

alpha 已发布后按这个顺序执行：

1. 检查 `README` / `issue` / release 文档是否仍与当前代码一致
2. 固定 `v0.1.0-mvp` 差距表
3. 固定 `v1.0.0` checklist 与 `post-1.0` 边界
