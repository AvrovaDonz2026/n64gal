# Windows x64 AVX2 Perf Investigation (2026-03-07)

## Context

本记录专门追踪 GitHub `windows-x64` runner 上 `scalar -> avx2` 仍未转正的问题，并给出当前代码内的两轮修正动作。

调查对象：GitHub Actions `ci-matrix` push run `22793662179`（head `eb83ea1`）中的 `perf-windows-x64` artifact。

## Runner Evidence

来自该 run 的 `scalar -> avx2` smoke compare 结果如下：

| scene | scalar p95 ms | avx2 p95 ms | p95 gain |
|---|---:|---:|---:|
| S1 | 2.525 | 2.668 | -5.66% |
| S3 | 2.726 | 2.735 | -0.33% |

结论：`windows-x64` 当前不是“阈值配置问题”，而是 runner 上真实存在 `avx2` 轻微回退。

同时，逐帧 CSV 显示回退集中在 `raster_ms`，不是 VM/build/audio：`frame_ms` 与 `raster_ms` 基本重合。

## Root Cause

当前最可信的解释不是“AVX2 算法错误”，而是“AVX2 覆盖面太窄，Windows/MSVC 正好把这套结构的缺点放大了”。

1. `avx2` 后端里真正显式使用 YMM intrinsic 的核心只有 `fill_u32`，见 [`src/backend/avx2/avx2_backend.c`](../src/backend/avx2/avx2_backend.c)。
2. `SPRITE/TEXT` 的像素主循环仍是逐像素标量调用 `sample -> combine -> blend`，而 helper 实现在 [`src/backend/common/pixel_pipeline.c`](../src/backend/common/pixel_pipeline.c) 这个公共 TU 中。
3. `S1/S3` 每帧固定包含 `CLEAR + SPRITE + TEXT + FADE` 四个 op，见 [`src/frontend/render_ops.c`](../src/frontend/render_ops.c)。其中 `FADE` 是整屏 `600x800` 的 uniform alpha blend。
4. 修改前，`VN_OP_FADE` 在 `alpha < 255` 时仍逐像素调用 `vn_pp_blend_rgb()`，所以 Windows runner 上最大的固定热点之一其实根本没有吃到 AVX2。
5. Windows/MSVC 只对 `avx2_backend.c` 单独加 `/arch:AVX2`，见 [`CMakeLists.txt`](../CMakeLists.txt)。这意味着公共 `pixel_pipeline.c` 既没有 AVX2 ISA，也没有任何 IPO/LTO 帮它自动并入 AVX2 热循环。

简化后可以理解为：

- `scalar`：大部分热路径都是标量，但 MSVC Release 对简单顺序循环已经足够强。
- `avx2`：只有少数 fill 路径是显式 SIMD，而整屏 fade 和逐像素 sample/combine 仍是标量，所以 GitHub Windows runner 上很容易只看到“额外 AVX2 壳层成本”，看不到足够大的 SIMD 收益面。

## In-Tree Mitigation

本轮已先把最值当的固定热点补成显式 AVX2，并继续把 `sample/combine/blend` 热循环收回 AVX2 TU：

1. `vn_avx2_fill_u32(...)` 改为“对齐前缀 + aligned store”，减少 Windows x64 上 `storeu` 的保守成本。
2. 新增 `vn_avx2_blend_uniform_u32(...)`，把 `uniform color + uniform alpha` 的 row kernel 改成显式 AVX2。
3. `vn_avx2_fill_rect_uniform_clipped(...)` 在 `alpha < 255` 时不再逐像素调用公共 `vn_pp_blend_rgb()`，而是直接走新的 AVX2 row kernel。
4. 新增 backend-local 的 `hash/sample/combine/blend` 热路径 helper，并把 `vn_avx2_draw_textured_rect_clipped(...)` 改成按 row 执行的 `opaque` / `alpha` 两条路径，避免 `windows-x64` 上每像素跨 TU 调用公共 `pixel_pipeline.c`。

这条修正的目标很明确：

- 先把 `S1/S3` 中固定出现的整屏 `FADE` 从标量路径移出去。
- 再把 `SPRITE/TEXT` 的每像素 `sample/hash -> combine -> blend` 从“AVX2 backend 外壳 + 公共 TU helper”收成后端本地热循环。
- 在不引入 gather/scatter 纹理向量化之前，先把 Windows/MSVC 最敏感的调用边界成本压下去。

## Local Validation

已完成的本地验证：

1. `./scripts/ci/run_cc_suite.sh`：通过。
2. 第一轮本地 smoke compare：
   `./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S1,S3 --duration-sec 2 --warmup-sec 1 --dt-ms 16 --resolution 600x800 --out-dir /tmp/n64gal_perf_windows_avx2_investigation_local`
3. 第二轮本地 smoke compare（加入 backend-local textured hot path 后）：
   `./tests/perf/run_perf_compare.sh --baseline scalar --candidate avx2 --scenes S1,S3 --duration-sec 2 --warmup-sec 1 --dt-ms 16 --resolution 600x800 --out-dir /tmp/n64gal_perf_windows_avx2_investigation_local_v2`

本机结果仅作为方向性证据，不可替代 GitHub Windows runner。

第一轮结果（`uniform alpha/fade` AVX2 row kernel）：

| scene | scalar p95 ms | avx2 p95 ms | p95 gain |
|---|---:|---:|---:|
| S1 | 12.661 | 5.975 | +52.81% |
| S3 | 9.073 | 6.329 | +30.24% |

第二轮结果（继续去掉 textured 热循环的跨 TU helper 调用）：

| scene | scalar p95 ms | avx2 p95 ms | p95 gain |
|---|---:|---:|---:|
| S1 | 8.992 | 5.867 | +34.75% |
| S3 | 10.366 | 6.166 | +40.52% |

这说明两点：

1. 把 `uniform alpha fade` 从标量路径拉进 AVX2 是有效的。
2. 把 `sample/combine/blend` 留在 AVX2 TU 内也没有引入新的语义或固定开销问题；`test_backend_consistency`、`test_runtime_golden` 与本地 smoke 都仍然维持正向结果。

## Next Experiments

1. 重新跑 GitHub `windows-x64` perf artifact，观察 `scalar -> avx2` 是否由负收益回到持平或正收益。
2. 如果仍未转正，下一步优先补 Windows x64 专项 kernel benchmark，把 `fill_u32`、`uniform alpha blend`、`backend-local sample+combine+blend` 三段拆开测。
3. 如果 kernel benchmark 仍显示 textured 热路径是主瓶颈，再决定是否继续做真正的 AVX2 row-vectorized `sample/hash -> combine -> alpha`，而不是只停留在 backend-local 标量融合。
4. 若 Windows/MSVC 仍表现异常，再补 objdump/asm 级分析，重点检查 `avx2_backend.obj` 周围是否存在不划算的调用边界或寄存器清理成本。
