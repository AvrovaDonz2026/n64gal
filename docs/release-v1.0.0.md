# N64GAL v1.0.0

## 定位

`v1.0.0` 是首个正式版目标骨架，用来收口当前已决定进入正式承诺的运行面、平台面和格式边界。

当前这份文件仍是发布前草案，但它已经固定：

1. 首版只覆盖 `x64/arm64 + Linux/Windows`
2. 首版默认后端只承诺 `scalar/avx2/neon`
3. `RVV/riscv64 native`、`avx2_asm`、`JIT` 不进入首个正式版默认承诺
4. `vnsave` 当前最小正式 save/load 范围固定为 `runtime-session-only`

## 当前范围

包含：

1. `vn_runtime_run(...)` 与 Session API
2. `vn_previewd` 与 `preview protocol v1`
3. `scalar`
4. `avx2`
5. `neon`
6. `vnpak` 读 `v1/v2`、默认写 `v2`
7. `vnsave v1` probe / reject / 最小 `v0 -> v1` migrate
8. runtime-specific session save/load（范围：`runtime-session-only`）

不包含：

1. `riscv64 native`
2. `RVV` 发布级 perf 承诺
3. `avx2_asm` 自动优先级
4. `JIT`
5. 更高阶 `vnsave` 多历史版本兼容矩阵

## 平台范围

当前正式版目标范围：

1. `x64 Linux`
2. `x64 Windows`
3. `arm64 Linux`
4. `arm64 Windows`

## 当前发布前提

发布前应至少对齐：

1. `docs/release-checklist-v1.0.0.md`
2. `docs/release-triage-v1.0.0.md`
3. `docs/compat-matrix.md`
4. `docs/vnsave-version-policy.md`
5. `docs/migration.md`

## 当前状态

这份文件目前是正式版 release note 骨架，还不是最终发布文本。

在真正打 tag 前，至少还要补齐：

1. 最终版本范围描述
2. 最终证据链接
3. 最终已知限制
4. 最终发布资产与远端 release URL
