# Runtime API (`vn_runtime.h`)

## 1. 头文件

```c
#include "vn_runtime.h"
```

## 2. 设计目标

1. 以库调用为主，不依赖 `vn_player` 二进制。
2. 运行时接口保持 C89 兼容。
3. 提供配置输入 + 结果输出的结构化 API。

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

### `int vn_runtime_run_cli(int argc, char** argv)`

CLI 包装入口，主要用于调试与脚本调用。

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

## 6. 键盘模式

仅在类 Unix TTY 环境下可用。

按键：

1. `1-9`：设置分支选择索引
2. `t`：切换 trace
3. `q`：退出运行循环

## 7. 当前已知约束

1. 结构化 API 当前仍复用 CLI 核心执行路径。
2. 会话化 API（`create/step/destroy`）尚未完成，计划在 `ISSUE-014` 落地。
