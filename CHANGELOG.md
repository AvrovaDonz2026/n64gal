# Changelog

## Unreleased

暂无未发布变更。

## v1.1.0 - 2026-07-17

### Added

1. 自定义场景目录、背景与最多 8 个持久立绘层进入既有 `VM -> runtime state -> VNRenderOp[] -> backend` 链路。
2. 包内 `RGBA16/CI8/IA8` 真实纹理渲染、按需缓存与 preview 截图证据。
3. 兼容追加 build-info v2、只读 frame view 和 snapshot v2；Runtime API 仍为 `public stable v1`。
4. release spec 显式声明 note/evidence/package/checklist 和多个 `assets[]`。

### Performance And Efficiency

1. 真实内容接入既有 framebuffer reuse 与 op cache；clean release soak 中 `Opening`、`Gallery` 各执行 28,125 帧，并分别记录 28,112 次 frame-reuse hit。
2. 真实纹理按 render-op 工作集懒加载，payload cache 上限为 32 MiB；当前帧资源 pin 后按 LRU 驱逐未 pin 项。
3. 最新四平台 CI 的 legacy smoke 中，AVX2 在 x64 的 `S1/S10` p95 降幅为 89.93%-92.23%，NEON 在 arm64 的 `S1/S10` p95 降幅为 65.58%-68.43%。这些是当前 SIMD 相对 scalar 的同机结果，不是 v1.0 到 v1.1 的版本提速结论，也不代表真实纹理 SIMD 增益。
4. Clang 19 ASan/UBSan 对真实内容执行 900 秒模拟 soak，最高单进程 RSS 为 32.48 MiB，低于 64 MiB 门限。

### Compatibility

1. `preview protocol v1` 不变。
2. `vnpak` 继续默认写 `v2`、读取 `v1/v2`。
3. `vnsave v1` 外层格式不变，并继续读取 v1.0 runtime payload。
4. 正式平台仍为 Linux/Windows 上的 x64/arm64；`RVV/riscv64 native` 继续延期。

### Validation

1. `ci-matrix`、Linux x64 sanitizer、四平台 correctness/perf gate 与 RISC-V cross/QEMU correctness 全绿。
2. `content-demo.vnpak` 的 scalar/AVX2/NEON 真实内容 CRC 固定为 `0x995FF007`。
3. 两个 release asset 均进入 spec、bundle、publish map 与远端校验流程。

## v1.0.0 - 2026-04-08

首个正式版本，冻结经过验证的 Runtime API v1、格式边界和四平台发布承诺。

### Added

1. `VNRuntimeBuildInfo` 与 runtime/preview/pack/save/host 版本协商。
2. `VNRuntimeSessionSnapshot`、会话 snapshot capture/restore 和基于 `vnsave v1` 的文件级 save/load。
3. `vnsave` probe、最小 `v0 -> v1` migrate 和公开版本策略。
4. host SDK smoke、platform/preview evidence、release bundle/report/publish-map/remote-summary 链路。

### Stable Contracts

1. Runtime API：`public stable v1`，后续只允许兼容追加。
2. Preview protocol：`v1`。
3. `vnpak`：读 `v1/v2`，默认写 `v2`。
4. `vnsave`：外层格式 `v1`，公开 save/load 范围为 `runtime-session-only`。
5. 正式平台：Linux x64、Windows x64、Linux arm64、Windows arm64。

### Deferred

1. `RVV/riscv64 native` 发布级支持与性能承诺。
2. `avx2_asm` 自动优先级、SSE2 和 JIT。

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
