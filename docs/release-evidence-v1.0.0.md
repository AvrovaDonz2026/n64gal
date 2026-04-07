# Release Evidence: v1.0.0

## Purpose

这份文档收口首个正式版 `v1.0.0` 的证据索引，避免 release note、README、toolchain summary 和 GitHub Actions 各自引用不同来源。

## Release Scope Covered By Evidence

当前正式版证据只覆盖以下范围：

1. `x64 Linux`
2. `x64 Windows`
3. `arm64 Linux`
4. `arm64 Windows`
5. `scalar`
6. `avx2`
7. `neon`

## Required Evidence Types

正式发布前至少应同时具备：

1. `release-gate` 摘要
2. `release-soak` 摘要
3. `release-host-sdk-smoke` 摘要
4. `release-platform-evidence` 摘要
5. `release-preview-evidence` 摘要
6. `release-bundle`
7. `release-report`
8. `release-publish-map`
9. GitHub `ci-matrix` 成功记录

## Current Evidence Inputs

当前正式版链路已经具备并可引用：

1. `docs/release-publish-v1.0.0.json`
2. `docs/release-v1.0.0.md`
3. `docs/release-package-v1.0.0.md`
4. `docs/release-checklist-v1.0.0.md`
5. `docs/release-triage-v1.0.0.md`
6. `assets/demo/demo.vnpak`

## Current Verified CI Evidence

最近一轮正式版相关修复已在 GitHub Actions 上转绿：

1. `af5ec0f`
   - `ci-matrix`: `24069410478`
   - `Push on main`: `24069410154`
2. `30bb973`
   - `ci-matrix`: `24072463940`
   - `Push on main`: `24072463548`

## Format And Interface Boundaries

当前正式版证据必须与以下边界一致：

1. `runtime api`
2. `preview protocol`
3. `vnpak`
4. `vnsave`

其中 `vnsave` 当前最小正式 save/load 范围固定为：

1. `runtime-session-only`

## Current Status

这份文件已经不再只是占位标题，但仍然是发布前证据索引。

在真正打 `v1.0.0` tag 前，还需要把以下内容替换成最终链接：

1. 最终 `release-gate` 摘要路径
2. 最终 `release-soak` 摘要路径
3. 最终 `release-bundle` / `release-report` / `release-publish-map` 路径
4. 最终远端 release URL 与资产信息
