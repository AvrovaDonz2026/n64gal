# VNSave Version Policy

## 1. 目标

这份文档定义 `vnsave` 在 `pre-1.0`、`v1.0.0` 与 `post-1.0` 三个阶段的版本承诺边界。

重点不是提前承诺一个尚未落地的迁移器，而是先把“什么时候开始正式承诺存档格式”写清楚。

## 2. 当前结论

1. `v0.x` 阶段没有对外稳定 `vnsave` 兼容承诺
2. 首个正式存档格式版本将定义为 `vnsave v1`
3. `vnsave v1` 的公开承诺不早于 `v1.0.0`
4. `pre-1.0` 开发快照产生的存档，一律视为非兼容输入

## 3. `pre-1.0` 策略

在 `v0.x` 阶段：

1. 可以继续实验 Session / Runtime / Preview 行为
2. 可以为未来 `vnsave` 预留接口或内部结构
3. 但不得对外声称“alpha/beta 期存档可跨版本兼容”

对宿主与内容侧的明确规则：

1. 不应把 `v0.x` 产生的存档视为可长期保留资产
2. 若宿主已自行实现保存格式，应继续由宿主自行管理
3. release note 必须明确写出“当前无正式存档兼容承诺”
4. 当前代码层也通过 `VNSAVE_API_STABILITY = "pre-1.0 unstable"` 暴露这一口径，避免宿主只能靠 README 猜测

## 4. `v1.0.0` 策略

进入 `v1.0.0` 前，至少需要固定：

1. `vnsave v1` 的版本编号与命名
2. 文件头 / 版本探测规则
3. 对损坏、未知版本、过新版存档的结构化错误行为
4. release 文档中的兼容边界

`v1.0.0` 的最低承诺是：

1. 首次对外公开 `vnsave v1`
2. `v1.0.0` release note 必须明确写出哪些 `vnsave` 版本被支持
3. 对未知、过新、损坏或 `pre-1.0` 存档，必须结构化拒绝，不能静默 best-effort 读取
4. 若 `v1.0.0` 同时公开 runtime-specific session persistence，则必须明确写出“这层只承诺当前 runtime session save/load，不自动等同于通用宿主 save ABI”
5. 首版最小正式 save/load 范围固定为 `runtime-session-only`

## 5. `post-1.0` 策略

一旦 `vnsave v1` 发布，后续版本规则应为：

1. 每个 release 都必须明确写出自己的 save 兼容范围
2. 破坏性变更必须升级版本并附迁移说明或明确拒绝策略
3. 迁移器只在实际交付时才能作为发布承诺写入 release 文档

## 6. 与 `ISSUE-015` 的关系

当前这份文档解决的是“版本策略”问题，不等于 `ISSUE-015` 已完成。

`ISSUE-015` 仍然负责：

1. `vnsave v1` 文件头与探测规则的最终实现
2. `v0 -> v1` 迁移命令
3. golden 样例与结构化错误输出
4. runtime-specific payload 与未来更通用 save ABI 的边界说明

## 7. 当前建议

从现在开始，所有 release 文档都按以下口径书写：

1. `v0.x`：无正式 `vnsave` 兼容承诺
2. `v1.0.0`：首次引入正式 `vnsave v1`
3. 未来版本：只承诺 release note 明确写出的 save 兼容范围
4. `post-1.0`：再谈迁移器与更高阶兼容矩阵
