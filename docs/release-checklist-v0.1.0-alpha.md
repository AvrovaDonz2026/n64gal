# Release Checklist: v0.1.0-alpha

## 1. 目标

这份清单用于把 `v0.1.0-alpha` 的发布动作收口成可以逐项勾选的检查，而不是只在 issue 中保留抽象描述。

## 2. 仓库状态

- [ ] `main` 工作树干净
- [ ] `C89` 门禁通过
- [ ] 最新提交已推到远端
- [ ] `README` / `issue` / release 文档已同步到同一版本口径

## 3. 测试与 CI

- [ ] `./scripts/check_c89.sh`
- [ ] `./scripts/ci/run_cc_suite.sh`
- [ ] GitHub `ci-matrix` 最新 run 全绿
- [ ] `test_runtime_golden` 通过 `S0/S1/S2/S3/S10`
- [ ] `test_renderer_dirty_submit` 通过
- [ ] `test_backend_consistency` 通过
- [ ] `test_preview_protocol` 通过

## 4. 平台范围说明

- [x] `x64 Linux` 状态已在 release note 中明确
- [x] `x64 Windows` 状态已在 release note 中明确
- [x] `arm64 Linux` 状态已在 release note 中明确
- [x] `arm64 Windows` 状态已在 release note 中明确
- [x] `riscv64 Linux` 当前为 `qemu-first` 已在 release note 中明确
- [x] `avx2_asm` 为 force-only 实验入口已明确

## 5. 产物

- [ ] 源码快照
- [ ] `README`
- [x] `LICENSE`
- [x] `assets/demo/demo.vnpak`
- [x] `docs/release-v0.1.0-alpha.md`
- [x] `docs/release-evidence-v0.1.0-alpha.md`
- [x] `docs/backend-porting.md`
- [x] `docs/migration.md`

## 6. 证据链

- [x] 至少一份 x64 perf 证据已在仓库内
- [x] 至少一份 arm64 perf 证据已在仓库内
- [x] `perf-report.md` 入口可指向当前可用 artifact 类型
- [x] 版本范围与已知限制已在 release note 中明确

## 7. 已知限制确认

- [x] `v0.1.0-alpha` 不是 ABI/格式冻结版本
- [x] `vnsave` 迁移不在当前版本范围
- [x] `riscv64 native` 不在当前发布级承诺范围
- [x] `JIT` 不在当前发布范围

## 8. 发布后动作

- [ ] 为 `v0.1.0-alpha` 打 tag
- [ ] 归档对应 CI / perf artifact 链接
- [ ] 在 `issue.md` 中记录已发版本
- [ ] 开始整理 `v0.1.0-mvp` 差距清单
