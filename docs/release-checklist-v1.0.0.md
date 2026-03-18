# Release Checklist: v1.0.0

## 1. 目标

这份清单定义首个正式版 `v1.0.0` 的硬门槛。

当前范围决策：

1. `v1.0.0` 先只覆盖 `x64/arm64 + Linux/Windows`
2. `RVV/riscv64 native` 转入 `post-1.0`
3. `avx2_asm` / `JIT` 都不进入默认发布承诺

## 2. 平台与后端

- [ ] `x64 Linux` 主链稳定
- [ ] `x64 Windows` 主链稳定
- [ ] `arm64 Linux` 主链稳定
- [ ] `arm64 Windows` 主链稳定
- [ ] `scalar` 作为基线后端稳定
- [ ] `avx2` 作为 x64 主后端稳定
- [ ] `neon` 作为 arm64 主后端稳定
- [ ] `rvv/riscv64 native` 已明确写入 `post-1.0`，不再混入正式版承诺

## 3. 测试与 CI

- [ ] `bash scripts/release/run_release_gate.sh`
- [ ] `python3 tools/toolchain.py validate-release-audit --require-clean`
- [ ] `python3 tools/toolchain.py validate-all`
- [ ] `python3 tools/toolchain.py release-gate`
- [ ] `python3 tools/toolchain.py release-host-sdk-smoke`
- [ ] `python3 tools/toolchain.py release-platform-evidence --out-dir <dir>`
- [ ] `python3 tools/toolchain.py release-preview-evidence`
- [ ] `python3 tools/toolchain.py release-soak`
- [ ] `python3 tools/toolchain.py release-gate --with-soak`
- [ ] `python3 tools/toolchain.py release-gate --with-soak --with-bundle`
- [ ] `python3 tools/toolchain.py release-bundle --out-dir <dir>`
- [ ] `python3 tools/toolchain.py release-report --out-dir <dir>`
- [ ] `python3 tools/toolchain.py release-publish-map --out-dir <dir>`
- [ ] `./scripts/check_c89.sh`
- [ ] `./scripts/ci/run_cc_suite.sh`
- [ ] GitHub `ci-matrix` 主矩阵长期稳定
- [ ] `test_runtime_golden` 稳定覆盖 `S0/S1/S2/S3/S10`
- [ ] `test_renderer_dirty_submit` 稳定
- [ ] `test_backend_consistency` 稳定
- [ ] `test_preview_protocol` 稳定
- [ ] Demo soak 至少一轮完整留痕
- [ ] `release-gate` / `release-soak` / `release-host-sdk-smoke` / `release-platform-evidence` / `release-preview-evidence` / `release-bundle` / `release-report` 的 markdown + json 摘要已归档
- [ ] `release-bundle` 已包含 `gate/soak/ci` 与 `host-sdk/platform/preview` 摘要
- [ ] `release-report` 已显式引用 `host-sdk/platform/preview` 摘要

## 4. 文档与版本边界

- [ ] `README` 与 `issue` 口径一致
- [ ] `CHANGELOG` 与 release note 一致
- [ ] `compat-matrix.md` 与 `README` / release 文档口径一致
- [ ] `migration.md` 明确 `vnpak` / `vnsave` 版本边界
- [ ] `vnsave-version-policy.md` 与 `migration.md` / host SDK 口径一致
- [ ] release note 明确写出 `vnsave` 的支持/迁移/拒绝策略
- [ ] `backend-porting.md` 与当前后端契约一致
- [ ] `1.0.0` / `post-1.0` 范围边界固定
- [ ] 发布 checklist 与 release package 文档完整

## 5. 兼容与格式

- [ ] `vnpak` 兼容范围固定
- [ ] `vnsave` 版本策略固定
- [ ] 未知/过新/损坏/`pre-1.0` 存档的拒绝行为已文档化
- [ ] Preview protocol 的稳定承诺写清楚
- [ ] CLI / Runtime 关键选项的兼容边界写清楚

## 6. 性能与证据

- [ ] `x64` perf smoke / threshold 持续稳定
- [ ] `arm64` perf smoke / threshold 持续稳定
- [ ] 现有 perf artifact 可追溯
- [ ] 已知性能实验项与正式承诺分离

## 7. 明确不纳入

- [x] `RVV/riscv64 native`
- [x] `avx2_asm` 默认优先级
- [x] `JIT`
- [x] 完整 Creator Toolchain / 模板生态闭环

## 8. 发布前最后确认

- [ ] 工作树干净
- [ ] tag / release note / asset 一致
- [ ] release 证据链可直接被外部引用
- [ ] `release_gate_summary(.md/.json)`、`demo_soak_summary(.md/.json)`、`host_sdk_smoke_summary(.md/.json)`、`platform_evidence_summary(.md/.json)`、`preview_evidence_summary(.md/.json)`、`release_bundle_index(.md/.json)`、`release_bundle_manifest(.md/.json)`、`release_report(.md/.json)`、`release_publish_map(.md/.json)` 可直接随 release 引用
- [ ] 版本范围、已知限制、迁移边界全部明确
