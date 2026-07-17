# N64GAL v1.1.0

## Status

`v1.1.0` 当前处于开发中的发布候选阶段；tag 和 GitHub Release 尚未创建。

## Planned Highlights

1. 自定义场景与图片资源进入既有 `VM -> runtime state -> VNRenderOp[] -> backend` 链路。
2. 新增背景切换、背景交叉淡化和最多 8 个持久立绘层。
3. runtime 按需加载并缓存 `RGBA16/CI8/IA8` 真实纹理，preview 可导出带尺寸和 CRC 的截图。
4. build-info、frame view 与 snapshot 通过版本化结构兼容追加，Runtime API 仍为 `public stable v1`。
5. 发布链由同一份 spec 驱动四份发布文档与多个资产。

## Compatibility

1. `preview protocol v1` 不变，只追加可选截图字段。
2. `vnpak` 继续默认写 `v2` 并读取已声明的 `v1/v2`；场景目录作为 v2 内的新资源类型。
3. `vnsave v1` 外层格式不变；v1.1 runtime payload 可版本化追加，并继续读取 v1.0 payload。
4. 旧 `S0/S1/S2/S3/S10` 与 legacy demo 行为保持不变。
5. 正式平台仍为 Linux x64、Windows x64、Linux arm64、Windows arm64。

## Deferred

1. `RVV/riscv64 native` 发布级支持与性能承诺，原因是仍没有可用原生设备证据。
2. `SSE2`、JIT 与 `avx2_asm` 自动优先级。
3. 真实字体/字符串、音频设备或回调、GUI 编辑器和跨场景跳转。

## Release Assets

1. Legacy benchmark demo: `assets/demo/demo.vnpak`
2. Real-content demo: `assets/demo/content-demo.vnpak`
