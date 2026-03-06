# Host SDK Guide

## Goals

1. 让宿主程序直接嵌入 `vn_runtime`，而不是依赖 `vn_player` 二进制。
2. 保持 Frontend / Backend / ISA 私有实现对宿主不可见。
3. 把宿主最小调用序列、输入桥接、文件桥接和版本协商写清楚。

## Public Surface

宿主当前应只依赖这些公共头：

1. `vn_runtime.h`
2. `vn_error.h`
3. `vn_types.h`

宿主不应直接依赖：

1. `src/backend/*` 私有实现
2. VM 内部结构布局
3. `vn_player` CLI 参数解析逻辑

## Minimal Host Flow

推荐调用顺序：

1. `vn_run_config_init(&cfg)`
2. 覆盖宿主所需字段：`pack_path/scene_name/backend_name/width/height/frames/dt_ms/...`
3. `vn_runtime_session_create(&cfg, &session)`
4. 宿主主循环内反复调用 `vn_runtime_session_step(session, &result)`
5. 在需要时调用 `vn_runtime_session_set_choice` 或 `vn_runtime_session_inject_input`
6. 通过 `vn_runtime_session_is_done(session)` 判断是否结束
7. 退出前调用 `vn_runtime_session_destroy(session)`

最小模式适合：

1. 游戏宿主自己的帧循环
2. 测试框架驱动单帧推进
3. 编辑器预览与自动化采样

## Session API Contract

### `vn_runtime_session_create`

负责：

1. 加载 pack
2. 加载脚本
3. 初始化 VM
4. 初始化 renderer/backend
5. 建立输入与日志初始状态

### `vn_runtime_session_step`

约束：

1. 每次调用最多推进一帧逻辑
2. 不承诺真实时间睡眠
3. 输出最新 `VNRunResult`
4. 出错返回负值错误码
5. 在本次调用前通过 `vn_runtime_session_inject_input` 写入的事件，会在本帧消费

### `vn_runtime_session_is_done`

返回 `VN_TRUE` 的条件包括：

1. 帧数用尽
2. VM 结束
3. VM 错误
4. quit 输入已消费
5. 空 session 指针

### `vn_runtime_session_set_choice`

用途：

1. 覆盖后续 `CHOICE` 的默认选择
2. 适合把外部 UI 的“当前默认选项”写回运行时

### `vn_runtime_session_inject_input`

用途：

1. 注入下一帧消费的离散输入事件
2. 适合 editor / preview / 自动化脚本的单步驱动
3. 适合宿主把 trace toggle / quit / key / choice 等控制信号传入 runtime

当前公开的事件种类：

1. `VN_INPUT_KIND_CHOICE`
2. `VN_INPUT_KIND_KEY`
3. `VN_INPUT_KIND_TRACE_TOGGLE`
4. `VN_INPUT_KIND_QUIT`

### `vn_runtime_session_destroy`

负责释放：

1. pack
2. 脚本内存
3. backend 全局状态
4. 键盘状态

## Input Bridge

宿主输入建议统一映射到两层：

1. 持续默认输入
   - 使用 `vn_runtime_session_set_choice`
   - 或在创建前写入 `choice_seq[]`
2. 单次事件输入
   - 使用 `vn_runtime_session_inject_input`
   - 当前已公开：`CHOICE`、`KEY`、`TRACE_TOGGLE`、`QUIT`

建议映射：

1. UI 分支选择
   - 立即生效：`VN_INPUT_KIND_CHOICE`
   - 作为默认值：`vn_runtime_session_set_choice`
2. 运行控制输入
   - trace 开关：`VN_INPUT_KIND_TRACE_TOGGLE`
   - 退出：`VN_INPUT_KIND_QUIT`
3. 调试键盘等价输入
   - `VN_INPUT_KIND_KEY`
   - 当前与 CLI 键盘模式对齐：`1-9`、`t/T`、`q/Q`

当前运行时内置键盘模式主要用于调试，不应被视为宿主 API 的唯一输入来源；宿主与 preview 工具应优先绑定公开的 Session 输入注入接口。

## File Bridge

当前宿主文件桥接是“路径级”约定：

1. `pack_path` 指向 `.vnpak`
2. `scene_name` 指向 pack 内逻辑场景名
3. 运行时自行打开 pack 并读取资源

宿主当前不需要提供自定义 I/O 回调；如果后续引入文件桥接接口，必须保持向后兼容，不破坏现有路径模式。

## Logging And Trace Bridge

宿主可以通过配置控制日志行为：

1. `emit_logs=0`
   - 关闭普通日志输出，适合静默嵌入
2. `trace=1`
   - 输出逐帧 `frame_ms/vm_ms/build_ms/raster_ms/audio_ms/rss_mb`
3. `hold_on_end=1`
   - 在脚本到达 `END` 后继续推进，适合性能采样

当前日志出口仍是标准输出/标准错误。
后续若增加宿主日志回调，必须保留现有文本模式作为兼容回退路径。

## Host-Facing Result Fields

宿主循环通常只需要读取：

1. `frames_executed`
2. `text_id`
3. `vm_waiting`
4. `vm_ended`
5. `vm_error`
6. `choice_count`
7. `choice_selected_index`
8. `op_count`
9. `backend_name`

这些字段足以支撑：

1. UI 更新
2. 自动化检查
3. 基本 perf 采样
4. 场景结束判断

## Version Negotiation Matrix

当前建议宿主按以下版本面做兼容判断：

| Surface | Current Status | Negotiation Rule |
|---|---|---|
| `runtime api` | `v1` | 仅追加字段/函数，旧宿主不应依赖未文档化结构 |
| `backend abi` | `v1` | 宿主不直接链接私有 backend；由 runtime 内部选择 |
| `script bytecode` | `v1` | 运行时只保证读取已声明兼容版本 |
| `vnpak` | `v2` 当前默认，兼容读取 `v1` | 生成端默认写 `v2`，读取端兼容 `v1/v2` |
| `vnsave` | `planned` | 未正式对外，未来发布必须附迁移规则 |
| `preview protocol` | `v1` | `vn_previewd` / `vn_preview_run_cli` 固定 `CLI + 文件 -> JSON` 语义，后续仅追加字段 |

规则：

1. 宿主只应绑定公开头文件语义，不应把源码目录结构视为 ABI。
2. `runtime api` 与 `backend abi` 的破坏性变更必须伴随版本升级和迁移说明。
3. `script/vnpak/vnsave` 的版本策略必须写进 release 文档和 issue 证据链。

## Linux And Windows Integration Notes

1. Linux/Windows 的宿主都应优先使用相同的 Session API。
2. 键盘调试模式不是跨平台宿主输入方案；Linux/Windows CLI 现在共享 `1-9` / `t` / `q` 语义，但宿主仍应优先使用 Session 输入注入接口。
3. 路径和二进制文件模式由 runtime/pack 层处理，宿主不应假定文本模式 I/O 足够。
4. 后端选择应默认使用 `backend_name="auto"`，除非宿主明确要做诊断或基准对比。

## Example

最小嵌入示例见：

- `examples/host-embed/session_loop.c`
- `examples/host-embed/README.md`

当前该示例已接入：

1. `CMake` target: `example_host_embed`
2. `ctest` item: `example_host_embed`
3. `scripts/ci/run_cc_suite.sh` 本地 C89 套件

## Non-Goals

当前 Host SDK 文档不承诺：

1. 多会话并发安全
2. 自定义文件系统回调
3. 动态插件 ABI
4. GUI 编辑器内部通信协议（已落地的 `preview protocol v1` 除外）

这些都属于后续 `M4-engine-ecosystem` 范围，必须在协议和版本面冻结后再推进。
