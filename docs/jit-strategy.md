# JIT Strategy

## 1. Decision Summary

1. `JIT` 不作为当前主线阻塞项。
2. 当前主线仍优先继续推进 `AVX2/NEON/RVV` 后端热点、Dirty-Tile、动态分辨率与 perf gate 收口。
3. 若后续要试 `JIT`，只接受“`x64-only + VM-only + 默认关闭`”的实验方案，不接受直接把 renderer/multi-arch 机器码生成器拉进主线。

## 2. Why Not Mainline Now

1. 当前整机热点主要仍在 `raster_ms`，而不是 `vm_ms`。
2. 项目当前必须同时维护 `x64 Linux/Windows`、`arm64 Linux/Windows`、`riscv64 Linux`，完整 `JIT` 会显著抬高跨平台复杂度。
3. 真正的 `JIT` 需要同时处理：
   - 可执行内存分配
   - `W^X` / `mprotect` / `VirtualProtect`
   - ARM64 icache flush
   - 不同 ABI / 调用约定
   - 崩溃定位与 CI 可重复性
4. 这些成本和当前“前后端分离 + C89 + 跨架构 + 可回退”主方向并不一致。

## 3. Preferred Near-Term Alternatives

在真正 `JIT` 之前，优先尝试以下三类收益/风险比更高的方案：

1. Runtime specialization
   - 按 `op/alpha/flags/width bucket` 选更细粒度的专用 row kernel。
   - 这类方案保持 AOT/C89，不需要生成机器码。
2. Pack-time AOT / superinstructions
   - 在 pack/script 编译阶段把常见脚本组合压成更粗粒度 VM op。
   - 目标是减少 VM dispatch，不引入运行时代码生成。
3. Deeper trace/cache
   - 在已有 `frame reuse + op cache + dirty tile` 之上继续做更 aggressive 的稳定序列缓存和 patch。

## 4. Allowed JIT Experiment Scope

如果要做实验，范围限定如下：

1. 只做 `VM JIT`
   - 不做 renderer JIT
   - 不做 `VNRenderOp[] -> machine code`
2. 平台只限 `x64 Linux` 与 `x64 Windows`
   - `arm64` / `riscv64` 统一继续走现有解释器路径
3. 默认关闭
   - 必须通过实验开关显式开启
4. 不得影响当前默认 CI 通过条件
   - 正常 CI 仍以解释器路径为准
   - `JIT` 相关验证先独立成 report-only 或专用 workflow

## 5. Proposed Experimental Contract

建议的最小实验契约：

1. 编译开关
   - `VN_EXPERIMENTAL_VM_JIT`
2. 运行时开关
   - `--experimental-vm-jit=on|off`
3. 结果观测
   - `VNRunResult` / trace 增加：
     - `vm_jit_enabled`
     - `vm_jit_compiled_blocks`
     - `vm_jit_hits`
     - `vm_jit_fallbacks`
4. 回退要求
   - 任一 block 编译失败、权限失败、平台不支持时，必须无条件回退解释器，不能改变脚本语义。

## 6. Minimum Viable Experiment

建议按 4 个阶段推进，而不是一步到位：

### Stage 0: Instrument First

1. 先补 `vm_ms / build_ms / raster_ms` 的稳定归因样本。
2. 用现有 `S1/S3/S10` 与更重的 stress case 确认：`vm_ms` 是否已经大到值得做 `JIT`。
3. 如果 `vm_ms` 长期只是次要项，则不进入 `JIT`。

### Stage 1: x64 Linux Proof-of-Concept

1. 只做 `x64 Linux`。
2. 只为最小一组纯算术/分支 block 生成代码。
3. 不做调用宿主 API，不做 I/O，不做复杂内存访问。
4. 目标只是证明：
   - 语义可保持
   - `vm_ms` 有可测下降

### Stage 2: x64 Windows Port

1. 解决 `VirtualAlloc/VirtualProtect` 与权限切换。
2. 补 Windows x64 上的独立 smoke / correctness。
3. 仍然默认关闭，不接主线 gate。

### Stage 3: Decide Go/No-Go

满足以下条件才允许继续：

1. `S1/S3/S10` 里整机 `p95 frame_ms` 至少有 `>= 5%` 的稳定改善。
2. 不能只改善 `vm_ms`，但 `frame_ms` 基本不动。
3. 不能引入新一类平台不稳定点。
4. 不能显著抬高 CI / debug / crash triage 成本。

否则停止实验，回到 AOT specialization 路线。

## 7. Explicit Non-Goals

当前明确不做：

1. `renderer JIT`
2. `multi-arch runtime codegen`
3. `ARM64/RVV` JIT bring-up
4. 把 `JIT` 作为 release blocker
5. 为了 `JIT` 破坏当前 C89 / fallback / CI 结构

## 8. Risks

1. 平台 API 复杂度明显上升。
2. 崩溃定位会比当前 AOT C 路径难得多。
3. 一旦收益主要落在 `vm_ms` 而非 `frame_ms`，整机收益可能不足以证明复杂度。
4. 如果过早扩展到 `arm64/riscv64`，会直接冲击当前跨架构主线节奏。

## 9. Recommendation

1. 当前先不把 `JIT` 排入主线最近 2-3 个迭代。
2. 先完成：
   - `AVX2/NEON` textured full-span 后续优化
   - x64 / arm64 perf gate 收口
   - `windows-x64 dirty S3` 长窗口证据
3. 等现有 raster/perf 热点收敛后，再根据 `vm_ms` 占比决定是否开启 `x64-only VM JIT` 实验。

## 10. Start Criteria

只有同时满足下面条件，才建议开 `JIT` 实验分支：

1. 当前主线 `ci-matrix` 连续稳定。
2. `AVX2` / `NEON` 剩余热点已基本不再是大回退源。
3. 有一批原生 perf 样本能证明 `vm_ms` 的占比已经值得优化。
4. 团队接受它是“实验能力”，不是短期交付承诺。
