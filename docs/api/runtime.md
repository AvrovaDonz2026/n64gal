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
   - 非 0 打印逐帧状态
9. `keyboard`
   - 非 0 启用键盘输入（TTY 环境）
10. `emit_logs`
   - 非 0 输出日志，0 时静默运行

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

## 4. API 函数

### `void vn_run_config_init(VNRunConfig* cfg)`

初始化默认配置，建议总是先调用。

### `int vn_runtime_run(const VNRunConfig* cfg, VNRunResult* out_result)`

结构化运行入口。

返回值：

1. `0`：成功
2. 非 0：失败（参数错误、资源加载失败、渲染初始化失败、VM 错误等）

### `int vn_runtime_session_create(const VNRunConfig* cfg, VNRuntimeSession** out_session)`

创建运行时会话并完成初始化（加载 pack、加载脚本、初始化 VM、初始化渲染后端）。

### `int vn_runtime_session_step(VNRuntimeSession* session, VNRunResult* out_result)`

推进一帧执行并返回最新状态快照。

行为要点：

1. 每次调用最多推进 1 帧。
2. 支持 `choice_seq` 和 `vn_runtime_session_set_choice` 的分支注入。
3. 当运行结束且 `vm_error != 0` 时返回非 0。

### `int vn_runtime_session_is_done(const VNRuntimeSession* session)`

查询会话是否结束（帧数到达、脚本结束、错误、或键盘退出）。

### `int vn_runtime_session_set_choice(VNRuntimeSession* session, vn_u8 choice_index)`

设置默认分支选择索引，供后续 `CHOICE` 指令消费。

### `int vn_runtime_session_destroy(VNRuntimeSession* session)`

销毁会话并释放资源（后端 shutdown、键盘状态恢复、脚本内存释放、pack 关闭）。

### `int vn_runtime_run_cli(int argc, char** argv)`

CLI 包装入口，主要用于调试与脚本调用。参数解析后会转调 `vn_runtime_run`。

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
    int rc;

    vn_run_config_init(&cfg);
    cfg.scene_name = "S2";
    cfg.frames = 300u;
    cfg.emit_logs = 0u;

    rc = vn_runtime_session_create(&cfg, &session);
    if (rc != 0) {
        return rc;
    }

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

按键：

1. `1-9`：设置分支选择索引
2. `t`：切换 trace
3. `q`：退出运行循环

## 8. 当前已知约束

1. 运行时会话当前是单实例全局渲染后端模型，不支持并发多会话。
2. Windows 平台暂未提供键盘非阻塞输入实现（会返回 `VN_E_UNSUPPORTED`）。
3. `vn_runtime_run_cli` 保留进程级退出码语义（参数错误返回 `2`，运行失败返回 `1`）。
