# Error Codes

## 1. 目标

这份文档定义 N64GAL 当前公开错误码以及它们的稳定字符串表示。

对外约束：

1. 所有公开 `VN_*` 错误码定义在 `include/vn_error.h`
2. 字符串映射统一通过 `vn_error_name(int)` 提供
3. 宿主、preview、CI 和工具链不应各自维护私有错误名表

## 2. 当前错误码

| Code | Name | Meaning |
|---|---|---|
| `0` | `VN_OK` | 成功 |
| `-1` | `VN_E_INVALID_ARG` | 参数非法、空指针、范围越界 |
| `-2` | `VN_E_IO` | 文件或流 I/O 失败 |
| `-3` | `VN_E_FORMAT` | 输入格式错误、损坏或不兼容 |
| `-4` | `VN_E_UNSUPPORTED` | 当前平台或当前实现不支持 |
| `-5` | `VN_E_NOMEM` | 内存不足或容量不够 |
| `-6` | `VN_E_SCRIPT_BOUNDS` | 脚本或脚本索引越界 |
| `-7` | `VN_E_RENDER_STATE` | 渲染器状态非法或初始化失败 |
| `-8` | `VN_E_AUDIO_DEVICE` | 音频设备失败 |

## 3. 公共 API

```c
#include "vn_error.h"

const char* vn_error_name(int error_code);
```

规则：

1. 已知错误码返回稳定常量字符串
2. 未知错误码返回 `VN_E_UNKNOWN`
3. 返回值用于诊断、日志、协议输出，不应用来代替错误码本身做逻辑分支

## 4. `v1.x` 承诺边界

1. 当前错误码集合已经是公开 surface
2. `v1.x` 只允许追加错误码，不能重写已公开数值或含义
3. 已公开错误码的含义不应被偷偷重写
4. 若出现破坏性调整，必须同步更新 `README`、release note 和本文件

## 5. 当前建议

1. 对外协议优先同时输出 `error_code` 和 `error_name`
2. 若输出 machine-readable 日志或协议，还应同时输出稳定 `trace_id`
3. 宿主优先按数值错误码做程序逻辑判断
4. `vn_error_name()` 只作为可读诊断层

## 6. `trace_id` 约定

当前仓库已经开始把稳定 `trace_id` 用到公开输出面：

1. `preview.request.*`
2. `preview.runtime.*`
3. `preview.event.*`
4. `runtime.cli.arg.*`
5. `runtime.cli.scene.invalid`
6. `runtime.run.ok`
7. `runtime.run.failed`
8. `tool.vnsave_migrate.*`

规则：

1. `trace_id` 用于稳定归类，不应用自然语言 message 代替
2. 同一类错误应尽量复用同一个 `trace_id`
3. 详细上下文继续通过 `error_code/error_name/message` 和附加字段表达

## 7. 公开版本协商相关错误

当前公开面里，以下“版本不匹配”场景继续统一落到已有错误码：

1. `preview protocol` 版本不支持
   - 返回 `VN_E_UNSUPPORTED`
2. `vnsave` 为 `pre-1.0` / 更高未来版本
   - 返回 `VN_E_UNSUPPORTED`
3. `vnpak` 版本超出当前声明兼容范围
   - 返回 `VN_E_FORMAT`

这意味着：

1. 版本协商新增为公开 surface 后，不需要额外再造一套错误码
2. 宿主可继续按 `error_code + error_name + trace_id` 统一处理

补充说明：

1. 即使 `preview` 的实现拆到多个源码文件，`trace_id/error_code/error_name` 语义也不应变化
2. 当前 `preview` 继续拆成 `preview_parse.c / preview_cli.c / preview_report.c` 只影响可维护性，不影响公开错误语义

## 8. `v1.1.0` 真实内容错误映射

`v1.1.0` 的项目、场景目录和真实纹理沿用现有错误码：

1. 项目声明参数非法、场景名不合规或数值越界：`VN_E_INVALID_ARG`
2. 场景哈希冲突、未知资源引用、资源目录/像素载荷损坏或 CRC 错误：`VN_E_FORMAT`
3. 包或截图文件读写失败：`VN_E_IO`
4. 纹理缓存或 framebuffer 分配失败：`VN_E_NOMEM`
5. 旧 snapshot API 无法无损表达内容场景状态：`VN_E_UNSUPPORTED`
6. backend 收到非法纹理 render op 或未初始化渲染状态：`VN_E_RENDER_STATE`

这组能力不新增重复的专用错误码；工具与宿主继续依据 `error_code + error_name + trace_id` 处理。
