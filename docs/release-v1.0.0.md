# N64GAL v1.0.0

## Positioning

`v1.0.0` 是 N64GAL 的首个正式版目标，用来把当前已经实装、验证并进入版本语义的运行面、平台面和格式边界收口成稳定对外承诺。

这个正式版的重点不是继续扩张范围，而是冻结已经证明可运行、可验证、可引用证据的最小主线：

1. `x64/arm64 + Linux/Windows`
2. `scalar/avx2/neon`
3. Runtime / Session API
4. `preview protocol v1`
5. `vnpak` 读 `v1/v2`、默认写 `v2`
6. `vnsave v1` 的 probe / reject / 最小 `v0 -> v1` migrate
7. runtime-specific session save/load，范围固定为 `runtime-session-only`

## Included In The First Formal Release

首个 `v1.0.0` 正式版当前只包含：

1. `vn_runtime_run(...)`
2. Session API：`create/step/is_done/set_choice/inject_input/destroy`
3. `vn_runtime_query_build_info(...)`
4. runtime snapshot / file save-load draft API
5. `vn_previewd` 与 `preview protocol v1`
6. `scalar`
7. `avx2`
8. `neon`
9. `vnpak` 读 `v1/v2`、默认写 `v2`
10. `vnsave v1` probe / reject / 最小 `v0 -> v1` migrate
11. runtime-specific session save/load（`runtime-session-only`）

## Not Included

以下内容明确不进入首个 `v1.0.0` 默认承诺：

1. `riscv64 native`
2. `RVV` 发布级 perf 承诺
3. `avx2_asm` 自动优先级
4. `JIT`
5. 更高阶 `vnsave` 多历史版本兼容矩阵
6. 模板 / Creator Toolchain / 兼容矩阵之外的完整生态闭环

## Platform Scope

首个正式版当前只承诺以下四个平台：

1. `x64 Linux`
2. `x64 Windows`
3. `arm64 Linux`
4. `arm64 Windows`

`Linux riscv64` 继续开发，但按 `post-1.0` 处理，不阻塞首个正式版。

## Format And Interface Boundaries

正式版当前应按以下边界对外解释：

1. `runtime api`
   - 当前从 `public v1-draft (pre-1.0)` 收口到首版正式公开面
2. `preview protocol`
   - 固定 `v1` 基面
3. `vnpak`
   - 兼容读取 `v1/v2`
   - 默认输出 `v2`
4. `vnsave`
   - 当前最小正式 save/load 范围固定为 `runtime-session-only`
   - 不等同于通用宿主 save ABI

## Evidence

当前正式版证据链应至少能引用：

1. `release-gate`
2. `release-soak`
3. `release-host-sdk-smoke`
4. `release-platform-evidence`
5. `release-preview-evidence`
6. `release-bundle`
7. `release-report`
8. `release-publish-map`
9. GitHub `ci-matrix` 成功记录

最近一轮已确认通过的正式版相关 push run：

1. `af5ec0f`
   - `ci-matrix`: `24069410478`
   - `Push on main`: `24069410154`
2. `30bb973`
   - `ci-matrix`: `24072463940`
   - `Push on main`: `24072463548`

## Known Limits

当前正式版前仍应明确告知：

1. `RVV/riscv64 native` 不在首版承诺范围
2. `avx2_asm` 仍是 force-only 实验后端
3. `JIT` 不在当前发布范围
4. `vnsave` 的最小正式 save/load 范围仍只覆盖 `runtime-session-only`

## Release Inputs

正式版发布前应至少与以下文档一致：

1. `docs/release-checklist-v1.0.0.md`
2. `docs/release-triage-v1.0.0.md`
3. `docs/compat-matrix.md`
4. `docs/vnsave-version-policy.md`
5. `docs/migration.md`
6. `docs/release-publish-v1.0.0.json`
