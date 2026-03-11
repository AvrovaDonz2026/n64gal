# Release Roadmap: v1.0.0

## 1. 目标

这份文档把 `v1.0.0` 的范围从“长期愿景”收口成当前项目可执行的发布边界。

核心决策：

1. `v1.0.0` **先不包含 RVV / riscv64 native 承诺**
2. `v1.0.0` 优先完成 `x64/arm64 + Linux/Windows`
3. `riscv64/RVV` 转入 `post-1.0` 轨道

## 2. `v1.0.0` 当前范围

纳入：

1. `x64 Linux`
2. `x64 Windows`
3. `arm64 Linux`
4. `arm64 Windows`
5. `scalar`
6. `avx2`
7. `neon`
8. Runtime / Session API
9. Preview protocol v1
10. 打包链与 demo pack

不纳入：

1. `riscv64 native`
2. `RVV` 发布级 perf 承诺
3. `avx2_asm` 默认优先级
4. `JIT`
5. 模板 / Creator Toolchain / 兼容矩阵的完整生态闭环

## 3. 进入 `v1.0.0` 的硬门槛

至少满足：

1. `x64/arm64 Linux+Windows` 四个平台主矩阵稳定
2. `scalar/avx2/neon` 的 golden / dirty submit / preview protocol / runtime API 全部通过
3. `README` / API 文档 / migration / backend porting / changelog / release note 完整
4. `compat-matrix.md` 能清楚表达平台、后端、格式与接口边界
5. `v0.1.x` 阶段里发现的主线兼容问题收口
6. `vnsave` 版本策略有明确定义，并与 release 文档 / host SDK 一致
7. 发布 checklist 从 alpha 级提升到正式版级别

## 4. `riscv64/RVV` 的定位

当前定义为：

1. `post-1.0`
2. `qemu-first`
3. 原生硬件到位后再提升为发布承诺

这不表示放弃 `RVV`，而是避免它继续阻塞 `1.0.0`。

## 5. 建议版本节奏

1. `v0.1.0-alpha`
   - 已发布
2. `v0.1.0-mvp`
   - 把当前主线文档、证据链和平台范围再收紧
3. `v0.2.x-beta`
   - 收口正式发布前的兼容与版本策略
4. `v1.0.0`
   - 先发布不含 `RVV` 的稳定版本

## 6. 当前建议

从现在开始，所有“按 `1.0.0` 标准推进”的动作都按以下优先级排序：

1. 先收口 `x64/arm64 Linux+Windows`
2. 再收口文档、迁移、版本边界
3. `RVV/riscv64` 保持继续开发，但不再作为 `1.0.0` 阻塞项
