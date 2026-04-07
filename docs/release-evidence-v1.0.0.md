# Release Evidence: v1.0.0

## 1. 目标

这份文档是首个正式版证据索引骨架，用来收口 `v1.0.0` 的正式发布证据链。

## 2. 当前范围

首版正式版当前只覆盖：

1. `x64 Linux`
2. `x64 Windows`
3. `arm64 Linux`
4. `arm64 Windows`

## 3. 当前必须引用的证据类型

正式发布前至少需要有：

1. `release-gate` 摘要
2. `release-soak` 摘要
3. `release-host-sdk-smoke` 摘要
4. `release-platform-evidence` 摘要
5. `release-preview-evidence` 摘要
6. `release-bundle` / `release-report` / `release-publish-map`
7. GitHub `ci-matrix` 成功记录

## 4. 当前格式边界

当前必须一起对齐：

1. `runtime api`
2. `preview protocol`
3. `vnpak`
4. `vnsave`

其中 `vnsave` 当前最小正式 save/load 范围固定为：

1. `runtime-session-only`

## 5. 当前状态

这份文件目前是正式版 evidence 骨架，还不是最终发布证据页。
