# Release Roadmap: v1.1.0

## Goal

`v1.1.0` 在现有项目设计上交付第一个真实内容渲染切片：内容项目中的场景和图片进入既有 `VM -> runtime state -> VNRenderOp[] -> backend` 链路，同时保持 `v1.0.0` 已冻结的兼容边界。

## Required Slice

1. 项目模板可以声明入口场景、场景脚本和图片资源，并稳定构建内容包。
2. 脚本可以切换背景和管理持久立绘层；图片由 Frontend 转换为统一 render ops。
3. runtime 可以渲染包内真实纹理，preview 可以导出可校验截图。
4. 新视觉状态可以通过兼容追加的 snapshot/build-info API 保存与恢复。
5. release spec、bundle、publish map、remote audit 支持显式发布文档和多资产。

## Compatibility Guardrails

1. Runtime API 继续保持 `public stable v1`，只允许兼容追加。
2. `preview protocol v1`、`vnpak v2` 默认写入格式和 `vnsave v1` 外层格式不升级。
3. `S0/S1/S2/S3/S10`、legacy demo、golden 与既有性能门限不得回归。
4. 正式平台仍为 Linux/Windows 上的 x64/arm64 四个平台。
5. `RVV/riscv64 native` 因缺少设备证据继续延期；只维护 cross/QEMU correctness，不作为 `v1.1.0` blocker。

## Exit Criteria

1. 从干净仓库可用统一项目命令构建模板并预览自定义场景。
2. scalar/AVX2/NEON 的真实内容 golden 和旧场景回归全绿。
3. v1.0 save fixture 可由 v1.1 读取，新增视觉状态可通过新 snapshot payload 恢复。
4. 四个正式平台的 correctness、host、preview、backend-hit 与性能门禁通过。
5. 两个发布资产都进入 bundle、publish map 和远端对象校验。
