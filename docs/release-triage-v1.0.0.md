# Release Triage: v1.0.0

## 1. 目的

这份文档把“距离 `v1.0.0` 还差什么”收口成执行优先级，而不是继续把所有事项混在同一层级。

当前判断基于三类事实：

1. 当前代码/测试已经可运行、可验证
2. 当前 release 文档与 release spec 仍停留在 `v0.1.0-alpha`
3. 当前首个 `v1.0.0` 范围已明确只收 `x64/arm64 + Linux/Windows`

## 2. Must Have

这些项不完成，就不应发布首个 `v1.0.0`。

1. 生成真正的 `v1.0.0` release 产物链，而不是继续复用 `v0.1.0-alpha` 产物名义。
   - 当前已存在的 release spec / release note / evidence 仍是 `v0.1.0-alpha`
   - 需要补齐 `v1.0.0` 的 release note、publish map、bundle、report、remote summary 对齐物
2. 四个平台 release-grade 证据收口。
   - 范围只包含 `x64 Linux`、`x64 Windows`、`arm64 Linux`、`arm64 Windows`
   - 需要把 `release-gate`、`release-soak`、`release-host-sdk-smoke`、`release-platform-evidence`、`release-preview-evidence` 的正式版留痕归档成可引用证据
3. Public surface 冻结到“正式版可承诺”级别。
   - Runtime API 不能继续停留在“`public v1-draft (pre-1.0)` 但字段还可继续收口”的口径
   - Preview protocol v1 需要明确哪些字段/错误语义在 `1.0.0` 后视为稳定
   - `vnpak` / `vnsave` 的版本边界必须在 release note、compat matrix、migration、host SDK 里完全一致
4. `vnsave` 的正式版立场固定。
   - 当前代码已经有 `probe + reject + v0 -> v1 migrate`
   - 但 `1.0.0` 必须明确：正式承诺是否只包含 `vnsave v1 probe/migrate`，还是包含完整 save/load 面
   - 这件事不能继续停留在“文档规则与实现之间”
5. Release checklist 从“文档存在”变成“已逐项完成”。
   - `docs/release-checklist-v1.0.0.md` 当前是完整清单，但大部分仍是待执行项
   - 发布前至少要产出一轮完整的、可复现的 checklist 勾选证据
6. 性能门禁达到“可发布解释”级别。
   - `x64` / `arm64` 的 perf smoke、threshold、artifact 需要持续稳定
   - 已知性能实验项必须继续与正式承诺分离，避免 dirty/dynres/JIT/实验后端混入发布口径

## 3. Nice To Have

这些项会明显提升 `1.0.0` 质量，但不应阻塞首发。

1. Dirty regression 的进一步分析与固定结论。
   - 当前 dirty-tile 已有 runtime、preview、artifact、repeat variability
   - 但仍有少量短窗口噪声需要继续解释
2. Dynamic resolution 的默认值/阈值校准。
   - 当前最小 slice 已落地，但默认仍保持 `off`
   - 是否在 `1.0.0` 默认开启，不应靠感觉决定
3. x64/arm64 热点收益继续固化。
   - 包括 textured full-span、row-palette gather/apply、remaining kernel hotspots
4. 模板与 Creator Toolchain 的进一步收口。
   - 当前模板、toolchain、validator 已可用
   - 但完整生态闭环仍不是 `1.0.0` 首发阻塞项
5. 更多 toolchain / compiler 证据。
   - 例如 `Clang/MSVC/ClangCL` 的额外编译与 smoke 证明

## 4. Post-1.0

这些项当前已经明确不属于首个 `v1.0.0` blocker。

1. `RVV/riscv64 native` 发布承诺
2. `avx2_asm` 自动优先级
3. `JIT`
4. 完整 Creator Toolchain / 模板生态闭环
5. 更高阶 `vnsave` 迁移矩阵与多历史版本兼容承诺

## 5. 当前最合理的推进顺序

1. 先跑通一轮 release-grade 证据链。
   - `validate-all`
   - `release-gate`
   - `release-soak`
   - `release-host-sdk-smoke`
   - `release-platform-evidence`
   - `release-preview-evidence`
   - `release-bundle`
   - `release-report`
2. 再固定 `1.0.0` 的格式/API 承诺。
   - runtime
   - preview
   - `vnpak`
   - `vnsave`
3. 最后生成真正的 `v1.0.0` release note / publish map / external evidence chain。

## 6. 一句话结论

当前离 `v1.0.0` 最近的缺口不是“再做一个核心子系统”，而是：

1. freeze the public contract
2. prove the four-platform release path
3. package the evidence as a real `v1.0.0` release
