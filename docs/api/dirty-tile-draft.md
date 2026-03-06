# Dirty-Tile 增量渲染设计与 API 草案

- 状态：`draft`（未实现，供 `ISSUE-008` 开工使用）
- 目标：把白皮书里的 `Dirty-Tile` 目标，落成可直接拆 PR 的运行时 / 前端 / 后端接口方案
- 约束：保持 `C89`；继续坚持“前后端一份 API，跨架构只重写后端”

## 1. 当前主线基线

截至当前主线，渲染路径已经是：

1. `src/core/runtime_cli.c`
   - `runtime_prepare_frame_reuse()` 先做整帧复用判断
   - 未命中时进入 `runtime_build_render_ops_cached()`
   - 命令列表命中后仍会通过 `runtime_render_patch_cached_ops()` 回写当前帧的动态字段
   - 最后统一走 `renderer_submit()`
2. `src/frontend/render_ops.c`
   - 当前 Frontend 只生成 `CLEAR / SPRITE / TEXT / FADE` 四类 `VNRenderOp`
3. `src/core/renderer.c`
   - 当前 Renderer 只暴露“整帧提交”接口，没有脏区/裁剪提交能力

也就是说，`Dirty-Tile` 的正确挂点不是 `frame reuse` 之前，也不是 `op cache key` 这一层，而是：

1. `frame reuse miss` 之后
2. `op cache hit/miss` 之后
3. `runtime_render_patch_cached_ops()` 已经把 `SPRITE.x` / `FADE.alpha` 这类动态字段补齐之后

否则脏区分析会拿到“并非最终显示结果”的中间态，造成误判。

## 2. 设计目标

第一版 `Dirty-Tile` 只解决“局部区域变化时避免整帧光栅”的问题，不在这一轮顺手解决：

1. Tile 级遮挡剔除
2. 更复杂的 z-order / overlap 裁剪最优解
3. 动态分辨率调度
4. 跨多张 render target 的增量提交

第一版必须满足：

1. 对当前四类 `VNRenderOp` 可正确工作
2. 与 `frame reuse -> op cache -> full render` 这条已落地主线兼容
3. 单个后端是否支持脏区提交，必须可独立探测和回退
4. 在任意可疑条件下优先回退整帧路径，不冒险输出错误图像

## 3. 当前代码下的最佳接入点

### 3.1 Runtime 层

最佳入口位于 `src/core/runtime_cli.c` 的 `vn_runtime_session_step()`：

1. `runtime_prepare_frame_reuse()` 命中：直接复用 framebuffer，跳过 `Dirty-Tile`
2. `runtime_build_render_ops_cached()` 返回后：此时已经拿到当前帧 `VNRenderOp[]`
3. `runtime_render_patch_cached_ops()` 已完成：此时 `session->ops` 才是可用于脏区分析的最终 IR
4. 在调用 `renderer_submit()` 之前插入 `runtime_prepare_dirty_tiles()`
5. 根据计划结果分流到：
   - `renderer_submit()`（整帧路径）
   - `renderer_submit_dirty()`（脏区路径）

### 3.2 Frontend / Shared 逻辑层

脏区分析本质上属于“前端生成出的 Render IR 之间的差分”，不应下沉到 ISA 后端。

建议新增：

1. `src/frontend/dirty_tiles.c`
2. `src/frontend/dirty_tiles.h`

职责：

1. 计算 `VNRenderOp` 的屏幕包围盒
2. 对比“上一帧已提交 op”与“当前帧最终 op”
3. 生成 tile bitset 与合并后的 dirty rect 列表
4. 在高风险条件下直接给出 `full_redraw=1`

`vn_frontend.h` 第一版不建议新增公开函数。脏区分析先保持为引擎内部实现，避免过早冻结外部 ABI。

### 3.3 Renderer / Backend 层

当前 `renderer_submit()` 和 `VNRenderBackend.submit_ops` 只有“整帧提交”能力，不足以支撑脏区路径。

第一版建议：

1. 保留现有 `renderer_submit()` 作为整帧快路径
2. 新增并行接口 `renderer_submit_dirty()`
3. 在 `VNRenderBackend` 上新增可选的 `submit_ops_dirty` 回调
4. 若当前后端未实现 `submit_ops_dirty`，Runtime 必须自动退回整帧提交

这条边界满足“前后端只有一份 API”：

1. Frontend / Runtime 负责产出 `VNRenderOp[] + VNDirtyPlan`
2. 各 ISA 后端只负责实现同一份 `submit_ops_dirty` 契约
3. 新架构只需补后端，不改前端逻辑

## 4. 第一版执行顺序

建议把 `Dirty-Tile` 放在主线中的顺序固定为：

```text
vm_step
  -> state_from_vm
  -> frame reuse check
      -> hit: skip build + raster
      -> miss:
           build or replay op cache
           patch dynamic op fields
           build dirty plan against last committed frame
             -> full redraw: renderer_submit()
             -> partial redraw: renderer_submit_dirty()
           commit dirty history
           commit frame reuse key
```

关键原则：

1. `Dirty-Tile` 不参与 `frame reuse` 的命中判断
2. `Dirty-Tile` 只分析“当前最终 op 列表”
3. 只有在本帧真正成功提交后，才更新 dirty history
4. 分辨率切换、后端切换、初始化首帧都必须清空 dirty history

## 5. 第一版数据模型

## 5.1 内部结构（建议）

这些结构第一版建议只放内部头文件，不进公开 API：

```c
#define VN_DIRTY_TILE_SIZE 8u
#define VN_DIRTY_RECT_MAX 128u

typedef struct {
    vn_u16 x;
    vn_u16 y;
    vn_u16 w;
    vn_u16 h;
} VNDirtyRect;

typedef struct {
    vn_u32 valid;
    vn_u16 tile_w;
    vn_u16 tile_h;
    vn_u16 tiles_x;
    vn_u16 tiles_y;
    vn_u32 bit_words;
    vn_u32 dirty_tile_count;
    vn_u32 dirty_rect_count;
    vn_u32 full_redraw;
    VNDirtyRect rects[VN_DIRTY_RECT_MAX];
} VNDirtyPlan;

typedef struct {
    vn_u32 valid;
    vn_u16 width;
    vn_u16 height;
    vn_u16 tiles_x;
    vn_u16 tiles_y;
    vn_u32 bit_words;
    vn_u32* dirty_bits;
    VNRenderOp prev_ops[16];
    VNDirtyRect prev_bounds[16];
    vn_u32 prev_op_count;
} DirtyTileState;
```

说明：

1. Tile 大小先固定 `8x8`，与白皮书一致，不先开放为外部可调参数
2. `dirty_bits` 在 session create 时按当前分辨率一次性分配
3. `prev_ops + prev_bounds` 保存“上一帧真正提交到 framebuffer 的最终结果”
4. 当前分辨率是 `600x800` 时，tile 总数为 `75 x 100 = 7500`，bitset 约 `940B`，成本可接受

## 5.2 公开 API 草案

### `include/vn_runtime.h`

建议新增：

```c
#define VN_RUNTIME_PERF_DIRTY_TILE (1u << 2)
```

当实现落地后，建议补充到 `VNRunResult`：

```c
vn_u32 dirty_tile_frames;
vn_u32 dirty_tile_total;
vn_u32 dirty_rect_total;
vn_u32 dirty_full_redraws;
vn_u32 dirty_backend_fallbacks;
```

建议新增 CLI 开关：

```text
--perf-dirty-tile=<on|off>
```

说明：

1. 第一版不建议把 tile size、rect cap、threshold 百分比暴露到公开 runtime 配置里
2. 这些参数先做成内部常量，等 perf 数据稳定后再决定是否升级为 API

### `include/vn_backend.h`

建议扩展后端能力位：

```c
typedef struct {
    vn_u32 has_simd;
    vn_u32 has_lut_blend;
    vn_u32 has_tmem_cache;
    vn_u32 has_dirty_submit;
} VNBackendCaps;
```

建议新增共享提交描述：

```c
typedef struct {
    const VNDirtyRect* rects;
    vn_u32 rect_count;
    vn_u32 full_redraw;
} VNRenderDirtySubmit;
```

建议在 `VNRenderBackend` 上新增可选回调：

```c
int (*submit_ops_dirty)(const VNRenderOp* ops,
                        vn_u32 op_count,
                        const VNRenderDirtySubmit* dirty_submit);
```

说明：

1. 旧的 `submit_ops` 保留不动，继续承载整帧路径
2. 新后端实现只需遵守这一份 `submit_ops_dirty` 契约
3. 若某后端未实现该回调，Runtime 自动回退整帧路径

### `include/vn_renderer.h`

建议新增并行入口：

```c
int renderer_submit_dirty(const VNRenderOp* ops,
                          vn_u32 op_count,
                          const VNRenderDirtySubmit* dirty_submit);
```

这里不建议直接改 `renderer_submit()` 的现有签名，避免把“已可用整帧路径”的接口也变成一轮大范围重构。

## 6. 脏区判定规则

第一版建议保守，不做激进判断。

### 6.1 直接整帧回退条件

满足任一条件，直接 `full_redraw=1`：

1. 首帧或 dirty history 无效
2. 分辨率发生变化
3. 当前后端不支持 dirty submit
4. 当前帧或上一帧存在 `FADE` 有效覆盖
5. `CLEAR` 的颜色/语义发生变化
6. 合并后 dirty tile 比例超过 `60%`
7. 合并后 dirty rect 数超过 `VN_DIRTY_RECT_MAX`
8. 任一 op 的包围盒超出可安全裁剪的规则集

### 6.2 可做局部刷新的条件

仅当下面条件都成立时才走 partial path：

1. 当前帧与上一帧的可见根层仍可由 `CLEAR` 重新建立
2. 变化只来自 `SPRITE/TEXT` 的位置、尺寸、纹理、alpha、flags 等局部区域
3. 所有 dirty rect 经过 tile 合并后互不重叠
4. 当前后端实现了裁剪提交

### 6.3 `VNRenderOp` 包围盒规则

第一版可直接按当前四类 op 定义：

1. `CLEAR`
   - 不直接加入 dirty rect 列表
   - 若参数变化，则直接整帧回退
2. `SPRITE`
   - 使用 `x/y/w/h` 计算矩形，并裁剪到 framebuffer 范围
   - 若位置变化，旧矩形与新矩形都必须标脏
3. `TEXT`
   - 同 `SPRITE`
4. `FADE`
   - 视为全屏效果
   - 第一版一律强制整帧回退

## 7. 后端执行契约

所有后端的 `submit_ops_dirty` 必须遵循同一套规则：

1. `dirty_submit->rects` 中每个 rect 都是最终不重叠矩形
2. 后端对每个 dirty rect 的处理顺序必须等价于：
   - 先在该 rect 内执行 `CLEAR`
   - 再按原始顺序重放与该 rect 相交的 `SPRITE/TEXT/FADE`
3. rect 外像素必须保持上一帧内容不变
4. 任一 dirty rect 提交失败，调用方应整帧回退并记录一次 fallback

这意味着第一版后端工作量主要集中在“为现有 clear / textured rect / fade 路径补 clip rect 参数”，不需要重写 Frontend。

## 8. 建议的落地切片

### Slice A: Shared Dirty Plan

1. 新增 `src/frontend/dirty_tiles.c/.h`
2. 为当前四类 op 计算 bounds
3. 生成 bitset、merge rect、实现 `>60%` 回退
4. 新增单元测试，先不接 Runtime

### Slice B: Scalar 参考实现

1. `vn_backend.h` / `vn_renderer.h` 新增 dirty submit 契约
2. `scalar` 后端先支持 `submit_ops_dirty`
3. Runtime 接上 `VN_RUNTIME_PERF_DIRTY_TILE`
4. 让 `scalar` 作为功能正确性的参考实现

### Slice C: Runtime / Preview / Trace

1. `vn_runtime.h` 新增 dirty-tile perf flag 与 counters
2. CLI 新增 `--perf-dirty-tile=<on|off>`
3. `trace` 新增 `dirty_tiles / dirty_rects / dirty_full_redraw`
4. `preview protocol` / preview JSON 补充对应统计字段

### Slice D: SIMD 后端对齐

1. `avx2` 实现 clip-aware clear / sprite / text path
2. `neon` 跟进同一契约
3. `rvv` 最后接入；无原生设备阶段先走 `qemu-first` 功能正确性

### Slice E: Perf / CI 固化

1. `run_perf.sh` 默认继续测 shipped path
2. 新增 `--perf-dirty-tile=off` 归因说明
3. 增加 dirty-tile on/off 对照报告
4. 再决定是否把 dirty-tile 相关门限接成阻塞 gate

## 9. 建议测试矩阵

### 功能正确性

1. `test_render_ops` 或新增 `test_dirty_tiles`
   - 静态文本：应产生 `0` dirty rect（由 frame reuse 先短路）
   - sprite 平移：应只标旧/新矩形覆盖的 tile
   - text id 变化：应只标文本框矩形
   - fade 激活：应直接整帧回退
2. `test_runtime_api`
   - dirty-tile only
   - dirty-tile + op-cache
   - dirty-tile + frame-reuse
   - all perf disabled
3. `test_runtime_golden`
   - 同一 scene 下比较 dirty-tile on/off 输出一致

### 后端一致性

1. `scalar` 先作为 dirty submit 参考
2. `avx2/neon/rvv` 逐个对照 `scalar`
3. 仍沿用当前 golden 容差策略，不另起一套判定口径

### 性能归因

建议最终形成三组对照：

1. 默认：`frame reuse + op cache + dirty tile`
2. 关闭 dirty tile：`frame reuse + op cache`
3. 全关：纯整帧路径

## 10. 与动态分辨率的关系

`Dirty-Tile` 与动态分辨率必须解耦：

1. dirty planner 永远基于“当前实际 framebuffer 尺寸”工作
2. 分辨率切换时必须清空 dirty history，并强制下一帧整帧重绘
3. 不允许把 tile bitset 复用到不同分辨率档位
4. 动态分辨率可以后做，不阻塞 Dirty-Tile 第一版落地

## 11. 当前建议

当前最合理的开工顺序是：

1. 先做 `Slice A`：dirty plan/bounds/merge 的共享逻辑
2. 立刻接 `Slice B`：先让 `scalar` 跑通 partial submit
3. Runtime trace / preview counters 与 `issue.md` 同步更新
4. 再把同一份 dirty submit API 下放到 `avx2 -> neon -> rvv`

这样可以保证：

1. 先拿到正确性基线
2. 再把优化复制到各架构后端
3. 不会在没有共享契约的前提下，让不同后端各写一套脏区逻辑
