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
python3 tools/toolchain.py validate-manifest tests/fixtures/tool_manifest/valid/vnsave_migrate.json
python3 tools/toolchain.py validate-release-contracts
python3 tools/toolchain.py validate-toolchain-contracts
python3 tools/toolchain.py validate-host-sdk-contracts
python3 tools/toolchain.py validate-preview-contracts
python3 tools/toolchain.py probe-vnsave --in tests/fixtures/vnsave/v1/sample.vnsave
python3 tools/toolchain.py probe-trace-summary tests/fixtures/runtime_trace/sample_trace.log
python3 tools/toolchain.py probe-preview --scene=S2 --frames=2 --command=step_frame:2
python3 tools/toolchain.py probe-perf-summary tests/fixtures/perf_summary/sample_perf_summary.csv
python3 tools/toolchain.py probe-perf-compare tests/fixtures/perf_compare/sample_perf_compare.csv
python3 tools/toolchain.py probe-kernel-bench tests/fixtures/kernel_bench/sample_kernel_bench.csv
python3 tools/toolchain.py probe-kernel-compare tests/fixtures/kernel_compare/sample_kernel_compare.csv
python3 tools/toolchain.py migrate-vnsave --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave
```

当前作用：

1. 给 `validate/migrate/probe` 三类子命令提供统一入口
2. 提供统一帮助文本
3. 保持现有 machine-readable 输出继续透传

### validate

当前已落地：

1. `tools/validate/validate_manifest.py`
2. `tools/validate/validate_release_contracts.py`
3. `tools/validate/validate_toolchain_contracts.py`
4. `tools/validate/validate_host_sdk_contracts.py`
5. `tools/validate/validate_preview_contracts.py`

示例：

```bash
python3 tools/validate/validate_manifest.py tests/fixtures/tool_manifest/valid/vnsave_migrate.json
python3 tools/validate/validate_release_contracts.py
python3 tools/validate/validate_toolchain_contracts.py
python3 tools/validate/validate_host_sdk_contracts.py
python3 tools/validate/validate_preview_contracts.py
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
