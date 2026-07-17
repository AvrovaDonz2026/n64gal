# Migration Notes

## 1. 目标

这份文档描述 `v1.0.0` 已发布后的迁移范围、`v1.1.0` 的兼容追加策略，以及仍未承诺的边界。

重点是把已有正式版本语义与尚未冻结的通用宿主能力分开，避免把 runtime-specific persistence 误解为任意宿主存档 ABI。

## 2. 当前结论

### 已有版本语义

1. `vnpak`
   - 当前已有 `v1` / `v2`
   - 运行时支持兼容读取
2. `preview protocol`
   - 当前固定为 `v1`
3. 对外发布标签
   - 当前正式发布版本是 `v1.1.0`
   - `v1.0.0` 的 Runtime API v1 与格式边界继续作为兼容基线

### 还没有完整迁移承诺

1. `vnsave`
   - 当前已有最小 `v0 -> v1` 迁移命令与 probe/reject 规则
   - `vnsave v1` 已有正式格式语义，但尚未形成完整多版本迁移链
2. 生态模板 / Creator Toolchain
   - 仍在 `M4-engine-ecosystem`
3. 通用宿主 save ABI 冻结
   - 当前不存在；正式承诺仍限于 runtime session save/load
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

对当前正式版的要求是：

1. 运行时必须继续支持读取 `v1` / `v2`
2. 新产物默认写 `v2`
3. 不要求提供独立 `v1 -> v2` 迁移工具

`v1.1.0` 的场景目录和真实图片继续作为 `vnpak v2` 资源追加；没有场景目录的旧包继续使用 legacy 场景映射，不引入 `vnpak v3`。

## 4. `vnsave` 状态

当前仓库还没有把对外存档迁移链收口完成：

1. `ISSUE-015` 已完成 probe、结构化拒绝、最小 `v0 -> v1` 迁移命令与 golden 样例
2. 当前尚未形成完整多版本迁移链，也还不是完整 save/load 子系统
3. 首个正式 `vnsave v1` 已随 `v1.0.0` 进入公开版本语义
4. 宿主不应把 `v0.x` 阶段实验存档视为正式兼容输入
5. 当前 `vn_save.h` 只解决“识别/拒绝/最小迁移”，不等于完整 save/load
6. 当前 runtime-specific quick-save / quick-load 继续通过 `vn_runtime.h` 正式 API 暴露，不应被误读成 `vn_save.h` 已冻结为完整 save/load 面
7. 当前最小正式 save/load 范围只承诺 `runtime session save/load`，不等同于通用宿主 save/load ABI

因此，对外 release 应明确写：

1. `v0.x` 实验存档迁移不在正式兼容承诺内
2. 若宿主已经自定义保存格式，应继续按宿主侧策略管理
3. `v0.x` 产生的内部/实验存档不视为正式兼容输入

## 5. Preview / Runtime 配置兼容

当前正式稳定且允许兼容追加的部分：

1. `VNRunConfig` 基本字段
2. `VNRunResult` 的当前已公开字段
3. `preview protocol v1`
4. `--backend=auto|scalar|avx2|avx2_asm|neon|rvv`

但仍不应承诺：

1. 结构尾部永不兼容追加字段
2. 预览协议永不追加可选字段
3. 生态工具链参数不再变化

## 6. `v1.0.0` 对外迁移说明

正式版当前应至少明确：

1. `vnpak` 继续兼容 `v1/v2`
2. `vnsave v1` 已进入正式版本语义
3. 当前最小正式 save/load 范围固定为 `runtime-session-only`
4. `preview protocol` 当前版本为 `v1`
5. `avx2_asm` 仍是 force-only 实验入口

## 7. `v1.1.0` 兼容追加

`v1.1.0` 不升级公开主版本：

1. Runtime API 继续为 v1；build-info v2、frame view 与 snapshot v2 使用新结构/入口追加。
2. `preview protocol v1` 只追加可选截图字段，旧客户端可忽略。
3. `vnpak` 默认写入版本仍为 v2；旧 v1/v2 包继续读取。
4. `vnsave v1` 外层格式不变；runtime payload 可写 v2，并必须读取 v1.0 payload v1。
5. 旧 snapshot 无法表达内容场景的背景过渡或立绘层时，应明确返回 `VN_E_UNSUPPORTED`，不得静默丢状态。

## 8. 后续里程碑

在进入 `post-1.0` 前，至少要把以下边界继续收口：

1. `docs/vnsave-version-policy.md`
   - `pre-1.0` / `v1.0.0` / `post-1.0` 规则固定
2. `ISSUE-015`
   - 若 `v1.0.0` 对外开放 `vnsave`，则完成 `vnsave v1` 文件头与探测规则
3. `ISSUE-025`
   - release 兼容矩阵

`v0 -> v1` 迁移器与 Creator Toolchain 的 `migrate` 聚合可以继续推进，但当前不再和首个 `v1.0.0` 边界混为一谈。

## 9. 当前建议

发布后当前建议是：

1. 把 `v0.x` 实验存档继续视为非兼容输入
2. `vnpak` 兼容边界继续只声明到 `v1/v2`
3. `vnsave v1` 的探测、拒绝与最小 `v0 -> v1` 迁移规则继续维持稳定
4. `runtime-session-only` 继续明确写成最小正式 save/load 范围
5. 更高阶迁移矩阵与通用宿主 save/load ABI 继续留到 `post-1.0`
6. 最小 probe API 继续以 [`docs/api/save.md`](./api/save.md) 为准
