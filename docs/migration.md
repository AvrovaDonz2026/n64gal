# Migration Notes

## 1. 目标

这份文档描述 `v0.1.0-alpha` 阶段已经存在、以及尚未承诺的迁移范围。

当前重点不是提供完整迁移工具，而是先把“哪些东西已经有版本语义、哪些还没有”写清楚，避免宿主和内容侧误以为 `1.0.0` 级别兼容承诺已经存在。

## 2. 当前结论

### 已有版本语义

1. `vnpak`
   - 当前已有 `v1` / `v2`
   - 运行时支持兼容读取
2. `preview protocol`
   - 当前固定为 `v1`
3. 对外发布标签
   - 当前对外准备的是 `v0.1.0-alpha`

### 还没有完整迁移承诺

1. `vnsave`
   - 当前已有最小 `v0 -> v1` 迁移命令与 probe/reject 规则
   - 尚未形成完整多版本迁移链，且尚未进入正式格式承诺
2. 生态模板 / Creator Toolchain
   - 仍在 `M4-engine-ecosystem`
3. `1.0.0` 级别格式冻结
   - 当前不存在
4. `vn_save.h`
   - 当前已提供 probe + 最小 `v0 -> v1` 离线迁移接口
   - 当前还公开了 `VNSAVE_API_STABILITY = "format v1 stable; generic ABI not public"`
   - 仍不等于完整 save/load 承诺
5. `vn_runtime.h`
   - 当前已提供 runtime-specific session snapshot / file save-load 正式 API
   - 但这层只解决“恢复当前 runtime session”，不等于通用宿主持久化 ABI

## 3. `vnpak` 迁移边界

当前资源包格式状态：

1. `v1`
   - 无 per-resource CRC
2. `v2`
   - 追加 per-resource CRC32
   - 当前打包器默认输出 `v2`

对 `v0.1.0-alpha` 的要求是：

1. 运行时必须继续支持读取 `v1` / `v2`
2. 新产物默认写 `v2`
3. 不要求提供独立 `v1 -> v2` 迁移工具

## 4. `vnsave` 状态

当前仓库还没有把对外存档迁移链收口完成：

1. `ISSUE-015` 已完成 probe、结构化拒绝、最小 `v0 -> v1` 迁移命令与 golden 样例
2. 当前尚未形成完整多版本迁移链，也还不是完整 save/load 子系统
3. 首个正式 `vnsave v1` 承诺不早于 `v1.0.0`
4. 宿主不应假设 `v0.1.0-alpha` 已承诺历史存档兼容
5. 当前 `vn_save.h` 只解决“识别/拒绝/最小迁移”，不等于完整 save/load
6. 当前 runtime-specific quick-save / quick-load 继续通过 `vn_runtime.h` 正式 API 暴露，不应被误读成 `vn_save.h` 已冻结为完整 save/load 面
7. 当前最小正式 save/load 范围只承诺 `runtime session save/load`，不等同于通用宿主 save/load ABI

因此，对外 release 应明确写：

1. `vnsave` 迁移不在 `v0.1.0-alpha` 范围内
2. 若宿主已经自定义保存格式，应继续按宿主侧策略管理
3. `v0.x` 产生的内部/实验存档不视为正式兼容输入

## 5. Preview / Runtime 配置兼容

当前可以认为稳定到 alpha 级别的部分：

1. `VNRunConfig` 基本字段
2. `VNRunResult` 的当前已公开字段
3. `preview protocol v1`
4. `--backend=auto|scalar|avx2|avx2_asm|neon|rvv`

但仍不应承诺：

1. 字段永不变化
2. 预览协议永不扩展
3. 生态工具链参数不再变化

## 6. `v0.1.0-alpha` 对外迁移说明建议

对外 release note 应至少明确：

1. 这是首个 alpha，不是稳定 ABI 版本
2. `vnpak` 继续兼容 `v1/v2`
3. `vnsave` 迁移不在本版本范围
4. `preview protocol` 当前版本为 `v1`
5. `avx2_asm` 仍是 force-only 实验入口

## 7. 后续里程碑

在进入 `v1.0.0` 前，至少要把以下边界收口清楚：

1. `docs/vnsave-version-policy.md`
   - `pre-1.0` / `v1.0.0` / `post-1.0` 规则固定
2. `ISSUE-015`
   - 若 `v1.0.0` 对外开放 `vnsave`，则完成 `vnsave v1` 文件头与探测规则
3. `ISSUE-025`
   - release 兼容矩阵

`v0 -> v1` 迁移器与 Creator Toolchain 的 `migrate` 聚合可以继续推进，但当前不再和首个 `v1.0.0` 边界混为一谈。

## 8. 当前建议

对 `v0.1.0-alpha`：

1. 明确写“当前无完整存档迁移承诺”
2. 资源包兼容只声明到 `vnpak`
3. 迁移责任边界写清楚，不要模糊承诺

对 `v1.0.0`：

1. 首次引入正式 `vnsave v1`
2. 至少把版本编号、探测规则与错误行为写清楚
3. 若继续保留 runtime-specific session persistence，必须在 release note 里明确写“这是 runtime-scoped save/load，不等于通用宿主 save ABI”
4. 详细策略见 [`docs/vnsave-version-policy.md`](./vnsave-version-policy.md)
5. 最小 probe API 见 [`docs/api/save.md`](./api/save.md)
