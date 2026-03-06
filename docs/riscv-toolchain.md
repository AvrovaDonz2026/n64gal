# RISC-V Toolchain And QEMU Validation

## 1. Goal

把 `riscv64` 支持拆成可渐进落地的四层验证链，而不是把“能编译”和“能发布”混在一起。

当前路线遵循两个原则：

1. 先验证功能正确性，再验证原生性能。
2. 先固化 Frontend/Backend 单一 API，再逐步提升 `rvv` 算子覆盖率。

## 2. 当前本地已验证组合（2026-03-06）

本仓库在本地已验证以下工具链组合可工作：

1. `gcc-riscv64-linux-gnu 14.2.0`
2. `qemu-riscv64 10.0.7`
3. sysroot: `/usr/riscv64-linux-gnu`

本地已确认：

1. `vn_player_riscv64` 可在 `qemu-riscv64 -L /usr/riscv64-linux-gnu` 下运行。
2. `vn_player_riscv64 --scene=S0` 自动选择链会回退到 `scalar`。
3. `vn_player_rvv --backend=rvv` 可在 `qemu-riscv64 -cpu max,v=true` 下运行，并输出 `backend=rvv`。
4. `vn_player_rvv --scene=S0` 自动选择链会跳过 `avx2/neon` 并落到 `rvv`。

## 3. 为什么先走 qemu-user

当前项目是用户态 CLI/库模型，不依赖自定义内核、设备树或系统服务，因此 `qemu-user` 是最短路径：

1. 它可以直接验证 ELF 装载、sysroot、文件 I/O、pack 读取和 Runtime 主循环。
2. 它足够验证 `scalar` 回退链与 `rvv` 指令路径是否真的可执行。
3. 它比 `qemu-system-riscv64` 更轻，更适合先进入 PR/CI。

`qemu-user` 不能替代原生机器的地方：

1. 不能代表真实性能。
2. 不能暴露真实缓存/内存带宽瓶颈。
3. 不能覆盖不同 `VLEN` 的真实硬件差异。
4. 不能替代最终的原生 riscv64 Linux 发布验证。

因此路线必须分层。

## 4. 分阶段验证路线

| 阶段 | 名称 | 目标 | 阻塞级别 |
|---|---|---|---|
| `R0` | cross-build | 保证 `riscv64` 二进制与测试可编译 | 阻塞 |
| `R1` | qemu-scalar | 在 `qemu-user` 下验证 `scalar`/回退链/pack/runtime | 阻塞 |
| `R2` | qemu-rvv | 在 `qemu-user` 下验证 `rvv` 二进制能实际跑到 `rvv` | `M3` 前告警 |
| `R3` | native-riscv64 | 在真实 riscv64 Linux 上跑功能与 perf | 发布前阻塞 |

### 4.1 R0: cross-build

使用脚本：`scripts/ci/build_riscv64_cross.sh`

覆盖内容：

1. `vn_player_riscv64`
2. `vn_player_rvv`
3. `rvv_backend.o`
4. 交叉构建的单测二进制：
   - `test_vnpak_riscv64`
   - `test_runtime_api_riscv64`
   - `test_runtime_session_riscv64`
   - `test_renderer_fallback_riscv64`
   - 其余基础单测

目的：确保接口、头文件、实现和链接关系在 `riscv64` 目标上成立。

### 4.2 R1: qemu-scalar

使用脚本：`scripts/ci/run_riscv64_qemu_suite.sh --skip-rvv`

阻塞检查：

1. `test_vnpak_riscv64`
2. `test_runtime_api_riscv64`
3. `test_runtime_session_riscv64`
4. `test_renderer_fallback_riscv64`
5. `vn_player_riscv64 --backend=scalar --scene=S0`
6. `vn_player_riscv64 --scene=S0`，日志必须落到 `backend=scalar`

R1 的核心价值是：

1. 证明 `riscv64 + scalar` 不是“只会编译”。
2. 证明自动选择链在无 RVV 能力时可稳定回退。
3. 证明 pack/runtime/session 基础链路在 emulated userspace 内可运行。

### 4.3 R2: qemu-rvv

使用脚本：`scripts/ci/run_riscv64_qemu_suite.sh --require-rvv`

默认 CPU 参数：`QEMU_RVV_CPU=max,v=true`

阻塞前先作为告警项，验证：

1. `vn_player_rvv --backend=rvv --scene=S0` 输出 `backend=rvv`
2. `vn_player_rvv --scene=S0` 自动选择链最终输出 `backend=rvv`

R2 只回答“RVV 路径能否运行”，不回答“RVV 是否足够快”。

### 4.4 R3: native-riscv64

必须在真实 riscv64 Linux 环境完成：

1. `scalar` 和 `rvv` 功能跑通
2. `S0-S3` perf 采样
3. `rvv` 与 `scalar` 画面一致性比对
4. 不同 CPU/VLEN 环境下的兼容性检查

R3 之前，任何 `rvv` 性能目标都只能视作预估，不能作为发版证据。

## 5. 推荐 CI 接入顺序

### 5.1 当前建议

1. 保留 `linux-riscv64-cross` 作为主线阻塞项。
2. `.github/workflows/ci-matrix.yml` 已接入 `linux-riscv64-qemu-scalar` 阻塞 job。
3. `.github/workflows/ci-matrix.yml` 已接入 `linux-riscv64-qemu-rvv`，并以 `continue-on-error` 维持 `M3` 前告警语义。
4. `R3` 放到 nightly 或专用 runner，不放在普通 PR 阶段。

### 5.2 建议映射

| Job | 内容 | 级别 |
|---|---|---|
| `linux-riscv64-cross` | `scripts/ci/build_riscv64_cross.sh` | 阻塞 |
| `linux-riscv64-qemu-scalar` | `scripts/ci/run_riscv64_qemu_suite.sh --skip-rvv` | 阻塞 |
| `linux-riscv64-qemu-rvv` | `scripts/ci/run_riscv64_qemu_suite.sh --require-rvv` | `M3` 前告警 |
| `linux-riscv64-native-nightly` | 真机功能 + perf | 夜间/手动 |

## 6. 本地执行命令

### 6.1 只做交叉编译

```bash
./scripts/ci/build_riscv64_cross.sh
```

### 6.2 做阻塞级 qemu smoke

```bash
./scripts/ci/run_riscv64_qemu_suite.sh --skip-rvv
```

### 6.3 做完整 qemu smoke（包含 RVV）

```bash
./scripts/ci/run_riscv64_qemu_suite.sh --require-rvv
```

### 6.4 覆盖 qemu 参数

```bash
QEMU_BIN=qemu-riscv64 \
QEMU_SYSROOT=/usr/riscv64-linux-gnu \
QEMU_RVV_CPU=max,v=true \
./scripts/ci/run_riscv64_qemu_suite.sh --require-rvv
```

## 7. Promotion Rule

`linux-riscv64-qemu-rvv` 只有在满足以下条件后才提升为阻塞项：

1. `rvv fill/blend/tex/combine` 核心热路径已基本齐备。
2. `qemu` 冒烟稳定通过率达到连续多次 100%。
3. 至少有一台真实 riscv64 Linux 机器完成 `scalar/rvv` 对照验证。
4. `rvv` 初始化失败回退到 `scalar` 的行为仍有单独证据链。

## 8. 现阶段结论

在当前仓库状态下，最合理的执行顺序不是“直接上原生 perf”，而是：

1. 先把 `R0 + R1` 固化到主线 CI。
2. 把 `R2` 作为告警项持续跑，驱动 RVV 可执行度提升。
3. 待原生 riscv64 runner 或开发板就绪后，再把 `R3` 接上。

这条路线能最大化复用现有前后端分离架构，并把跨架构新增成本限制在后端与验证脚本层。
