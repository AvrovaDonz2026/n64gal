# Preview Protocol (`v1`)

## Goals

1. 为编辑器、自动化脚本和 CI 提供统一的无 GUI 预览入口。
2. 不把预览能力绑定到 `vn_player`，而是直接复用 `vn_runtime_session_*`。
3. 在 Linux/Windows 上保持相同的请求、响应和退出码语义。
4. 先冻结 `CLI + 文件协议`，后续再升级本地 IPC，而不打破 `v1` 字段语义。

## Entry Points

当前 `v1` 提供两个等价入口：

1. `vn_previewd` 可执行程序
2. `vn_preview_run_cli(int argc, char** argv)`
   - 头文件：`include/vn_preview.h`

推荐关系：

1. 编辑器/脚本工具优先调用 `vn_previewd`
2. 仓库内测试可直接调用 `vn_preview_run_cli`
3. 宿主程序若已直接使用 Session API，可把 `preview protocol` 视为更高层的工具协议，而不是必须依赖的 ABI

## Transport Model

`v1` 采用单次请求/单次响应模型：

1. 输入：CLI 参数，或 `key=value` 请求文件
2. 输出：JSON 响应，写到 `stdout` 或 `--response=<path>`
3. 控制：通过重复 `command=` 描述本次会话内要执行的动作序列
4. 非目标：当前不提供长连接、socket、named pipe 或共享内存

这意味着：

1. `reload_scene` 是“销毁并重建当前 session”
2. `step_frame` 是“在同一 session 内推进若干帧”
3. 外部工具如果需要持续交互，可以多次启动 `vn_previewd`，或后续升级到 `v2` IPC

## CLI Syntax

```bash
./build/vn_previewd \
  --project-dir=. \
  --scene=S2 \
  --backend=auto \
  --resolution=600x800 \
  --frames=8 \
  --trace \
  --command=set_choice:1 \
  --command=inject_input:choice:1 \
  --command=inject_input:key:t \
  --command=step_frame:8 \
  --response=/tmp/preview_response.json
```

常用参数：

1. `--request=<path>`
2. `--response=<path>`
3. `--project-dir=<path>`
4. `--pack=<path>`
5. `--scene=<name>`
6. `--backend=auto|scalar|avx2|neon|rvv`
7. `--resolution=<width>x<height>`
8. `--frames=<n>`
9. `--dt-ms=<n>`
10. `--trace`
11. `--hold-end`
12. `--choice-index=<n>`
13. `--choice-seq=0,1,0`
14. `--command=<command>`

退出码：

1. `0`：请求成功，JSON `status="ok"`
2. `1`：运行失败，JSON `status="error"`
3. `2`：参数或请求文件格式错误，JSON `status="error"`

## Request File Format

请求文件是严格的 `key=value` 文本协议：

1. 空行忽略
2. `#` / `;` 开头的行视为注释
3. 未知 key 直接报错，避免编辑器拼写错误被静默吞掉
4. 重复的 `command=` 行会按出现顺序追加执行

示例：

```ini
preview_protocol=v1
project_dir=.
scene_name=S2
backend=auto
resolution=600x800
frames=8
trace=1
command=set_choice:1
command=inject_input:choice:1
command=inject_input:key:t
command=step_frame:8
response=tests/integration/preview_protocol_response.tmp.json
```

## Request Keys

| Key | Required | Meaning |
|---|---|---|
| `preview_protocol` | recommended | 当前固定为 `v1` |
| `project_dir` | no | 相对 `pack_path` 的解析基准目录 |
| `pack_path` / `pack` | no | `.vnpak` 路径；默认 `assets/demo/demo.vnpak` |
| `scene_name` / `scene` | no | 逻辑场景名；默认 `S0` |
| `backend` | no | `auto|scalar|avx2|neon|rvv` |
| `resolution` | no | 形如 `600x800` |
| `width` / `height` | no | 分开给宽高；若与 `resolution` 同时出现，以最后写入值为准 |
| `frames` | no | session 最大推进帧数 |
| `dt_ms` | no | 逻辑帧时间步长 |
| `trace` | no | `0/1/true/false/on/off/yes/no` |
| `hold_on_end` | no | `END` 后继续推进 |
| `choice_index` | no | 默认选择索引 |
| `choice_seq` | no | 预置选择序列，如 `0,1,0` |
| `response` / `response_path` | no | JSON 输出路径 |
| `command` | no | 预览控制命令，可重复 |

路径解析规则：

1. 若 `pack_path` 是绝对路径，则直接使用
2. 若 `pack_path` 是相对路径且存在 `project_dir`，相对 `project_dir` 解析
3. 若未提供 `project_dir` 但使用了 `--request=<file>`，则相对请求文件所在目录解析
4. 若两者都不存在，则按当前工作目录解析

## Commands

当前支持的控制命令：

1. `run_to_end`
   - 推进直到 session 结束，或达到 `frames` 上限
2. `step_frame`
   - 推进 1 帧
3. `step_frame:<n>`
   - 推进 `n` 帧
4. `reload_scene`
   - 销毁并按同一配置重建 session
5. `set_choice:<n>`
   - 调用 `vn_runtime_session_set_choice`
6. `inject_input:choice:<n>`
   - 通过 `vn_runtime_session_inject_input` 注入离散分支选择
7. `inject_input:key:<c>`
   - 注入单个键值；当前支持 `1-9`、`t/T`、`q/Q`
8. `inject_input:trace_toggle`
   - 注入一次 trace 切换事件
9. `inject_input:quit`
   - 注入一次退出事件

兼容性说明：

1. `inject_input` 在 `v1` 当前支持 `choice`、`key`、`trace_toggle`、`quit` 四类事件
2. `inject_input:key:<c>` 当前只接受单字节 ASCII，并仅保证 `1-9`、`t/T`、`q/Q` 的运行时语义
3. 键盘、鼠标、触摸、文本输入等更细输入种类仍保留到后续版本
4. 将来若新增 `inject_input:<kind>:...`，必须保持现有 `choice` / `key` / `trace_toggle` / `quit` 语义不变

## Response JSON

响应顶层字段：

| Field | Meaning |
|---|---|
| `preview_protocol` | 当前固定 `v1` |
| `status` | `ok` 或 `error` |
| `error_code` | `VN_*` 错误码或运行错误码 |
| `error_name` | 文本错误名 |
| `error_message` | 稳定的人类可读错误描述 |
| `host_os` | `linux` / `windows` / ... |
| `host_arch` | `x64` / `arm64` / `riscv64` / ... |
| `request` | 回显本次解析后的有效请求 |
| `summary` | 统计摘要 |
| `perf_summary` | 基于 host 侧 step 包围时间的摘要 |
| `first_frame` | 首个成功推进帧的快照 |
| `last_frame` | 最后一个成功推进帧的快照 |
| `final_state` | 最终 `VNRunResult` |
| `events` | 结构化事件日志 |

### `request`

回显的字段是“解析后的最终值”，因此可以直接用于诊断：

1. `project_dir`
2. `pack_path`
3. `scene_name`
4. `backend_name`
5. `width`
6. `height`
7. `frames`
8. `dt_ms`
9. `trace`
10. `hold_on_end`
11. `choice_index`
12. `choice_seq_count`
13. `command_count`

### `summary`

当前字段：

1. `reload_count`
2. `frame_samples`
3. `session_done`
4. `events_truncated`

### `perf_summary`

当前字段：

1. `samples`
2. `total_step_ms`
3. `avg_step_ms`
4. `max_step_ms`

说明：

1. 这是围绕 `vn_runtime_session_step` 的 host 侧时间包围统计
2. 适合作为预览诊断或 editor telemetry
3. 不替代 `tests/perf/` 的正式性能基准

### Frame Snapshot

`first_frame` / `last_frame` 结构：

1. `host_step_ms`
2. `result`

`result` 当前回显以下 `VNRunResult` 字段：

1. `frames_executed`
2. `text_id`
3. `vm_waiting`
4. `vm_ended`
5. `vm_error`
6. `fade_alpha`
7. `fade_remain_ms`
8. `bgm_id`
9. `se_id`
10. `choice_count`
11. `choice_selected_index`
12. `choice_text_id`
13. `op_count`
14. `perf_flags_effective`
15. `frame_reuse_hits`
16. `frame_reuse_misses`
17. `op_cache_hits`
18. `op_cache_misses`
19. `dirty_tile_count`
20. `dirty_rect_count`
21. `dirty_full_redraw`
22. `dirty_tile_frames`
23. `dirty_tile_total`
24. `dirty_rect_total`
25. `dirty_full_redraws`
26. `render_width`
27. `render_height`
28. `dynamic_resolution_tier`
29. `dynamic_resolution_switches`
30. `backend_name`

### `events`

`v1` 事件流是 append-only：

1. `set_choice`
2. `inject_input`
3. `inject_input.key`
4. `inject_input.trace_toggle`
5. `inject_input.quit`
6. `step_frame`
7. `run_to_end`
8. `reload_scene`
9. `frame`
10. `VN_*` 错误事件

当 `trace=1` 时，会额外记录逐帧 `frame` 事件。
当事件数量超过实现上限时，不会让请求失败，而是把 `summary.events_truncated` 置为 `1`。

## Example Response

```json
{
  "preview_protocol": "v1",
  "status": "ok",
  "error_code": 0,
  "error_name": "VN_OK",
  "host_os": "linux",
  "host_arch": "x64",
  "summary": {
    "reload_count": 0,
    "frame_samples": 8,
    "session_done": 1,
    "events_truncated": 0
  },
  "perf_summary": {
    "samples": 8,
    "avg_step_ms": 0.031
  }
}
```

## Compatibility Rules

1. `v1` 响应字段仅允许追加，不允许重命名或改变现有含义。
2. `request` 中未知 key 当前直接报错；后续若放宽，必须只对新版本生效。
3. `events.type` 只是实现侧分类编号，稳定消费应优先看 `events.kind` 文本名。
4. `final_state=null` 表示当前 session 尚未成功推进任何一帧，或在 `reload_scene` 后尚未再次执行 `step_frame/run_to_end`。

## Validation

仓库内当前验证链：

1. `tests/integration/test_preview_protocol.c`
2. `tests/unit/test_runtime_input.c`
3. `scripts/ci/run_cc_suite.sh`
4. `ctest -R integration_preview_protocol`（当本地具备 `cmake` 时）

## Non-Goals

`v1` 当前不承诺：

1. 多请求长连接
2. GUI 渲染或窗口集成
3. 二进制帧缓冲导出
4. 远程预览协议
5. 多会话并发隔离

这些能力可以在后续版本增加，但必须保持 `v1` 请求/响应字段兼容。
