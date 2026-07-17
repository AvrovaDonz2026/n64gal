# Release Evidence: v1.1.0

## Status

`v1.1.0` 已于 2026-07-17 发布。本文件记录实现候选、最终 release gate、GitHub Actions 和发布资产的可复核证据。

Release URL：`https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.1.0`

## Verified CI

实现候选：`eb843c7c9fc01881aedc0206806a3a9eb4f54a5e`。

1. [ci-matrix run 29553907770](https://github.com/AvrovaDonz2026/n64gal/actions/runs/29553907770)：`success`
   - 四个正式平台：Linux x64、Windows x64、Linux arm64、Windows arm64
   - RISC-V：cross、QEMU scalar、QEMU RVV correctness
   - GCC、Clang、MSVC、ClangCL、host/preview/backend-hit 和四平台 perf gate
2. [linux-x64-sanitizers run 29553907764](https://github.com/AvrovaDonz2026/n64gal/actions/runs/29553907764)：`success`
3. [Push on main run 29553907856](https://github.com/AvrovaDonz2026/n64gal/actions/runs/29553907856)：`success`

CI perf artifact：`perf-linux-x64`、`perf-windows-x64`、`perf-linux-arm64`、`perf-windows-arm64`。

| Platform | Compare | Mean p95 gain | Mean avg gain | S10 candidate p95 |
|---|---|---:|---:|---:|
| Linux x64 | scalar -> AVX2 | `60.19%` | `60.01%` | `0.397ms` |
| Windows x64 | scalar -> AVX2 | `60.93%` | `61.43%` | `0.541ms` |
| Linux arm64 | scalar -> NEON | `44.06%` | `44.06%` | `1.519ms` |
| Windows arm64 | scalar -> NEON | `44.74%` | `44.83%` | `1.458ms` |

Mean 包含被 frame reuse 压到 `0-0.001ms` 的静态 `S3`。逐场景 `S1/S10` 数据见 release note；这些是当前 legacy 场景的 SIMD/scalar 对照，不是 v1.0 到 v1.1 的版本差值，也不代表 real-content texture SIMD 提速。

## Real-Content Soak

clean `eb843c7` 证据：

1. 命令：`BUILD_DIR=build_release_content_soak python3 tools/toolchain.py release-soak --pack assets/demo/content-demo.vnpak --scenes Opening,Gallery --scene-duration-sec 450 --backend scalar`
2. 摘要：`build_release_content_soak/demo_soak_summary.md` 与 `.json`
3. 配置：scalar、`600x800`、`dt=16ms`、每场景 28,125 帧
4. 总模拟时间：900 秒
5. `Opening`：`end=1`、`err=0`、28,112 frame-reuse hits、3 misses
6. `Gallery`：`end=1`、`err=0`、28,112 frame-reuse hits、3 misses
7. Pack SHA256：`4205e36162566072ddd88c2f34e856c603e96d984eb58cda67c747744553c3ea`

## Sanitizer And Memory

clean `eb843c7` 证据：

1. Clang 19，严格 C89，AddressSanitizer + UndefinedBehaviorSanitizer
2. `Opening/Gallery` 同样各执行 450 秒模拟时间
3. 最高单进程 RSS：33,260 KiB（32.48 MiB）
4. RSS 门限：64 MiB
5. 摘要：`build_ci_sanitizers_release/sanitizer_summary.md` 与 `.json`
6. 所有 unit/soak record 均为 `success`

ASan 设置包含 `detect_leaks=0`，因此此证据不作 leak-free 声明。

## Golden And Compatibility

1. 真实内容 scalar/AVX2/NEON RGB CRC32：`0x995FF007`。
2. Legacy scalar golden：`S0=58C8928B`、`S1=80D7F175`、`S2=587BC5A4`、`S3=0BC0160F`、`S10=C9A161B9`。
3. `tests/fixtures/vnsave/v1/runtime-v1.0.0-s3.vnsave` 可由 v1.1 runtime 读取。
4. Runtime API v1、preview protocol v1、`vnpak v1/v2` 读取与 `vnsave v1` 外层格式保持兼容。

## Release Pipeline

最终 release metadata commit 上执行：

1. `validate-all`
2. clean-worktree `release-gate --with-soak --with-export`
3. release host SDK、platform evidence、preview evidence
4. bundle、report、publish map 与 release audit
5. 发布后 remote-state、remote-summary 和下载资产 SHA256 复核

最终生成物位于 `build_release_gate/`、`build_release_export/bundle/`、`build_release_export/report/`、`build_release_export/publish/` 和 `build_release_remote/`。

## Release Assets

1. `demo.vnpak`：1853 bytes，SHA256 `7e6e2488496d738f98b3b6dca494fc93d37799685cebb43e7a3850e955d000fa`
2. `content-demo.vnpak`：1771 bytes，SHA256 `4205e36162566072ddd88c2f34e856c603e96d984eb58cda67c747744553c3ea`

## Deferred Evidence

`RVV/riscv64 native` 不属于本版本正式证据范围。现有 cross/QEMU correctness 继续维护，但不替代原生设备性能证据。
