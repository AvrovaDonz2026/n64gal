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

## 3. 当前已具备

- [x] `README.md`
- [x] `LICENSE`
- [x] `assets/demo/demo.vnpak`
- [x] `docs/release-v0.1.0-alpha.md`
- [x] `docs/release-evidence-v0.1.0-alpha.md`
- [x] `docs/backend-porting.md`
- [x] `docs/migration.md`

## 4. 当前缺口

当前最明确的文档/产物缺口已经从 `LICENSE` 变成“发布动作本身”：

1. 打 tag
2. 归档对应 CI / perf artifact 链接
3. 确认 release checklist 全部勾完

## 5. 推荐动作

发布前按这个顺序执行：

1. 补 `LICENSE`
2. 再检查 `README` / release note / evidence / checklist 是否与当前代码一致
3. 再打 `v0.1.0-alpha` tag
