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
6. `v1.1.0` 在旧场景回归路径之外加入 `VNSC` 场景目录、真实图片资源、frame view 与版本化 snapshot 扩展。

## 2.1 当前版本承诺级别

`vn_runtime.h` 当前应按“`public stable v1`”理解：

1. 它已经是公开头文件，宿主可以基于文档化接口接入。
2. 当前已文档化的函数、常量与结构体字段应视为首版正式公开面。
3. 后续 `v1.x` 仅应做兼容追加，不应静默改写已公开字段语义或移除已文档化入口。
4. 宿主不应依赖未文档化结构布局、私有状态或源码目录结构。
5. 任何未来破坏性变更都必须通过显式版本升级、release note 与兼容矩阵说明完成。

## 2.2 当前性能扩展状态

1. 当前 runtime 已公开 `frame reuse + op cache + dirty tile + dynamic resolution` 四条 perf 开关。
2. `Dirty-Tile` 的设计背景、阶段拆分与实现约束仍单独整理在 [`dirty-tile-draft.md`](./dirty-tile-draft.md)。
3. 动态分辨率当前已落地 runtime 最小 slice：`VN_RUNTIME_PERF_DYNAMIC_RESOLUTION`、`--perf-dynamic-resolution=<on|off>`、以及 `VNRunResult.render_width/render_height/dynamic_resolution_*` 已可直接使用。
4. `dirty tile` 与 `dynamic resolution` 当前都默认 `off`；先以可观测、可回退、可做 perf compare 为第一目标，再决定是否提升为默认路径。

## 2.3 当前实现落点

当前 runtime 实现按职责拆分为：

1. `src/core/runtime_cli.c`
   - Session 驱动
   - Session create/step/run 主流程
   - `load-save / fresh-session` 两条 CLI 执行分流
2. `src/core/runtime_input.c`
   - keyboard 输入轮询与按键映射
   - injected input 合并
   - fade player 状态推进
3. `src/core/runtime_parse.c`
   - CLI 参数解析
   - scene/backend 字符串映射
   - CLI structured error 输出
4. `src/core/runtime_session_support.c`
   - scene script 装载
   - runtime state 同步与 session cleanup
5. `src/core/runtime_session_loop.c`
   - session step 主循环
   - trace / summary / result publish
6. `src/core/runtime_persist.c`
   - build info 查询
   - snapshot encode/decode
   - file save/load wrapper
7. `src/core/scene_catalog.c`
   - `VNSC v1` 场景目录加载与名称/ID 查询
8. `src/core/runtime_texture.c`
   - 图片资源校验、CRC 读取与 32 MiB LRU 缓存

这次拆分的目标是降低单文件维护压力，不改变当前公开 API 语义。
`runtime_session_loop.c` 在进入 frame reuse / render-op 重建分支前会从 session 的上一帧结果初始化本地 op 数量；这是 C89/MSVC 严格告警兼容修正，不改变成功帧的 `VNRunResult.op_count` 语义。

## 3. 结构体

### `VNRuntimeBuildInfo`

用于把“版本协商/构建协商”从纯文档推进成可查询公开面。

关键字段：

1. `runtime_api_version`
2. `runtime_api_stability`
3. `preview_protocol_version`
4. `vnpak_read_min_version`, `vnpak_read_max_version`, `vnpak_write_default_version`
5. `vnsave_latest_version`, `vnsave_api_stability`, `vnsave_public_saveload_scope`
6. `host_os`, `host_arch`, `host_compiler`

当前用途：

1. 宿主在运行时确认当前 build 的公开版本边界
2. 工具或 smoke 测试读取当前 host/build 元信息
3. 避免第三方宿主只能靠 README/文档猜测当前 runtime 口径

### `VNRuntimeBuildInfoV2`

兼容追加的版本化 build info。调用者先用 `vn_runtime_build_info_v2_init(...)` 初始化，再查询：

1. `struct_size` 与 `version` 用于结构版本协商
2. `engine_version` 当前为 `1.1.0`
3. `capability_flags` 当前声明 `SCENE_CATALOG / RESOURCE_TEXTURE / FRAME_VIEW / SNAPSHOT_V2`
4. `v1` 完整嵌入原有 `VNRuntimeBuildInfo`，旧查询入口和旧结构布局不变

### `VNRuntimeSessionSnapshot`

用于捕获一个“可恢复”的 live session 快照，并在后续重新创建 session。

当前设计边界：

1. 这是 runtime API 的最小正式 persistence 面，不是通用宿主 save/load ABI
2. 它解决的是“可恢复当前 runtime session”，不是抽象的长期宿主持久化协议
3. dirty planner、frame reuse、op cache 等 perf 缓存不会被保留；恢复后允许从干净状态重新建立
4. 当前只面向 live session；已结束 session、带待消费 injected input 的 session 不保证可捕获

关键字段：

1. `pack_path`, `backend_name`, `scene_id`
2. `base_width/base_height`, `frames_limit/frames_executed`, `dt_ms`
3. `trace`, `emit_logs`, `hold_on_end`, `perf_flags`, `keyboard_enabled`
4. `default_choice_index`, `choice_feed_items[]`, `choice_feed_count`, `choice_feed_cursor`
5. `dynamic_resolution_tier`, `dynamic_resolution_switches`
6. `vm_*` 系列字段
7. `fade_*` 系列字段

当前语义：

1. VM 执行状态、fade 状态、choice feed 与 dynres tier 会被保留
2. framebuffer、dirty planner、op cache、frame reuse 统计与 pending injected input 不会保留
3. 恢复后下一帧的用户可见语义应与未中断继续推进一致，但 perf 计数可重新累计
4. 若保存点恰好耗尽原 frame budget，恢复时追加一份原预算，使循环场景仍可推进
5. 已执行 `END` 或进入 VM error 的终态不可捕获或恢复，相关入口返回 `VN_E_UNSUPPORTED`，不会生成伪可恢复 session
6. snapshot 的 `vm_flags` 含未知位时，恢复入口返回 `VN_E_INVALID_ARG`

`VNRuntimeSessionSnapshot` 是保留给 legacy 场景的 v1 结构。对真实内容 session 调用旧 capture API 会返回 `VN_E_UNSUPPORTED`，避免静默丢失背景和立绘状态。

### `VNRuntimeSessionSnapshotV2`

真实内容使用的版本化快照结构。它嵌入完整 v1 字段，并追加：

1. `scene_name` 与 `content_mode`
2. VM 当前/上一背景、过渡时长与 serial
3. 8 个持久 sprite layer 的资源、坐标和 active 状态
4. background player 的过渡进度

调用者必须先执行 `vn_runtime_session_snapshot_v2_init(...)`。恢复时会重新打开 pack、按场景目录核对 name/ID、重建 renderer，然后恢复视觉状态；framebuffer 和各类 perf cache 仍不会序列化。

### `VNRuntimeFrameView`

版本化的只读 framebuffer 借用结构。调用者必须先执行 `vn_runtime_frame_view_init(...)`，由 `struct_size/version` 协商当前布局；未初始化或版本不匹配会返回 `VN_E_INVALID_ARG`。

当前字段：

1. `struct_size/version` 当前使用 `sizeof(VNRuntimeFrameView)` 与 `VN_RUNTIME_FRAME_VIEW_VERSION`
2. `pixels` 是逻辑 `ARGB8888` 的 `vn_u32` 数组
3. `width/height/stride_pixels/pixel_count` 描述当前实际渲染尺寸
4. `pixel_format` 当前固定为 `VN_RUNTIME_PIXEL_FORMAT_ARGB8888_U32`
5. 指针只在下一次 `step`、renderer 重配或 session destroy 前有效，调用者需要长期保留时必须自行复制

### `VNRunConfig`

运行输入配置。

关键字段：

1. `pack_path`
   - 资源包路径（默认 `assets/demo/demo.vnpak`）
   - create 时会复制到 session 自有存储，调用者无需让输入字符串持续存活
2. `scene_name`
   - 带 `VNSC` catalog 的内容包接受 catalog 中声明的任意合法场景名
   - 保持初始化默认值 `S0` 且 catalog 未声明 `S0` 时，自动使用 catalog 的 `entry_scene`
   - 没有 catalog 的旧包继续只接受 `S0` / `S1` / `S2` / `S3` / `S10`
   - 未知名称会被显式拒绝
3. `backend_name`
   - `"auto"` / `"scalar"` / `"avx2"` / `"avx2_asm"` / `"neon"` / `"rvv"`
   - 其中 `avx2_asm` 当前是实验性 force-only 变体
4. `width`, `height`
   - 输出分辨率，默认 `600x800`
5. `frames`, `dt_ms`
   - 运行帧数与每帧步长
6. `choice_index`
   - 默认分支选择（`0` 表示第 1 个选项）
7. `choice_seq[]`, `choice_seq_count`
   - 分支选择序列，按 `CHOICE` 发生顺序消费
8. `trace`
   - 非 0 打印逐帧状态与性能采样（`frame_ms/vm_ms/build_ms/raster_ms/audio_ms/rss_mb`），并在 perf 扩展开启时追加 `dirty_*` / `render_width` / `render_height` / `dynres_*` 字段
9. `keyboard`
   - 非 0 启用键盘输入（Linux TTY / Windows console 调试模式）
10. `emit_logs`
   - 非 0 输出日志，0 时静默运行
11. `hold_on_end`
   - 非 0 时脚本到达 `END` 后继续维持帧循环直到 `frames` 用尽（主要用于 perf 采样）
12. `perf_flags`
   - 运行时性能特性开关位图
   - 默认值为 `VN_RUNTIME_PERF_DEFAULT_FLAGS`
   - 当前已公开：`VN_RUNTIME_PERF_FRAME_REUSE`（静态帧短路 / frame reuse）
   - 当状态签名稳定且无 active fade / 新 SE 时，直接复用上一帧 framebuffer，跳过本帧 `build_render_ops + renderer_submit`
   - 当前实现会折叠 `frame_index` 派生的前端占位动画，因此命中后会冻结这类占位动画，优先换取稳定帧的 CPU 收益
   - 当前已公开：`VN_RUNTIME_PERF_OP_CACHE`（Frontend `VNRenderOp[]` LRU 命令缓存）
   - 命中时跳过命令构建，但仍会按当前帧回写 `SPRITE/FADE` 动态字段，避免命令缓存路径因占位动画长期 0 hit
   - 当前已公开：`VN_RUNTIME_PERF_DIRTY_TILE`（Dirty-Tile 规划/提交）
   - 当前会在 `frame reuse miss` 且拿到最终 `VNRenderOp[]` 后生成 dirty plan，回传 tile/rect/full-redraw 统计；当 plan 可局部提交时，Runtime 会优先走 `renderer_submit_dirty(...)`。当前 `scalar/avx2/neon/rvv` 已实现 dirty submit，默认 `off`。
   - 当前已公开：`VN_RUNTIME_PERF_DYNAMIC_RESOLUTION`（动态分辨率自动升降档）
   - 当前按 `R0/R1/R2 = 100%/75%/50%` 三档工作；当最近 120 帧 p95 超过 `16.67ms` 时尝试降档，当最近 300 帧 p95 低于 `13.0ms` 时尝试升档。切档时 runtime 会重配 renderer 尺寸、失效 frame reuse/op cache/dirty planner 相关缓存，并通过 `VNRunResult` 回传实际渲染尺寸与切档统计；默认 `off`。

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
10. `frame_reuse_hits`, `frame_reuse_misses`
11. `op_cache_hits`, `op_cache_misses`
12. `dirty_tile_count`, `dirty_rect_count`, `dirty_full_redraw`
13. `dirty_tile_frames`, `dirty_tile_total`, `dirty_rect_total`, `dirty_full_redraws`
14. `render_width`, `render_height`
15. `dynamic_resolution_tier`, `dynamic_resolution_switches`

## 4. API 函数

### `void vn_run_config_init(VNRunConfig* cfg)`

初始化默认配置，建议总是先调用。

### `void vn_runtime_query_build_info(VNRuntimeBuildInfo* out_info)`

填充当前 build 的公开协商信息。

当前保证：

1. `runtime_api_version` 当前为 `v1`
2. `runtime_api_stability` 当前为 `public stable v1`
3. `preview_protocol_version` 当前为 `v1`
4. `vnpak` 当前公开读范围为 `v1..v2`，默认写 `v2`
5. `vnsave_latest_version` 当前为 `VNSAVE_VERSION_1`
6. `vnsave_api_stability` 当前为 `format v1 stable; generic ABI not public`
7. `vnsave_public_saveload_scope` 当前为 `runtime-session-only`
8. `host_os/host_arch/host_compiler` 来自当前 build 平台探测结果

### `void vn_runtime_build_info_v2_init(VNRuntimeBuildInfoV2* out_info)`

把版本化 build info 清零，并写入当前 `struct_size/version`。空指针无操作。

### `int vn_runtime_query_build_info_v2(VNRuntimeBuildInfoV2* out_info)`

查询 `1.1.0` 引擎版本和兼容追加能力位。未初始化、结构过小或版本不匹配返回 `VN_E_INVALID_ARG`；原有 `vn_runtime_query_build_info(...)` 行为不变。

### `int vn_runtime_session_capture_snapshot(const VNRuntimeSession* session, VNRuntimeSessionSnapshot* out_snapshot)`

捕获当前 live session 的可恢复快照。

当前约束：

1. 空指针返回 `VN_E_INVALID_ARG`
2. 已结束 session 返回 `VN_E_UNSUPPORTED`
3. 若当前 session 还带有待消费 injected input，也返回 `VN_E_UNSUPPORTED`
4. 若路径/状态超出当前 snapshot 能力范围，则返回格式或容量错误

当前用途：

1. 宿主做内存级 quick-save / quick-load
2. 作为正式 `runtime-session-only` persistence API 的状态捕获层

真实内容 session 必须改用 v2 capture；旧入口会返回 `VN_E_UNSUPPORTED`。

### `void vn_runtime_session_snapshot_v2_init(VNRuntimeSessionSnapshotV2* snapshot)`

初始化 v2 snapshot 的 `struct_size/version`。每次 capture 前都应调用。

### `int vn_runtime_session_capture_snapshot_v2(const VNRuntimeSession* session, VNRuntimeSessionSnapshotV2* out_snapshot)`

捕获 legacy 或真实内容 session。除 v1 状态外，还保留场景名称、背景交叉淡化进度和 8 个 sprite layer；未初始化的输出结构返回 `VN_E_INVALID_ARG`。

### `int vn_runtime_session_create_from_snapshot(const VNRuntimeSessionSnapshot* snapshot, VNRuntimeSession** out_session)`

从 snapshot 直接重新创建一个 session。

当前行为：

1. 会重新打开 `pack`、重载脚本、初始化 VM/renderer
2. 然后恢复 VM、fade、choice feed、frame 进度与 dynres tier
3. dirty planner、frame reuse、op cache 会按恢复后的尺寸和空缓存重新初始化

当前限制：

1. snapshot 必须包含可解析的 `scene_id`
2. `pack_path/backend_name` 必须是当前支持的公开字符串
3. `vm_flags` 中的 `ENDED/ERROR` 返回 `VN_E_UNSUPPORTED`，未知位返回 `VN_E_INVALID_ARG`
4. `vm_pc_offset` 与有效 call-stack return offset 必须落在脚本指令边界，否则返回 `VN_E_FORMAT`
5. 当前是最小正式恢复 API，不承诺保留所有 perf 累计统计

### `int vn_runtime_session_create_from_snapshot_v2(const VNRuntimeSessionSnapshotV2* snapshot, VNRuntimeSession** out_session)`

从 v2 snapshot 恢复 legacy 或 catalog 内容场景。恢复会核对 snapshot 的 `scene_name/scene_id/content_mode` 与 pack，拒绝损坏或指向错误资源的视觉状态，并沿用 v1 对终态及未知 `vm_flags` 的拒绝规则。恢复后的 framebuffer 尚未生成，需先成功推进一帧。

### `int vn_runtime_session_save_to_file(const VNRuntimeSession* session, const char* path, vn_u32 slot_id, vn_u32 timestamp_s)`

把当前 live session 写成一个 `vnsave v1` 文件。

当前行为：

1. 顶层继续复用 `vnsave v1` 头
2. payload 当前默认写入 runtime snapshot payload v2
3. `slot_id`、`scene_id`、`script_pc`、`timestamp_s` 会同时写入 `vnsave` 头
4. payload CRC32 按现有 `vnsave v1` 规则写回 header

当前限制：

1. 只支持保存 live session
2. 若 session 带有待消费 injected input，返回 `VN_E_UNSUPPORTED`
3. VM 已执行 `END` 或进入 error 时返回 `VN_E_UNSUPPORTED`
4. 当前这是正式公开的 runtime session persistence API，但不等于长期通用宿主 save/load ABI

### `int vn_runtime_session_load_from_file(const char* path, VNRuntimeSession** out_session)`

从 `vn_runtime_session_save_to_file(...)` 生成的文件恢复一个 session。

当前行为：

1. 先复用 `vnsave_probe_file(...)` 验证外层 `vnsave v1` 头与 CRC
2. 再解析 runtime snapshot payload
3. 最后通过 `vn_runtime_session_create_from_snapshot(...)` 恢复 session

当前限制：

1. 同时读取历史 runtime snapshot payload v1 与当前 payload v2
2. payload 表示 VM 终态时返回 `VN_E_UNSUPPORTED`；`vm_flags` 含未知位时返回 `VN_E_INVALID_ARG`
2. 对普通 `vnsave v1` 但 payload 不是 runtime snapshot 的文件，返回格式或不支持错误
3. 这层正式承诺只覆盖 `runtime-session-only`

### `int vn_runtime_run(const VNRunConfig* cfg, VNRunResult* out_result)`

结构化运行入口。

行为补充：

1. 常规模式下不承诺真实时间节流。
2. 当 `cfg->keyboard != 0` 且 `cfg->dt_ms > 0` 时，`vn_runtime_run()` 会在帧与帧之间调用平台层 sleep，以便 CLI 调试模式可交互。
3. `backend_name`、perf 扩展开关和结果统计字段属于当前公开面的一部分；后续仅允许兼容追加。

返回值：

1. `0`：成功
2. 非 0：失败（参数错误、资源加载失败、渲染初始化失败、VM 错误等）

### `int vn_runtime_session_create(const VNRunConfig* cfg, VNRuntimeSession** out_session)`

创建运行时会话并完成初始化（加载 pack、加载脚本、初始化 VM、初始化渲染后端）。

当前 renderer/backend 和纹理 provider 使用进程级状态，因此一次只支持一个 live session；创建第二个 session 前应先销毁第一个。

### `int vn_runtime_session_step(VNRuntimeSession* session, VNRunResult* out_result)`

推进一帧执行并返回最新状态快照。

行为要点：

1. 每次调用最多推进 1 帧。
2. 支持 `choice_seq`、`vn_runtime_session_set_choice` 与 `vn_runtime_session_inject_input` 的输入注入。
3. `vn_runtime_session_inject_input` 注入的事件会在下一次 `step` 时消费。
4. 当运行结束且 `vm_error != 0` 时返回非 0。
5. 若启用 `VN_RUNTIME_PERF_FRAME_REUSE`，则会在状态签名稳定时直接复用上一帧 framebuffer，并在 `VNRunResult` 中回传 `frame_reuse_hits/misses`。
6. 若启用 `VN_RUNTIME_PERF_OP_CACHE`，则会对 `VNRenderOp[]` 构建结果做 LRU 缓存，并在 `VNRunResult` 中回传命中统计。
7. 若启用 `VN_RUNTIME_PERF_DIRTY_TILE`，则会在当前帧最终 `VNRenderOp[]` 与上一帧已提交 op 之间构建 dirty plan，并在 `VNRunResult` 中回传当前帧与累计统计；当后端支持时，runtime 会优先走 `renderer_submit_dirty(...)`，否则自动回退整帧提交。
8. 若启用 `VN_RUNTIME_PERF_DYNAMIC_RESOLUTION`，则会按滑动窗口 p95 自动在 `R0/R1/R2` 之间升降档；切档后会失效依赖旧尺寸的缓存，并在 `VNRunResult` 中回传 `render_width/render_height/dynamic_resolution_tier/dynamic_resolution_switches`。

### `void vn_runtime_frame_view_init(VNRuntimeFrameView* out_view)`

把 frame view 清零并写入当前 `struct_size/version`。每个 view 在第一次查询前必须初始化；后续查询会保留一个可继续复用的已初始化结构。

### `int vn_runtime_session_get_frame_view(const VNRuntimeSession* session, VNRuntimeFrameView* out_view)`

取得当前已提交画面的只读 view：

1. 调用前先执行 `vn_runtime_frame_view_init(&view)`；空指针、未初始化结构或版本不匹配返回 `VN_E_INVALID_ARG`
2. 新建/刚恢复的 session 尚无有效帧，返回 `VN_E_RENDER_STATE`
3. 动态分辨率刚重建 renderer 时，旧 view 立即失效；下一次成功 step 后可再次获取
4. 返回的像素内存由 runtime 持有，不得释放或写入

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

当前 machine-readable 约定：

1. 参数错误会输出稳定的 `trace_id=runtime.cli.*`
2. 运行失败会输出 `trace_id=runtime.run.failed`
3. 成功 summary 继续以 `vn_runtime ok ` 开头，并额外附带 `trace_id=runtime.run.ok`
4. 详细错误解释继续复用 `error_code + error_name + message`

扩展参数：

1. `--load-save=<path>`
   - 直接从 `vn_runtime_session_save_to_file(...)` 生成的文件恢复 session
   - 当前只允许与 `--trace` / `--quiet` 这类输出控制参数组合
   - 若与 `--scene` / `--pack` / `--backend` / `--frames` / `--dt-ms` / `--choice-*` / perf 开关等运行配置混用，返回结构化参数错误
2. `--save-out=<path>`
   - 在本次 CLI run 结束后，把当前 session 状态写成 runtime session save/load 文件
   - 可用于 normal run，也可与 `--load-save` 组合，形成“读一个 save，继续推进，再写回一个 save”
   - 当前默认写 `slot_id=0` 与 `timestamp_s=0`
3. `--save-slot=<n>`
   - 与 `--save-out` 配合，显式写入 `vnsave` header 的 `slot_id`
   - 若未提供 `--save-out`，返回结构化参数错误
4. `--save-timestamp=<n>`
   - 与 `--save-out` 配合，显式写入 `vnsave` header 的 `timestamp_s`
   - 若未提供 `--save-out`，返回结构化参数错误
5. `--hold-end`
   - 对应 `VNRunConfig.hold_on_end=1`
   - 用于场景脚本提前结束时仍持续输出帧采样数据
6. `--perf-frame-reuse=<on|off>`
   - 切换 `VN_RUNTIME_PERF_FRAME_REUSE`
   - 默认 `on`（来自 `VN_RUNTIME_PERF_DEFAULT_FLAGS`）
7. `--perf-op-cache=<on|off>`
   - 切换 `VN_RUNTIME_PERF_OP_CACHE`
   - 默认 `on`（来自 `VN_RUNTIME_PERF_DEFAULT_FLAGS`）
8. `--perf-dirty-tile=<on|off>`
   - 切换 `VN_RUNTIME_PERF_DIRTY_TILE`
   - 默认 `off`（开启后会尝试 dirty submit；当前 `scalar` / `avx2` / `neon` / `rvv` 都已实现 partial submit）
9. `--perf-dynamic-resolution=<on|off>`
   - 切换 `VN_RUNTIME_PERF_DYNAMIC_RESOLUTION`
   - 默认 `off`（开启后允许 runtime 在 `R0/R1/R2` 之间自动升降档；建议配合 `--hold-end` 做长窗口 perf 采样）

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
8. `frame_reuse_hit`, `frame_reuse_hits`, `frame_reuse_misses`（附加诊断字段）
9. `op_cache_hit`, `op_cache_hits`, `op_cache_misses`（附加诊断字段）
10. `dirty_tiles`, `dirty_rects`, `dirty_full_redraw`, `dirty_tile_frames`, `dirty_tile_total`, `dirty_rect_total`, `dirty_full_redraws`（Dirty-Tile 规划/提交统计字段）

`tests/perf/run_perf.sh` 基于这些字段生成 `perf_<scene>.csv` 与 `perf_summary.csv`。

宿主集成的更完整说明见 [`../host-sdk.md`](../host-sdk.md)。

## 9. 当前已知约束

1. 运行时会话当前是单实例全局渲染后端模型，不支持并发多会话。
2. Windows 平台的 `keyboard` 调试模式已接入 `_kbhit/_getch`；非 console 环境仍不应把它视为宿主输入接口。
3. `VN_INPUT_KIND_KEY` 当前只保证 `1-9`、`t/T`、`q/Q` 的运行时语义。
4. `vn_runtime_run_cli` 保留进程级退出码语义（参数错误返回 `2`，运行失败返回 `1`）。
