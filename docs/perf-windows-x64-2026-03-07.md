# Windows x64 AVX2 Perf Investigation (2026-03-07)

## Context

本记录最初用于追踪 GitHub `windows-x64` runner 上 `scalar -> avx2` 未转正的问题；到 `2026-03-07` 晚些时候，随着多轮修正落地，它也同时记录了该问题在 GitHub 原生 runner 上的恢复证据与新的后续热点。

时间线分两段：

1. 历史负样本：GitHub Actions `ci-matrix` push run `22793662179`（head `eb83ea1`）。
2. 最新恢复样本：GitHub Actions `ci-matrix` push run `22795078202`（head `d6081b4`）。

## Historical Negative Evidence

来自 run `22793662179` 的 `scalar -> avx2` smoke compare：

| scene | scalar p95 ms | avx2 p95 ms | p95 gain |
|---|---:|---:|---:|
| S1 | 2.525 | 2.668 | -5.66% |
| S3 | 2.726 | 2.735 | -0.33% |

当时的结论是：`windows-x64` 的问题不是阈值配置，而是 runner 上真实存在 `avx2` 轻微回退。

## Root Cause

当时最可信的解释不是“AVX2 算法错误”，而是“AVX2 覆盖面太窄，Windows/MSVC 正好把结构性成本放大了”。

1. `avx2` 后端里真正显式使用 YMM intrinsic 的热点最初主要集中在 `fill_u32`，见 [`src/backend/avx2/avx2_backend.c`](../src/backend/avx2/avx2_backend.c)。
2. `SPRITE/TEXT` 的像素主循环虽然已经在 AVX2 TU 内，但仍是逐像素 `sample/hash -> combine -> blend/store`。
3. `S1/S3` 每帧固定包含 `CLEAR + SPRITE + TEXT + FADE` 四个 op，见 [`src/frontend/render_ops.c`](../src/frontend/render_ops.c)。
4. 修改前，整屏 `FADE` 与 textured 路径都还保留了明显的标量热点，因此 Windows runner 上容易只看到“额外壳层成本”，看不到足够大的 SIMD 覆盖面。

## In-Tree Mitigation Timeline

截至当前工作区，已依次补上四轮修正：

1. `vn_avx2_fill_u32(...)` 改成“对齐前缀 + aligned store”，减少 Windows x64 上的保守 `storeu` 成本。
2. 新增 `uniform alpha/fade` AVX2 row kernel，把整屏 `FADE` 从逐像素标量 `blend_rgb` 拉进显式 AVX2。
3. 把 `SPRITE/TEXT` 的 `sample/hash -> combine -> blend` 热循环收回 AVX2 TU，避免每像素跨 TU 调公共 `pixel_pipeline.c`。
4. 最新一轮又在 textured full-span 路径上补了 row palette + repeated-`v8` reuse：当 `vis_w > 256` 时，按 row 构建 256 项 texel palette，并在连续重复的 `v8` 行上直接复用，避免 full-span sprite/text 反复重算同一批 `sample/hash/combine`。

第 4 条的动机直接来自 Compare D：既然 `u_lut` 本身已经是 `0..255` 索引，那么 full-span 场景下最大的浪费不是“blend/store 不够快”，而是同一 row / 同一 `v8` 上重复做了太多次完全相同的 texel 生成。

## Latest GitHub Recovery Evidence

最新 run `22795078202`（head `d6081b4`）里的 `perf-windows-x64` artifact 已经显示整机 smoke 明显转正：

| scene | scalar p95 ms | avx2 p95 ms | p95 gain |
|---|---:|---:|---:|
| S1 | 2.551 | 0.417 | +83.65% |
| S3 | 2.607 | 0.482 | +81.51% |

这意味着：

1. `windows-x64` 已经不再处于“AVX2 轻微回退”的旧状态。
2. 现有 `windows-x64-scalar-avx2-smoke` profile 之所以仍暂时保留 regression-envelope gate，只是因为我们还没有累积到足够多的连续正样本，不是因为当前 run 仍为负收益。

同一个 run 的 `Compare D = kernel scalar -> avx2` 也给出了新的热点排序：

| kernel | scalar avg ms | avx2 avg ms | 结论 |
|---|---:|---:|---|
| `clear_full` | 0.037621 | 0.039546 | 绝对值极小，Windows runner 上可视为计时噪声 |
| `fade_full_alpha160` | 2.164904 | 0.133900 | `fade` 已被显著压平 |
| `sprite_scene_opaque` | 0.181575 | 0.122858 | scene-sized textured 已正向 |
| `text_scene_opaque` | 0.138321 | 0.093425 | scene-sized text 已正向 |
| `sprite_full_opaque` | 5.100917 | 3.172533 | 仍是最大的剩余绝对热点之一 |
| `sprite_full_alpha180` | 7.335675 | 5.231879 | 仍是最大的剩余绝对热点之一 |

这里最关键的结论不是“AVX2 还不够快”，而是“在已经转正之后，继续优化应优先盯住 `sprite_full_*`，而不是回头再碰 `clear/fade`”。

## Local Follow-Up After Row Palette Reuse

在当前工作区里，继续针对 textured full-span 路径加上 row palette + repeated-`v8` reuse 之后，本地 `linux-x64` follow-up 样本（`24 iterations / 6 warmup / 600x800`）变成：

| kernel | previous local avx2 avg ms | current local avx2 avg ms |
|---|---:|---:|
| `sprite_full_opaque` | 19.173 | 4.728 |
| `sprite_full_alpha180` | 25.215 | 9.093 |

同一轮本地整机 smoke compare 为：

| scene | scalar p95 ms | avx2 p95 ms | p95 gain |
|---|---:|---:|---:|
| S1 | 8.837 | 6.541 | +25.98% |
| S3 | 9.098 | 5.333 | +41.38% |

这些本地数据只能说明方向正确，不能替代 GitHub `windows-x64` runner；但它们已经足够支持当前判断：重复 texel 生成确实是 full-span textured 的主要浪费面。

## Current Reading

截至 `2026-03-07`，`windows-x64` 应这样解读：

1. 历史上的“AVX2 未转正”问题已经被修回，最新 GitHub run `22795078202` 明确是正收益。
2. 当前最大的剩余绝对热点已经收敛到 `sprite_full_opaque` / `sprite_full_alpha180`，也就是 textured full-span 路径。
3. `clear/fade` 不再值得作为下一轮主要目标。
4. row palette + repeated-`v8` reuse 是一条高收益、低语义风险的继续压缩路径，并且已经在本地样本中证明有效。

## Next Experiments

1. 等待当前这轮 row-palette 改动对应的 GitHub `perf-windows-x64` artifact，确认 `Compare D` 上 `sprite_full_*` 是否继续明显下降。
2. 如果新的 artifact 里 `sprite_full_alpha180` 仍明显高于 `sprite_full_opaque`，下一刀优先放在 translucent textured row 上的批量 blend，而不是再次重写 sample/hash。
3. 如果 opaque 路径仍然是最大的绝对热点，再评估是否把当前 palette apply 继续推进到 AVX2 chunk 化的 lookup/store，或直接移植一版接近 RVV `sample_combine_chunk` 的 AVX2 向量版。
