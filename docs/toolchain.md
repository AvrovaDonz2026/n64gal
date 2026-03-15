# Creator Toolchain

## 1. 目标

这份文档收口当前 Creator Toolchain 的最小命令面，避免校验、迁移、预览和性能采样继续以零散脚本存在。

## 2. 当前命令面

### unified

当前已落地统一入口：

1. `tools/toolchain.py`

示例：

```bash
python3 tools/toolchain.py --help
python3 tools/toolchain.py validate-all
python3 tools/toolchain.py validate-release-audit --allow-dirty
python3 tools/toolchain.py validate-release-docs
python3 tools/toolchain.py validate-manifest tests/fixtures/tool_manifest/valid/vnsave_migrate.json
python3 tools/toolchain.py validate-release-contracts
python3 tools/toolchain.py validate-toolchain-contracts
python3 tools/toolchain.py validate-backend-contracts
python3 tools/toolchain.py validate-api-index-contracts
python3 tools/toolchain.py validate-compat-matrix
python3 tools/toolchain.py validate-ecosystem-contracts
python3 tools/toolchain.py validate-error-contracts
python3 tools/toolchain.py validate-host-sdk-contracts
python3 tools/toolchain.py validate-migration-contracts
python3 tools/toolchain.py validate-pack-contracts
python3 tools/toolchain.py validate-platform-contracts
python3 tools/toolchain.py validate-preview-contracts
python3 tools/toolchain.py validate-perf-contracts
python3 tools/toolchain.py validate-porting-contracts
python3 tools/toolchain.py validate-runtime-contracts
python3 tools/toolchain.py validate-save-contracts
python3 tools/toolchain.py validate-template-contracts
python3 tools/toolchain.py probe-vnsave --in tests/fixtures/vnsave/v1/sample.vnsave
python3 tools/toolchain.py probe-trace-summary tests/fixtures/runtime_trace/sample_trace.log
python3 tools/toolchain.py probe-preview --scene=S2 --frames=2 --command=step_frame:2
python3 tools/toolchain.py probe-perf-summary tests/fixtures/perf_summary/sample_perf_summary.csv
python3 tools/toolchain.py probe-perf-compare tests/fixtures/perf_compare/sample_perf_compare.csv
python3 tools/toolchain.py probe-kernel-bench tests/fixtures/kernel_bench/sample_kernel_bench.csv
python3 tools/toolchain.py probe-kernel-compare tests/fixtures/kernel_compare/sample_kernel_compare.csv
python3 tools/toolchain.py migrate-vnsave --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave
bash scripts/release/run_release_gate.sh --allow-dirty --skip-cc-suite
python3 tools/toolchain.py release-gate --allow-dirty --skip-cc-suite
python3 tools/toolchain.py release-host-sdk-smoke --summary-out build_release_host_sdk/host_sdk_smoke_summary.md
python3 tools/toolchain.py release-preview-evidence --summary-out build_release_preview/preview_evidence_summary.md
bash scripts/release/run_demo_soak.sh --frames-per-scene 600 --scenes S0,S1,S2,S3,S10
python3 tools/toolchain.py release-soak --frames-per-scene 600 --scenes S0,S1,S2,S3,S10
python3 tools/toolchain.py release-soak --skip-build --runner-bin build_release_soak/vn_player --frames-per-scene 600 --scenes S0,S1,S2,S3,S10
python3 tools/toolchain.py release-gate --allow-dirty --skip-cc-suite --with-soak --soak-frames-per-scene 600 --soak-scenes S0,S1,S2,S3,S10
python3 tools/toolchain.py release-gate --allow-dirty --skip-cc-suite --with-soak --soak-skip-build --soak-skip-pack --soak-runner-bin build_release_soak/vn_player --soak-frames-per-scene 600 --soak-scenes S0,S1,S2,S3,S10
python3 tools/toolchain.py release-bundle --out-dir build_release_bundle
python3 tools/toolchain.py release-report --out-dir build_release_report
python3 tools/toolchain.py release-gate --allow-dirty --skip-cc-suite --with-soak --with-bundle --soak-skip-build --soak-skip-pack --soak-runner-bin build_release_soak/vn_player --bundle-out-dir build_release_bundle
```

当前作用：

1. 给 `validate/migrate/probe` 三类子命令提供统一入口
2. 提供统一帮助文本
3. 保持现有 machine-readable 输出继续透传
4. `validate-all` 可作为当前 `1.0.0` release gate 的单命令入口
5. `scripts/release/run_release_gate.sh` 可生成 release gate 摘要并串行执行本地发布前门禁
6. `tools/toolchain.py release-gate` 是对 release gate 脚本的统一入口包装
7. `scripts/release/run_demo_soak.sh` / `tools/toolchain.py release-soak` 可产出 demo soak 摘要，用于满足正式版 soak 留痕要求
8. `release-gate --with-soak` 可把 contract gate 与 soak 留痕合并成一条正式版前命令，并把 soak 摘要内嵌进 release gate summary
9. `release-soak --runner-bin <path>` 可直接复用 release-like / 预编译 `vn_player`，避免每次都现场编译
10. `release-gate --with-soak --soak-runner-bin <path>` 可在单条正式版前命令里直接复用 release-like 二进制
11. `release-bundle` 可把 gate/soak/ci summary 与关键 release docs 汇总成单一目录和 markdown index
12. `release-host-sdk-smoke` 可给宿主 SDK 示例产出发布级 smoke 摘要
13. `release-report` 可把 bundle/gate/soak/ci summary 汇总成单一发布报告
14. `release-gate --with-bundle` 可把 contract gate、soak 和 bundle 合并成一条正式版前命令
15. `release-preview-evidence` 可给 preview protocol 固定 request/response 路径产出发布级证据摘要
12. `release-gate --with-bundle` 可把 gate / soak / bundle 合并成一条正式版前命令

### validate

当前已落地：

1. `tools/validate/validate_release_audit.py`
2. `tools/validate/validate_release_docs.py`
3. `tools/validate/validate_manifest.py`
4. `tools/validate/validate_release_contracts.py`
5. `tools/validate/validate_toolchain_contracts.py`
6. `tools/validate/validate_backend_contracts.py`
7. `tools/validate/validate_api_index_contracts.py`
8. `tools/validate/validate_compat_matrix.py`
9. `tools/validate/validate_ecosystem_contracts.py`
10. `tools/validate/validate_error_contracts.py`
11. `tools/validate/validate_host_sdk_contracts.py`
12. `tools/validate/validate_migration_contracts.py`
13. `tools/validate/validate_pack_contracts.py`
14. `tools/validate/validate_platform_contracts.py`
15. `tools/validate/validate_preview_contracts.py`
16. `tools/validate/validate_perf_contracts.py`
17. `tools/validate/validate_porting_contracts.py`
18. `tools/validate/validate_runtime_contracts.py`
19. `tools/validate/validate_save_contracts.py`
20. `tools/validate/validate_template_contracts.py`

示例：

```bash
python3 tools/validate/validate_release_audit.py --allow-dirty
python3 tools/validate/validate_release_docs.py
python3 tools/validate/validate_manifest.py tests/fixtures/tool_manifest/valid/vnsave_migrate.json
python3 tools/validate/validate_release_contracts.py
python3 tools/validate/validate_toolchain_contracts.py
python3 tools/validate/validate_backend_contracts.py
python3 tools/validate/validate_api_index_contracts.py
python3 tools/validate/validate_compat_matrix.py
python3 tools/validate/validate_ecosystem_contracts.py
python3 tools/validate/validate_error_contracts.py
python3 tools/validate/validate_host_sdk_contracts.py
python3 tools/validate/validate_migration_contracts.py
python3 tools/validate/validate_pack_contracts.py
python3 tools/validate/validate_platform_contracts.py
python3 tools/validate/validate_preview_contracts.py
python3 tools/validate/validate_perf_contracts.py
python3 tools/validate/validate_porting_contracts.py
python3 tools/validate/validate_runtime_contracts.py
python3 tools/validate/validate_save_contracts.py
python3 tools/validate/validate_template_contracts.py
```

输出约定：

1. 成功：
   - `trace_id=tool.validate.manifest.ok ...`
2. 失败：
   - `trace_id=tool.validate.manifest.format ...`
   - `trace_id=tool.validate.manifest.unsupported ...`
   - `trace_id=tool.validate.manifest.io ...`

### migrate

当前已落地：

1. `tools/migrate/vnsave_migrate`

示例：

```bash
./tools/migrate/vnsave_migrate --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave
```

### probe

当前已落地：

1. `tools/probe/vnsave_probe`
2. `tools/probe/trace_summary.py`
3. `tools/probe/preview_summary.py`
4. `tools/probe/perf_summary.py`
5. `tools/probe/perf_compare_summary.py`
6. `tools/probe/kernel_bench_summary.py`
7. `tools/probe/kernel_compare_summary.py`

示例：

```bash
./tools/probe/vnsave_probe --in tests/fixtures/vnsave/v1/sample.vnsave
```

输出约定：

1. 成功：
   - `trace_id=tool.probe.vnsave.ok ...`
2. 失败：
   - `trace_id=tool.probe.vnsave.failed ...`

当前仍以现有验证链和公开 API 为补充：

1. `vn_previewd`
2. `vn_preview_run_cli`
3. `vn_save.h` 的 probe API
4. `tests/perf/*` 的 artifact 生成链

## 3. 当前统一约定

1. 参数错误和格式错误尽量输出 machine-readable：
   - `trace_id`
   - `error_code`
   - `error_name`
   - `message`
2. 新工具优先放在：
   - `tools/validate/`
   - `tools/migrate/`
   - `tools/probe/`
3. 版本边界必须同步写回：
   - `docs/compat-matrix.md`
   - `docs/api/compat-log.md`
   - `docs/migration.md`

## 4. 当前结论

`Creator Toolchain` 现在还不是完整聚合层，但已经不再是纯概念：

1. 有统一入口
2. 有最小 `validate`
3. 有最小 `migrate`
4. 有最小 `probe`（save + runtime trace + preview + perf summary + perf compare + kernel bench + kernel compare）
5. 有统一 machine-readable 输出约定
