# Release Gap: v0.1.0-mvp

## 1. 目标

这份文档把 `v0.1.0-alpha` 到 `v0.1.0-mvp` 之间的剩余工作收口成可执行差距表。

`v0.1.0-mvp` 的定位不是正式版，而是：

1. 把当前 alpha 的发布文档、证据链和平台边界补齐
2. 把 `1.0.0` 之前真正还欠的高优先级事项单独列清楚

## 2. 进入 `v0.1.0-mvp` 的建议门槛

至少满足：

1. `README` / `issue` / release 文档保持同一口径
2. `v0.1.0-alpha` 发布证据链完整可追溯
3. `x64/arm64 Linux+Windows` 的当前支持边界明确
4. `vnsave` 版本策略有书面定义，即使迁移器尚未完成
5. `1.0.0` 范围和 `post-1.0` 范围不再混淆

## 3. 当前差距

### 已完成

1. `v0.1.0-alpha` GitHub prerelease 已发布
2. `README` / `CHANGELOG` / `release note` 已入库
3. `backend-porting` / `migration` 文档已入库
4. `v1.0.0` 范围已明确排除 `RVV/riscv64 native`
5. `vnsave` 已补到版本策略级文档，明确 `v0.x` 不承诺兼容、`v1.0.0` 起才引入 `vnsave v1`

### 仍待完成

1. 完成 `v0.1.0-alpha` checklist 最后一轮逐项核对
2. 固定 `v0.1.0-mvp` 的最小交付边界
3. 把 `vnsave` 版本策略继续从“文档规则”推进到“实现与错误契约”
4. 把 `1.0.0` checklist 从路线说明提升成正式发布门槛
5. 持续补 `x64/arm64` 平台证据与兼容性收口

## 4. 不纳入 `v0.1.0-mvp`

1. `RVV/riscv64 native`
2. `avx2_asm` 自动选择
3. `JIT`
4. 模板工程 / Creator Toolchain / 兼容矩阵完整闭环

## 5. 建议执行顺序

1. 先完成 alpha 文档链闭环
2. 再完成 `v0.1.0-mvp` 边界文档
3. 再把 `1.0.0` checklist 固化
4. 最后再决定 `v0.2.x-beta` 节奏
