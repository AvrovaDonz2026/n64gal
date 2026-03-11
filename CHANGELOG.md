# Changelog

## v0.1.0-alpha

首个对外预发布版本，目标是固定当前已经可运行、可验证、可跨平台构建的最小能力集，而不是 `1.0.0` 级别的长期兼容承诺。

### Added

1. Frontend 输出统一 `VNRenderOp[]`
2. `vn_runtime_run(config, result)` 结构化运行入口
3. Session API：
   - `create`
   - `step`
   - `is_done`
   - `set_choice`
   - `inject_input`
   - `destroy`
4. `vn_previewd` 与 `preview protocol v1`
5. `scalar` 基线后端
6. `avx2` 主线后端
7. `neon` 主线后端
8. `rvv` 最小可运行后端与 `qemu-first` 验证链
9. `vn_player` CLI、demo pack、脚本编译器与打包工具

### Changed

1. 运行时已接入：
   - `frame reuse`
   - `op cache`
   - `dirty tile`
   - `dynamic resolution` 最小 runtime slice
2. x64/arm64 + Linux/Windows CI 矩阵已全绿
3. x64 perf smoke / kernel compare / dirty compare 已固化成 artifact 流程
4. `neon` 当前已从“最小可运行”推进到持续压热点阶段

### Experimental

1. `avx2_asm`
   - force-only
   - 不参与 auto 优先级
2. `JIT`
   - 当前仅保留文档化实验方向
   - 不在本版本范围内

### Known Limits

1. 不是 ABI/格式冻结版本
2. `riscv64` 原生设备 perf 证据仍缺
3. `vnsave` 迁移不在当前版本范围
4. `neon` 仍处于热点持续优化期
5. 模板、Creator Toolchain、兼容矩阵等 `M4-engine-ecosystem` 目标未纳入本版本承诺
