# N64GAL v1.1.0

## Status

`v1.1.0` 已于 2026-07-17 发布：

`https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.1.0`

## Highlights

1. 自定义场景与图片资源进入既有 `VM -> runtime state -> VNRenderOp[] -> backend` 链路。
2. 新增背景切换、背景交叉淡化和最多 8 个持久立绘层。
3. runtime 按需加载并缓存 `RGBA16/CI8/IA8` 真实纹理，preview 可导出带尺寸和 CRC 的截图。
4. build-info、frame view 与 snapshot 通过版本化结构兼容追加，Runtime API 仍为 `public stable v1`。
5. 发布链由同一份 spec 驱动四份发布文档与多个资产。

## Performance And Efficiency

最新正式测量来自实现候选 `eb843c7` 的 [ci-matrix run 29553907770](https://github.com/AvrovaDonz2026/n64gal/actions/runs/29553907770)。Smoke 使用 `600x800`、`2s` 采样、`1s` warmup、每场景 63 个样本；Linux perf binary 使用 `-O2 -DNDEBUG`，Windows 使用 Release build。

| Platform | Native backend | S1 p95 scalar -> SIMD | S10 p95 scalar -> SIMD |
|---|---|---:|---:|
| Linux x64 | AVX2 | `1.560ms -> 0.146ms` (`10.685x`, `90.64%` lower) | `3.943ms -> 0.397ms` (`9.932x`, `89.93%` lower) |
| Windows x64 | AVX2 | `2.708ms -> 0.256ms` (`10.578x`, `90.55%` lower) | `6.966ms -> 0.541ms` (`12.876x`, `92.23%` lower) |
| Linux arm64 | NEON | `1.685ms -> 0.580ms` (`2.905x`, `65.58%` lower) | `4.546ms -> 1.519ms` (`2.993x`, `66.59%` lower) |
| Windows arm64 | NEON | `1.803ms -> 0.617ms` (`2.922x`, `65.78%` lower) | `4.618ms -> 1.458ms` (`3.167x`, `68.43%` lower) |

这些数字是当前 legacy procedural 场景中 native SIMD 相对 scalar 的同机结果，不是 `v1.0.0 -> v1.1.0` 的版本级提速，也不代表真实纹理路径已经获得同等 SIMD 收益。静态 `S3` 会被既有 frame reuse 压到 `0-0.001ms`，因此不拿它计算有意义的场景提速结论。

真实内容效率证据：

1. clean `eb843c7` 上以 scalar、`600x800`、`dt=16ms` 运行 `Opening` 和 `Gallery`，每场景 28,125 帧、合计 900 秒模拟时间；两场景均 `end=1`、`err=0`，各记录 28,112 次 frame-reuse hit 与 3 次 miss。部分动态帧不具备复用资格，因此 hit/miss 计数不等同于总帧数。
2. framebuffer reuse 命中时跳过 render-op 构建、纹理准备和 raster；真实内容已进入 v1.0 既有的 frame reuse/op-cache 分层回退链。
3. 真实纹理按当前 render-op 工作集懒加载；纹理 payload cache 上限为 32 MiB，当前帧资源 pin 后按 LRU 驱逐未 pin 项。该上限不是进程总内存上限。
4. 背景 crossfade 以配对 op 在单次像素循环中同时采样两张纹理并写回一次合成结果。
5. clean Clang 19 ASan/UBSan 运行同一 900 秒内容 soak，最高单进程 RSS 为 32.48 MiB，低于 64 MiB 门限；ASan 配置禁用了 leak detector，因此这里只声明 ASan/UBSan 未报错，不声明“无内存泄漏”。

## Compatibility

1. `preview protocol v1` 不变，只追加可选截图字段。
2. `vnpak` 继续默认写 `v2` 并读取已声明的 `v1/v2`；场景目录作为 v2 内的新资源类型。
3. `vnsave v1` 外层格式不变；v1.1 runtime payload 可版本化追加，并继续读取 v1.0 payload。
4. 旧 `S0/S1/S2/S3/S10` 与 legacy demo 行为保持不变。
5. 正式平台仍为 Linux x64、Windows x64、Linux arm64、Windows arm64。

## Validation

1. [ci-matrix run 29553907770](https://github.com/AvrovaDonz2026/n64gal/actions/runs/29553907770)：四个正式平台、RISC-V cross/QEMU 和全部 perf gate 通过。
2. [linux-x64-sanitizers run 29553907764](https://github.com/AvrovaDonz2026/n64gal/actions/runs/29553907764)：Clang ASan/UBSan 与 900 秒内容 soak 通过。
3. 真实内容 scalar/AVX2/NEON golden RGB CRC32：`0x995FF007`。
4. Legacy scalar golden：`S0=58C8928B`、`S1=80D7F175`、`S2=587BC5A4`、`S3=0BC0160F`、`S10=C9A161B9`。

## Deferred

1. `RVV/riscv64 native` 发布级支持与性能承诺，原因是仍没有可用原生设备证据。
2. `SSE2`、JIT 与 `avx2_asm` 自动优先级。
3. 真实字体/字符串、音频设备或回调、GUI 编辑器和跨场景跳转。

## Release Assets

| Asset | Purpose | Bytes | SHA256 |
|---|---|---:|---|
| `demo.vnpak` | Legacy benchmark demo | 1853 | `7e6e2488496d738f98b3b6dca494fc93d37799685cebb43e7a3850e955d000fa` |
| `content-demo.vnpak` | Real-content `Opening/Gallery` demo | 1771 | `4205e36162566072ddd88c2f34e856c603e96d984eb58cda67c747744553c3ea` |
