# Platform Matrix

本文档收口当前 N64GAL 已支持与计划支持的平台、后端回退链、构建入口与验证要求。

## 目标平台

| Arch | OS | Backend Priority | Current Status | Validation Route | Evidence |
|---|---|---|---|---|---|
| `amd64` | Linux | `avx2 -> scalar` | 已接入并在 CI 验证 | `scripts/ci/run_cc_suite.sh` + GitHub Actions Linux x64 | `suite-linux-x64`, `perf-linux-x64` |
| `amd64` | Windows | `avx2 -> scalar` | 已接入并在 CI 验证 | `scripts/ci/run_windows_suite.ps1 -PlatformLabel windows-x64 -CMakePlatform x64` + GitHub Actions Windows x64 | `suite-windows-x64` |
| `arm64` | Linux | `neon -> scalar` | 已接入并在 CI 验证 | `scripts/ci/run_cc_suite.sh` + GitHub Actions Linux arm64 | `suite-linux-arm64` |
| `arm64` | Windows | `neon -> scalar` | 已接入并在 CI 验证 | `scripts/ci/run_windows_suite.ps1 -PlatformLabel windows-arm64 -CMakePlatform ARM64` + GitHub Actions Windows arm64 | `suite-windows-arm64` |
| `riscv64` | Linux | `rvv -> scalar` | 交叉构建与 QEMU 已接入，原生验证进行中 | `scripts/ci/build_riscv64_cross.sh` + `scripts/ci/run_riscv64_qemu_suite.sh` | `suite-linux-riscv64-qemu-scalar`, `suite-linux-riscv64-qemu-rvv`, `perf-riscv64-qemu-rvv` |

## 平台层原则

1. Frontend/Backend API 只有一份，跨架构迁移只新增后端实现并重编译。
2. 运行时代码保持严格 `C89`，平台差异优先收口到 `src/core/platform.c` 这一类内部层。
3. 后端选择总是先走平台优先 ISA，再自动回退到 `scalar`。
4. 每个性能优化必须保留回退链，不能把平台适配写死在 Frontend 或宿主侧。

## 当前内部平台抽象

当前已收口的能力：

1. 路径绝对/相对判定：同时接受 `/`、`\\` 与 Windows `X:\`/`X:/` 风格。
2. 路径目录提取：`vn_platform_path_dirname(...)`。
3. 路径拼接：`vn_platform_path_join(...)`，生成宿主平台原生分隔符。
4. 二进制文件打开：`vn_platform_fopen_read_binary(...)`，避免 pack 读取走文本模式。
5. 统一计时入口：`vn_platform_now_ms()`，`runtime` 与 `preview` 共享同一套时间来源。
6. 统一休眠入口：`vn_platform_sleep_ms()`，当前已接入 CLI `keyboard` 调试模式的帧间节奏控制。
7. 编译/平台探测入口：`src/core/build_config.h` 统一收口 OS/Arch/Compiler 宏，`preview` 与 backend 复用同一套判断。
8. CLI 键盘语义统一：Linux/Windows 都收口到 `1-9`、`t/T`、`q/Q`。

当前仍待继续收口的能力：

1. 若未来引入宿主文件桥接，需要在不破坏现有 `pack_path` 模式的前提下扩展。

## 代码落点

| Area | Files |
|---|---|
| 平台路径/文件 I/O | `src/core/platform.h`, `src/core/platform.c` |
| 编译/平台探测 | `src/core/build_config.h` |
| pack 读取 | `src/core/pack.c` |
| 预览路径解析 | `src/tools/preview_cli.c` |
| 运行时键盘输入 | `src/core/runtime_cli.c` |
| 路径单测 | `tests/unit/test_platform_paths.c` |
| Windows suite 归档脚本 | `scripts/ci/run_windows_suite.ps1` |

## 构建与验证

### CMake

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### 本地 C89 套件

```bash
./scripts/ci/run_cc_suite.sh
```

该脚本当前覆盖：

1. `C89` 门禁检查。
2. Demo script/pack 生成。
3. 所有 unit tests。
4. `preview protocol` 集成测试。
5. `example_host_embed` 嵌入示例验证。
6. 生成 `build_ci_cc/ci_logs/*`、`build_ci_cc/ci_suite_summary.md` 与 `build_ci_cc/golden_artifacts/`（如有 diff）。

### Windows 本地/CI 套件

```powershell
pwsh -File scripts/ci/run_windows_suite.ps1 -PlatformLabel windows-x64 -CMakePlatform x64 -SuiteRoot build_ci_windows_x64
pwsh -File scripts/ci/run_windows_suite.ps1 -PlatformLabel windows-arm64 -CMakePlatform ARM64 -SuiteRoot build_ci_windows_arm64
```

该脚本会统一完成：

1. `cmake configure/build`。
2. `ctest --output-on-failure`。
3. 复跑 `test_renderer_fallback`、`test_runtime_api`、`test_runtime_golden`。
4. 将 `VN_GOLDEN_ARTIFACT_DIR` 指向 suite artifact 目录。
5. 生成 `ci_logs/*`、`ci_suite_summary.md` 与 `golden_artifacts/`。
6. 即使 `ctest` 或单个复跑二进制失败，也会尽量保留 summary 与已生成日志。

GitHub 侧最新已验证证据：`ci-matrix` push run `22772138491`（head `8e5dcd8`）于 `2026-03-07 00:26 HKT` 完成 `success`，`windows-x64` 与 `windows-arm64` 两个 job 均为 `success`。

### riscv64 交叉验证

```bash
./scripts/ci/build_riscv64_cross.sh
./scripts/ci/run_riscv64_qemu_suite.sh --skip-rvv
./scripts/ci/run_riscv64_qemu_suite.sh --require-rvv
```

当前 qemu 套件会把日志与摘要落到：

1. `build_ci_riscv64/ci_logs/*`
2. `build_ci_riscv64/ci_suite_summary.md`
3. `build_ci_riscv64/golden_artifacts/`（如有 golden diff 产物）

## 宿主集成要求

1. 宿主应优先使用 `VNRunConfig.pack_path`、`scene_name` 与 Session API，不直接依赖平台私有后端实现。
2. 宿主可以传入相对或绝对 `pack_path`；pack 层负责以二进制模式打开资源。
3. 预览工具与 editor/automation 侧应通过统一预览协议与 Session API 驱动运行时，而不是自行复制平台差异逻辑。
4. 对 Windows/Linux 的支持不应分叉 API，只允许分叉内部 backend 或 platform 实现。

## 后续要求

1. 新增平台相关代码时，必须同步更新本文档和 `issue.md` 中对应 issue 的完成状态。
2. 若修改了宿主可见行为，还必须同步更新 `docs/host-sdk.md` 与 `docs/api/runtime.md`。
3. 新增平台能力后，必须至少补一条可复现验证命令和一条对应测试。
4. Windows CI 目前也必须上传 suite artifact；若只保留 workflow 控制台输出，不视为证据链闭环。
