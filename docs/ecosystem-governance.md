# Ecosystem Governance

## 1. 目标

这份文档定义 N64GAL 生态扩展在 `1.0.0` 前后的治理规则。

目标不是尽快开放一切扩展能力，而是避免 ABI、格式和迁移责任失控。

## 2. 当前总原则

1. 先文件级扩展，后插件 ABI
2. 先工具进程扩展，后运行时主进程扩展
3. 格式变更必须绑定迁移策略或拒绝加载理由
4. 每次 release 都必须有兼容矩阵

## 3. 当前允许的扩展面

优先级顺序：

1. 导入器
2. 导出器
3. 校验器
4. 迁移器

当前默认运行位置：

1. CLI 工具
2. 独立工具进程

当前默认不开放：

1. 运行时主进程动态插件
2. 未声明 manifest 的第三方扩展
3. 直接依赖私有 backend ABI 的扩展

## 4. 评审规则

任何新增扩展或扩展面变化，至少回答这些问题：

1. 影响哪个 surface：
   - `runtime api`
   - `backend abi`
   - `script bytecode`
   - `vnpak`
   - `vnsave`
   - `preview protocol`
   - `tool manifest`
2. 是 `additive`、`behavior`、`compat-note` 还是 `breaking`
3. 是否需要迁移器
4. 若没有迁移器，拒绝加载理由是否已写清楚
5. `compat-matrix`、release note、`compat-log` 是否同步

## 5. 格式变更规则

1. `script/vnpak/vnsave` 发生 breaking change 时：
   - 要么提供迁移命令
   - 要么提供明确拒绝策略
2. 不允许“尽力读取但语义未定义”
3. 破坏性变化必须更新：
   - `docs/migration.md`
   - `docs/compat-matrix.md`
   - `docs/api/compat-log.md`

## 6. 平台与发布规则

1. 进入正式发布链的扩展必须声明平台/架构范围
2. `experimental` 扩展不得提升为默认路径
3. 若扩展只在工具链侧有效，不得被文档误写成 runtime 默认能力

## 7. 当前结论

对 `1.0.0` 前的仓库治理，最重要的是：

1. release 文档先于生态开放
2. 兼容矩阵先于第三方接入
3. 工具链扩展先于运行时插件
