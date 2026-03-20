# N64GAL v0.1.0-alpha

## 定位

`v0.1.0-alpha` 是项目的首个对外预发布标签，用来固定当前已经可运行、可验证、可跨平台构建的最小能力集，而不是 `1.0.0` 级别的长期兼容承诺。

这个版本的目标是：

1. 固定当前前后端分离与单一 API 契约。
2. 固定 `scalar/avx2/neon/rvv` 四条主线后端的当前能力边界。
3. 固定 `x64 Linux/Windows`、`arm64 Linux/Windows`、`riscv64 qemu-first` 的验证现状。
4. 为后续 `v0.1.x` / `v0.2.x` 的发布节奏提供基线。

## 当前范围

包含：

1. `vn_runtime_run(config, result)` 与 Session API。
2. `vn_previewd` 与 `preview protocol v1`。
3. `scalar` 基线后端。
4. `avx2` 主线后端。
5. `neon` 主线后端。
6. `rvv` 最小可运行后端与 `qemu-first` 验证链。
7. `vn_player` CLI、demo pack、脚本编译与打包工具。

不包含：

1. `1.0.0` 级别的 ABI/格式冻结承诺。
2. `riscv64` 原生设备上的发布级 perf 承诺。
3. `avx2_asm` 自动选择；它仍是 force-only 实验后端。
4. `JIT`；当前仍是文档化实验方向，不是 release blocker。
5. 模板工程、Creator Toolchain、兼容矩阵、存档迁移器等 `M4-engine-ecosystem` 目标。

## 平台范围

当前验证覆盖：

1. `x64 Linux`：`avx2 -> scalar`
2. `x64 Windows`：`avx2 -> scalar`
3. `arm64 Linux`：`neon -> scalar`
4. `arm64 Windows`：`neon -> scalar`
5. `riscv64 Linux`：`rvv -> scalar`，当前以 `cross-build + qemu` 为主

## 质量门槛

当前 alpha 版本至少要求：

1. `C89` 门禁通过。
2. `run_cc_suite` / Windows suite / CI matrix 全绿。
3. `test_runtime_golden` 的 `S0/S1/S2/S3/S10 @ 600x800` 基线通过。
4. dirty submit、一致性测试与 preview protocol 测试通过。
5. 已有 perf artifact 可用于回归对照。

## 已知限制

1. `README` / `issue` / `docs` 仍在高频同步更新阶段，不等价于接口冻结。
2. `rvv` 缺少原生设备 perf 证据，当前按 `qemu-first` 解释。
3. `avx2_asm` 仍是 force-only 实验入口，不纳入默认优先级。
4. `neon` 正处于热点持续优化期，性能仍会继续变化。
5. 生态层目标尚未完成：模板、迁移器、Creator Toolchain、兼容矩阵都不在本 alpha 交付范围。

## 建议产物

建议 `v0.1.0-alpha` 对外产物至少包含：

1. 源码快照
2. `README`
3. `LICENSE`
4. `assets/demo/demo.vnpak`
5. 关键 API 文档入口
6. 当前 release note
7. `release_bundle_manifest(.md/.json)`
8. `release_publish_map(.md/.json)`

## 下一步

`v0.1.0-alpha` 之后的优先方向：

1. 继续收口 `ISSUE-012` 的发布文档和证据链
2. 继续收口 `NEON` 与 `RVV` 的发布级 perf 说明
3. 明确 `v0.1.0-mvp` 与未来 `v1.0.0` 的范围差异
