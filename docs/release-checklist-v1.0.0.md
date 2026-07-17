# Release Checklist: v1.0.0

> Archived: `v1.0.0` 已于 2026-04-08 发布；以下状态按最终 evidence、bundle 和远端对象回填。

## 1. 目标

这份清单定义首个正式版 `v1.0.0` 的硬门槛。

当前范围决策：

1. `v1.0.0` 先只覆盖 `x64/arm64 + Linux/Windows`
2. `RVV/riscv64 native` 转入 `post-1.0`
3. `avx2_asm` / `JIT` 都不进入默认发布承诺

## 2. 平台与后端

- [x] `x64 Linux` 主链稳定
- [x] `x64 Windows` 主链稳定
- [x] `arm64 Linux` 主链稳定
- [x] `arm64 Windows` 主链稳定
- [x] `scalar` 作为基线后端稳定
- [x] `avx2` 作为 x64 主后端稳定
- [x] `neon` 作为 arm64 主后端稳定
- [x] `rvv/riscv64 native` 已明确写入 `post-1.0`，不再混入正式版承诺

## 3. 测试与 CI

- [x] `bash scripts/release/run_release_gate.sh`
- [x] `python3 tools/toolchain.py validate-release-audit --require-clean`
- [x] `python3 tools/toolchain.py validate-all`
- [x] `python3 tools/toolchain.py release-gate`
- [x] `python3 tools/toolchain.py release-host-sdk-smoke`
- [x] `python3 tools/toolchain.py release-platform-evidence --out-dir <dir>`
- [x] `python3 tools/toolchain.py release-preview-evidence`
- [x] `python3 tools/toolchain.py release-soak`
- [x] `python3 tools/toolchain.py release-preflight --out-dir <dir>`
- [x] `python3 tools/toolchain.py release-gate --with-soak`
- [x] `python3 tools/toolchain.py release-gate --with-soak --with-bundle`
- [x] `python3 tools/toolchain.py release-gate --with-soak --with-export`
- [x] `python3 tools/toolchain.py release-bundle --out-dir <dir>`
- [x] `python3 tools/toolchain.py release-report --out-dir <dir>`
- [x] `python3 tools/toolchain.py release-publish-map --out-dir <dir>`
- [x] `python3 tools/toolchain.py release-export --out-dir <dir>`
- [x] `python3 tools/toolchain.py validate-release-remote-state --release-json <path>`
- [x] `python3 tools/toolchain.py release-remote-summary --release-json <path> --out-dir <dir>`
- [x] `./scripts/check_c89.sh`
- [x] `./scripts/ci/run_cc_suite.sh`
- [x] GitHub `ci-matrix` 主矩阵长期稳定
- [x] `test_runtime_golden` 稳定覆盖 `S0/S1/S2/S3/S10`
- [x] `test_renderer_dirty_submit` 稳定
- [x] `test_backend_consistency` 稳定
- [x] `test_preview_protocol` 稳定
- [x] Demo soak 至少一轮完整留痕
- [x] `release-gate` / `release-soak` / `release-host-sdk-smoke` / `release-platform-evidence` / `release-preview-evidence` / `release-bundle` / `release-report` 的 markdown + json 摘要已归档
- [x] `release-bundle` 已包含 `gate/soak/ci` 与 `host-sdk/platform/preview` 摘要
- [x] `release-report` 已显式引用 `host-sdk/platform/preview` 摘要

## 4. 文档与版本边界

- [x] `README` 与 `issue` 口径一致
- [x] `CHANGELOG` 与 release note 一致
- [x] `compat-matrix.md` 与 `README` / release 文档口径一致
- [x] `migration.md` 明确 `vnpak` / `vnsave` 版本边界
- [x] `vnsave-version-policy.md` 与 `migration.md` / host SDK 口径一致
- [x] release note 明确写出 `vnsave` 的支持/迁移/拒绝策略
- [x] `backend-porting.md` 与当前后端契约一致
- [x] `1.0.0` / `post-1.0` 范围边界固定
- [x] 发布 checklist 与 release package 文档完整

## 5. 兼容与格式

- [x] `vnpak` 兼容范围固定
- [x] `vnsave` 版本策略固定
- [x] 未知/过新/损坏/`pre-1.0` 存档的拒绝行为已文档化
- [x] Preview protocol 的稳定承诺写清楚
- [x] CLI / Runtime 关键选项的兼容边界写清楚

## 6. 性能与证据

- [x] `x64` perf smoke / threshold 持续稳定
- [x] `arm64` perf smoke / threshold 持续稳定
- [x] 现有 perf artifact 可追溯
- [x] 已知性能实验项与正式承诺分离

## 7. 明确不纳入

- [x] `RVV/riscv64 native`
- [x] `avx2_asm` 默认优先级
- [x] `JIT`
- [x] 完整 Creator Toolchain / 模板生态闭环

## 8. 发布前最后确认

- [x] 工作树干净
- [x] tag / release note / asset 一致
- [x] release 证据链可直接被外部引用
- [x] `release_gate_summary(.md/.json)`、`demo_soak_summary(.md/.json)`、`host_sdk_smoke_summary(.md/.json)`、`platform_evidence_summary(.md/.json)`、`preview_evidence_summary(.md/.json)`、`release_bundle_index(.md/.json)`、`release_bundle_manifest(.md/.json)`、`release_report(.md/.json)`、`release_publish_map(.md/.json)`、`release_export_summary(.md/.json)`、`release_remote_summary(.md/.json)` 可直接随 release 引用
- [x] 版本范围、已知限制、迁移边界全部明确
