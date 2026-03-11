# Release Evidence: v0.1.0-alpha

## 1. 目标

这份文档把 `v0.1.0-alpha` 对外发布所依赖的证据链收口成固定入口，避免 release note、README 和 issue 分别引用零散信息。

## 2. 已发布版本

1. GitHub prerelease:
   - `https://github.com/AvrovaDonz2026/n64gal/releases/tag/v0.1.0-alpha`
2. Git tag:
   - `v0.1.0-alpha`
3. 已上传 asset:
   - `demo.vnpak`

## 3. 当前测试基线

当前 alpha 版本依赖以下测试链：

1. `./scripts/check_c89.sh`
2. `./scripts/ci/run_cc_suite.sh`
3. `test_runtime_golden`
4. `test_renderer_dirty_submit`
5. `test_backend_consistency`
6. `test_preview_protocol`

当前 golden 场景固定为：

1. `S0`
2. `S1`
3. `S2`
4. `S3`
5. `S10`

默认分辨率固定为：

1. `600x800`

## 4. 当前平台证据

当前对外声称的验证范围：

1. `x64 Linux`
2. `x64 Windows`
3. `arm64 Linux`
4. `arm64 Windows`
5. `riscv64 Linux`（当前仅 `qemu-first`）

当前需要引用的主矩阵：

1. GitHub `ci-matrix`
2. 本地/CI `run_cc_suite`
3. 对应 release note 与 `README` 的平台说明

## 5. 当前性能证据入口

仓库内已固定的证据入口：

1. [perf-report.md](./perf-report.md)
2. [perf-dirty-2026-03-07.md](./perf-dirty-2026-03-07.md)
3. [perf-dynres-2026-03-07.md](./perf-dynres-2026-03-07.md)
4. [perf-windows-x64-2026-03-07.md](./perf-windows-x64-2026-03-07.md)
5. [perf-x64-hosts-2026-03-09.md](./perf-x64-hosts-2026-03-09.md)
6. [perf-rvv-2026-03-06.md](./perf-rvv-2026-03-06.md)

当前 perf smoke / compare 重点场景：

1. `S1`
2. `S3`
3. `S10`

## 6. 已知边界

alpha 版本必须同时明确这些边界：

1. 不是 `1.0.0` 级 ABI/格式冻结承诺
2. `v1.0.0` 当前先不包含 `RVV/riscv64 native`
3. `avx2_asm` 为 force-only 实验后端
4. `JIT` 不在当前 release blocker 范围
5. `vnsave` 迁移不在当前版本承诺内

## 7. 当前结论

`v0.1.0-alpha` 已具备：

1. 已发布 tag + GitHub prerelease
2. 可追溯的测试链
3. 可追溯的平台范围
4. 可追溯的 perf 证据入口
5. 可追溯的已知限制说明

下一步不再继续扩 alpha 口径，而是转向：

1. `v0.1.0-mvp` 差距清单
2. `v1.0.0` 正式发布 checklist
