# Extension Manifest

## 1. 目标

这份文档定义 N64GAL 生态扩展在进入更开放阶段前，必须遵守的最小 `manifest` 字段。

当前重点不是立即开放运行时插件，而是先把工具链侧扩展的版本协商面固定下来。

## 2. 当前策略

当前采用两级策略：

1. 先支持文件级扩展
2. 插件级扩展后置

当前优先支持的扩展类型：

1. 导入器
2. 导出器
3. 校验器
4. 迁移器

这些扩展应优先运行在 CLI 或独立工具进程中，而不是直接进入 `vn_runtime` 主进程。

## 3. Manifest 最小字段

推荐最小字段：

| Field | Required | Meaning |
|---|---|---|
| `manifest_version` | yes | manifest 自身版本，当前建议 `1` |
| `name` | yes | 扩展唯一名 |
| `kind` | yes | `importer/exporter/validator/migrator` |
| `version` | yes | 扩展自身版本 |
| `api_range` | yes | 兼容的工具/宿主版本范围 |
| `capabilities` | yes | 能力位数组 |
| `entry` | yes | CLI 子命令或工具入口 |
| `platforms` | no | `linux/windows/*` 等平台范围 |
| `arches` | no | `x64/arm64/riscv64` 等架构范围 |
| `inputs` | no | 输入格式或资源面 |
| `outputs` | no | 输出格式或资源面 |
| `stability` | yes | `experimental/preview/stable` |

## 4. `api_range` 规则

当前建议：

1. 使用闭区间或显式版本列表
2. 至少声明：
   - `runtime api`
   - `script bytecode`
   - `vnpak`
   - `vnsave`
   - `preview protocol`
   - `tool manifest`
3. 若某项不适用，也要显式写 `n/a`

## 5. `capabilities` 规则

当前建议能力位：

1. `read.script`
2. `write.script`
3. `read.vnpak`
4. `write.vnpak`
5. `read.vnsave`
6. `write.vnsave`
7. `migrate.script`
8. `migrate.vnpak`
9. `migrate.vnsave`
10. `emit.report`

规则：

1. 新扩展必须最小声明所需能力位
2. 未声明的能力位视为不可用
3. 发布文档中的兼容矩阵应能映射这些能力位

## 6. 当前非目标

当前不承诺：

1. 运行时动态插件 ABI
2. 任意第三方二进制插件可直接注入主进程
3. 不带 manifest 的扩展可进入正式发布链

## 7. 示例

```json
{
  "manifest_version": 1,
  "name": "vnsave-migrate-v0-v1",
  "kind": "migrator",
  "version": "0.1.0",
  "api_range": {
    "runtime_api": "v1-draft",
    "script_bytecode": "v1",
    "vnpak": "v1-v2",
    "vnsave": "v0-v1",
    "preview_protocol": "v1",
    "tool_manifest": "v1"
  },
  "capabilities": [
    "read.vnsave",
    "write.vnsave",
    "migrate.vnsave",
    "emit.report"
  ],
  "entry": "vnsave_migrate",
  "platforms": ["linux", "windows"],
  "arches": ["x64", "arm64", "riscv64"],
  "stability": "preview"
}
```
