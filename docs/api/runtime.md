# Runtime API (`vn_runtime.h`)

## 1. 头文件

```c
#include "vn_runtime.h"
```

## 2. 设计目标

1. 以库调用为主，不依赖 `vn_player` 二进制。
2. 运行时接口保持 C89 兼容。
3. 提供“一次性运行 + 会话化运行”两套库 API。
4. CLI 仅作为包装层，核心行为由运行时会话层统一承载。
5. 宿主、preview、自动化脚本应复用同一套 Session API 与输入注入接口。

## 3. 结构体

### `VNRunConfig`

运行输入配置。

关键字段：

1. `pack_path`
   - 资源包路径（默认 `assets/demo/demo.vnpak`）
2. `scene_name`
   - 场景名：`S0` / `S1` / `S2` / `S3`
3. `backend_name`
   - `"auto"` / `"scalar"` / `"avx2"` / `"neon"` / `"rvv"`
4. `width`, `height`
   - 输出分辨率，默认 `600x800`
5. `frames`, `dt_ms`
   - 运行帧数与每帧步长
6. `choice_index`
   - 默认分支选择（`0` 表示第 1 个选项）
7. `choice_seq[]`, `choice_seq_count`
   - 分支选择序列，按 `CHOICE` 发生顺序消费
8. `trace`
   - 非 0 打印逐帧状态与性能采样（`frame_ms/vm_ms/build_ms/raster_ms/audio_ms/rss_mb`）
9. `keyboard`
   - 非 0 启用键盘输入（Linux TTY / Windows console 调试模式）
10. `emit_logs`
   - 非 0 输出日志，0 时静默运行
11. `hold_on_end`
   - 非 0 时脚本到达 `END` 后继续维持帧循环直到 `frames` 用尽（主要用于 perf 采样）
12. `perf_flags`
   - 运行时性能特性开关位图
   - 默认值为 `VN_RUNTIME_PERF_DEFAULT_FLAGS`
   - 当前已公开：`VN_RUNTIME_PERF_OP_CACHE`（Frontend `VNRenderOp[]` LRU 命令缓存）
   - 当前实现会折叠 `frame_index` 派生的前端伪动画键值，并在命中时按当前帧回写 `SPRITE/FADE` 动态字段，避免因占位动画导致缓存长期 0 hit

### `VNInputEvent`

会话级输入注入结构，供宿主、preview 协议和自动化脚本复用。

字段：

1. `kind`
2. `value0`
3. `value1`

当前公开的 `kind`：

1. `VN_INPUT_KIND_CHOICE`
   - `value0=<choice_index>`
2. `VN_INPUT_KIND_KEY`
   - `value0=<ascii>`
   - 当前支持与 CLI 键盘模式一致的键：`1-9`、`t/T`、`q/Q`
3. `VN_INPUT_KIND_TRACE_TOGGLE`
   - 等价于注入一次 trace 切换信号
4. `VN_INPUT_KIND_QUIT`
   - 等价于请求结束当前 session

### `VNRunResult`

运行结果摘要。

关键字段：

1. `frames_executed`
2. `text_id`
3. `vm_waiting`, `vm_ended`, `vm_error`
4. `fade_alpha`, `fade_remain_ms`
5. `bgm_id`, `se_id`
6. `choice_count`, `choice_selected_index`, `choice_text_id`
7. `op_count`
8. `backend_name`
9. `perf_flags_effective`
10. `op_cache_hits`, `op_cache_misses`

## 4. API 函数

### `void vn_run_config_init(VNRunConfig* cfg)`

初始化默认配置，建议总是先调用。

### `int vn_runtime_run(const VNRunConfig* cfg, VNRunResult* out_result)`

结构化运行入口。

行为补充：

1. 常规模式下不承诺真实时间节流。
2. 当 `cfg->keyboard != 0` 且 `cfg->dt_ms > 0` 时，`vn_runtime_run()` 会在帧与帧之间调用平台层 sleep，以便 CLI 调试模式可交互。

返回值：

1. `0`：成功
2. 非 0：失败（参数错误、资源加载失败、渲染初始化失败、VM 错误等）

### `int vn_runtime_session_create(const VNRunConfig* cfg, VNRuntimeSession** out_session)`

创建运行时会话并完成初始化（加载 pack、加载脚本、初始化 VM、初始化渲染后端）。

### `int vn_runtime_session_step(VNRuntimeSession* session, VNRunResult* out_result)`

推进一帧执行并返回最新状态快照。

行为要点：

1. 每次调用最多推进 1 帧。
2. 支持 `choice_seq`、`vn_runtime_session_set_choice` 与 `vn_runtime_session_inject_input` 的输入注入。
3. `vn_runtime_session_inject_input` 注入的事件会在下一次 `step` 时消费。
4. 当运行结束且 `vm_error != 0` 时返回非 0。
5. 若启用 `VN_RUNTIME_PERF_OP_CACHE`，则会对 `VNRenderOp[]` 构建结果做 LRU 缓存，并在 `VNRunResult` 中回传命中统计。

### `int vn_runtime_session_is_done(const VNRuntimeSession* session)`

查询会话是否结束（帧数到达、脚本结束、错误、或退出输入被消费）。

### `int vn_runtime_session_set_choice(VNRuntimeSession* session, vn_u8 choice_index)`

设置默认分支选择索引，供后续 `CHOICE` 指令消费。

适合：

1. 宿主 UI 的“当前默认选项”
2. 测试框架在多帧推进前设置稳定缺省值

### `int vn_runtime_session_inject_input(VNRuntimeSession* session, const VNInputEvent* event)`

向 session 注入一次“下一帧消费”的输入事件。

当前建议用途：

1. 宿主把 UI 选择写入 runtime
2. editor / preview 协议做脚本化驱动
3. 测试框架注入 quit / trace toggle / key 等控制信号

返回值：

1. `VN_OK`：注入成功
2. `VN_E_INVALID_ARG`：空指针或值越界
3. `VN_E_UNSUPPORTED`：当前 `kind` 或 key 值未实现

### `int vn_runtime_session_destroy(VNRuntimeSession* session)`

销毁会话并释放资源（后端 shutdown、键盘状态恢复、脚本内存释放、pack 关闭）。

### `int vn_runtime_run_cli(int argc, char** argv)`

CLI 包装入口，主要用于调试与脚本调用。参数解析后会转调 `vn_runtime_run`。

扩展参数：

1. `--hold-end`
   - 对应 `VNRunConfig.hold_on_end=1`
   - 用于场景脚本提前结束时仍持续输出帧采样数据
2. `--perf-op-cache=<on|off>`
   - 切换 `VN_RUNTIME_PERF_OP_CACHE`
   - 默认 `on`（来自 `VN_RUNTIME_PERF_DEFAULT_FLAGS`）

## 5. 最小示例（推荐集成方式）

```c
#include <stdio.h>
#include "vn_runtime.h"

int main(void) {
    VNRunConfig cfg;
    VNRunResult res;
    int rc;

    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 120u;
    cfg.dt_ms = 16u;
    cfg.choice_index = 1u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_run(&cfg, &res);
    if (rc != 0) {
        return 1;
    }

    printf("backend=%s frames=%u text=%u\n",
           res.backend_name,
           (unsigned int)res.frames_executed,
           (unsigned int)res.text_id);
    return 0;
}
```

## 6. 会话化示例（宿主循环）

```c
#include "vn_runtime.h"

int run_scene_once(void) {
    VNRunConfig cfg;
    VNRunResult res;
    VNRuntimeSession* session;
    VNInputEvent input;
    int rc;

    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 300u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != 0) {
        return rc;
    }

    input.kind = VN_INPUT_KIND_CHOICE;
    input.value0 = 1u;
    input.value1 = 0u;
    (void)vn_runtime_session_inject_input(session, &input);

    while (vn_runtime_session_is_done(session) == 0) {
        rc = vn_runtime_session_step(session, &res);
        if (rc != 0) {
            break;
        }
    }

    (void)vn_runtime_session_destroy(session);
    return rc;
}
```

## 7. 键盘模式

仅在类 Unix TTY 环境下可用。

这些按键语义与 `vn_runtime_session_inject_input` 的 `VN_INPUT_KIND_KEY` 保持一致。

按键：

1. `1-9`：设置分支选择索引
2. `t`：切换 trace
3. `q`：退出运行循环

## 8. Trace 输出（性能采样）

启用 `trace` 时，每帧会输出包含以下键值对的单行日志：

1. `frame`
2. `frame_ms`
3. `vm_ms`
4. `build_ms`
5. `raster_ms`
6. `audio_ms`
7. `rss_mb`

`tests/perf/run_perf.sh` 基于这些字段生成 `perf_<scene>.csv` 与 `perf_summary.csv`。

宿主集成的更完整说明见 [`../host-sdk.md`](../host-sdk.md)。

## 9. 当前已知约束

1. 运行时会话当前是单实例全局渲染后端模型，不支持并发多会话。
2. Windows 平台的 `keyboard` 调试模式已接入 `_kbhit/_getch`；非 console 环境仍不应把它视为宿主输入接口。
3. `VN_INPUT_KIND_KEY` 当前只保证 `1-9`、`t/T`、`q/Q` 的运行时语义。
4. `vn_runtime_run_cli` 保留进程级退出码语义（参数错误返回 `2`，运行失败返回 `1`）。
