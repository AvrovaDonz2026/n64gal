---

# 技术白皮书（执行版）

## N64-RDP 架构视觉小说引擎

**从“技术可行”到“工程可交付”的落地蓝图**

---

## 目录

1. 执行摘要
2. 对 v1.0 白皮书的执行性评估
3. 产品边界与非目标
4. 可验证技术假设与验收标准
5. 架构设计与模块契约
6. 资源格式与脚本规范（可实现版本）
7. 帧循环与性能预算
8. 基准测试方案（可复现）
9. 路线图与里程碑（DoD）
10. 14 天 MVP 任务分解
11. 风险矩阵与回退方案
12. CI / 质量门禁
13. 工程治理补充规范
14. 性能深度优化路线
15. 引擎生态设想与演进边界
16. 结论与立即行动项
17. 开工实施手册（Day0-Day2）
18. 首周并行执行计划（可直接派工）

---

## 1. 执行摘要

本项目目标是构建一个在低功耗 CPU 上稳定运行的视觉小说引擎，采用 N64 RDP 的固定功能思想，以降低运行时复杂度、提升确定性和可移植性。

语言与实现约束：运行时代码统一采用 **C89（ANSI C）**，不依赖 C99/C11 特性。

与上一版不同，本版将“预测性能”改为“分阶段验证”：

- 先证明可运行：`600x800 / 60fps` 的完整剧情切片
- 再证明可维护：有稳定 ABI、回归测试、资源打包链路
- 最后证明可扩展：前后端分离 + `AVX2/NEON/RVV` 后端路线

本版不再直接宣称“10-50 倍能效提升”为既成事实，而是将其降级为假设，必须通过统一测试场景与对照组验证。

分辨率策略：默认目标为 `600x800`（竖屏），并提供运行时档位降级以保障帧率稳定。

---

## 2. 对 v1.0 白皮书的执行性评估

### 2.1 已有优势

- 技术方向清晰：固定功能渲染 + 字节码脚本 + 离线预处理
- 关键数据结构思路正确：Tile 处理、显式资源格式、低动态分配
- 目标场景明确：视觉小说（静态/弱动态 2D 图层）

### 2.2 执行断层（本版重点修复）

| 问题 | v1.0 表现 | 风险 |
|------|----------|------|
| 指标不可验收 | 大量“预测值”缺测量协议 | 项目后期争议大，难收敛 |
| 路线图粒度粗 | 仅阶段描述，无任务依赖 | 进度不可控 |
| 缺少失败回退 | SIMD / 格式设计无 Plan B | 卡在单点难题 |
| 接口边界不硬 | 模块职责未冻结 | 后期重构成本高 |
| 测试场景不统一 | 没有标准场景集 | 性能结论不可复现 |

---

## 3. 产品边界与非目标

### 3.1 MVP 必须支持（Must Have）

1. 背景图切换（含淡入淡出）
2. 角色立绘多图层叠加
3. 对话文本逐字显示 + 历史回看（至少 128 条）
4. 选择分支（`CHOICE` + 跳转）
5. BGM 流式播放 + 单轨 SE
6. 存档/读档（3 槽位，完整状态恢复）
7. 单包发布（`*.vnpak`）

### 3.2 明确非目标（MVP 阶段不做）

1. Live2D / Spine
2. 视频解码播放
3. 3D 场景与透视摄像机
4. 复杂后处理（Bloom、DoF、Motion Blur）
5. 在线热更新与联网服务

### 3.3 目标平台优先级

1. `P0`: Linux x86_64（开发基线）
2. `P1`: Linux ARM64（树莓派 4 / Zero 2W）
3. `P2`: Windows x86_64
4. `P3`: WebAssembly（实验性）

### 3.4 语言标准约束（强制）

1. 运行时代码、公共头文件、平台层统一使用 C89
2. 禁止使用 C99/C11 特性：`stdint.h`、`stdbool.h`、`//` 注释、`for (int i=...)`、可变长数组、指定初始化器、`inline`
3. 允许在工具链代码中使用 Python（打包器、资源预处理），但不影响运行时 C89 约束
4. SIMD 优化允许使用编译器 intrinsic，但必须包在 C89 风格接口后面（禁止向外泄漏编译器私有类型）

### 3.5 分辨率目标与档位策略

1. 设计分辨率：`600x800`（竖屏坐标系，UI 与脚本逻辑均以此为基准）
2. 质量档位：
   - `R0`：`600x800`（质量优先）
   - `R1`：`450x600`（平衡模式）
   - `R2`：`300x400`（性能保底）
3. 当 `frame_time_p95 > 16.67ms` 连续 120 帧时，自动降一级
4. 当 `frame_time_p95 < 13.0ms` 连续 300 帧时，可尝试升一级
5. 分辨率切换不得改变脚本逻辑坐标，仅影响渲染后端输出尺寸

---

## 4. 可验证技术假设与验收标准

| 假设 ID | 假设内容 | 验证方法 | 通过标准 |
|--------|----------|---------|---------|
| H1 | 固定功能命令流可满足 VN 渲染需求 | 跑标准场景 S0-S3，并追加 S10 压力样本 | 全场景视觉正确，差异图误差 <1% |
| H2 | 提升到 `600x800` 后仍可保持可玩帧率 | 关闭/开启 SIMD 与 P0 优化对比 | `600x800` 下标量 >=20fps；SIMD+P0 在 `S0/S1` >=60fps、`S2/S3` >=50fps，`S10` 作为更重压力样本进入 perf 趋势报告 |
| H3 | `vnpak` + mmap 可将加载峰值内存压到 64MB 内 | 启动与场景切换采样 RSS | 峰值 RSS <=64MB |
| H4 | 字节码 VM 可在 0.2ms 内完成单帧脚本推进 | 场景 S2 中 1000 指令压测 | P95 <=0.2ms |
| H5 | 资源离线化能显著降低启动耗时 | 对比“原图直接加载”方案 | 启动时间 <=1.5s |

说明：`H2/H5` 失败不阻塞发布，可降级发布；`H1/H3/H4` 失败则阻塞发布。

---

## 5. 架构设计与模块契约

### 5.1 逻辑分层

```text
┌──────────────────────────────────────────┐
│             Game Runtime                 │
├──────────────────────────────────────────┤
│ Script VM │ Scene Graph │ Save/Load     │
├──────────────────────────────────────────┤
│ Command Builder (Render IR / VNRenderOp)│
├──────────────────────────────────────────┤
│ Software RDP Backend (scalar/SIMD)      │
├──────────────────────────────────────────┤
│ Platform Layer (window/input/audio/fs)  │
└──────────────────────────────────────────┘
```

### 5.2 线程模型（MVP）

- 主线程：脚本推进、命令构建、输入处理
- 音频线程：BGM 解码与缓冲提交
- 不启用渲染多线程，先保证确定性和可调试性

### 5.3 模块边界（冻结 ABI，减少返工）

```c
/* types.h (C89) */
typedef unsigned char  vn_u8;
typedef signed short   vn_i16;
typedef unsigned short vn_u16;
typedef unsigned int   vn_u32;  /* 约束: sizeof(vn_u32) == 4 */

/* C89 尺寸检查（编译期） */
typedef char vn_check_u8[(sizeof(vn_u8) == 1) ? 1 : -1];
typedef char vn_check_u16[(sizeof(vn_u16) == 2) ? 1 : -1];
typedef char vn_check_u32[(sizeof(vn_u32) == 4) ? 1 : -1];

#define VN_TRUE  1
#define VN_FALSE 0

/* vn_renderer.h */
typedef struct {
    vn_u16 width;
    vn_u16 height;
    vn_u32 flags;    /* bit0:simd, bit1:vsync */
} RendererConfig;

int renderer_init(const RendererConfig* cfg);
void renderer_begin_frame(void);
void renderer_submit(const VNRenderOp* ops, vn_u32 op_count);
void renderer_end_frame(void);
void renderer_shutdown(void);

/* vn_vm.h */
typedef struct VNState VNState;
void vm_step(VNState* s, vn_u32 delta_ms);
int vm_is_waiting(const VNState* s);

/* vn_pack.h */
typedef struct VNPak VNPak;
int vnpak_open(VNPak* pak, const char* path);
const ResourceEntry* vnpak_get(const VNPak* pak, vn_u32 id);
void vnpak_close(VNPak* pak);
```

接口冻结规则：

1. `M1` 后只允许新增字段，不允许修改既有语义
2. 所有公共结构体必须显式大小与字节对齐
3. 通过 ABI smoke test（加载旧 `vnpak` 和旧脚本）
4. 公共头文件必须通过 `-std=c89 -pedantic-errors` 编译

### 5.4 前后端分离架构（强制）

前端（Frontend）职责：

1. 脚本 VM 推进、剧情状态管理、输入语义处理
2. 场景图计算（图层、文字、转场）并生成 Render IR
3. 分辨率档位策略（`R0/R1/R2`）与性能策略决策
4. 不包含任何 ISA 相关代码（AVX2/NEON/RVV）

后端（Backend）职责：

1. 消费 Render IR 并执行光栅与混合
2. 维护平台相关像素缓冲、present、同步策略
3. 提供可查询能力（SIMD、LUT、纹理缓存）
4. 仅通过 `vn_backend.h` 暴露统一 ABI，不反向依赖 VM/脚本模块

边界约束：

1. Frontend 不得直接调用 `avx2/neon/rvv` 私有函数
2. Backend 不得读取脚本字节码、存档结构、资源包解析状态
3. 所有跨层通信只能通过 `VNRenderOp[]` 与 `RendererConfig`
4. 新增后端时不得修改 Frontend 逻辑代码路径
5. 跨架构迁移遵循“前端源码零改动”：只新增后端实现并重新编译前端

### 5.5 Render IR 与后端 ABI（C89）

```c
/* vn_backend.h */
#define VN_ARCH_SCALAR  1
#define VN_ARCH_AVX2    2
#define VN_ARCH_NEON    3
#define VN_ARCH_RVV     4

typedef struct {
    vn_u8  op;         /* 1=clear,2=sprite,3=text,4=fade */
    vn_u8  layer;
    vn_u16 tex_id;
    vn_i16 x;
    vn_i16 y;
    vn_u16 w;
    vn_u16 h;
    vn_u8  alpha;
    vn_u8  flags;
} VNRenderOp;

typedef struct {
    vn_u32 has_simd;
    vn_u32 has_lut_blend;
    vn_u32 has_tmem_cache;
} VNBackendCaps;

typedef struct {
    const char* name;
    vn_u32 arch_tag;
    int  (*init)(const RendererConfig* cfg);
    void (*shutdown)(void);
    void (*begin_frame)(void);
    int  (*submit_ops)(const VNRenderOp* ops, vn_u32 op_count);
    void (*end_frame)(void);
    void (*query_caps)(VNBackendCaps* out_caps);
} VNRenderBackend;

int vn_backend_register(const VNRenderBackend* be);
const VNRenderBackend* vn_backend_select(vn_u32 prefer_arch_mask);
```

### 5.6 后端能力矩阵与落地顺序

| 后端 | ISA | 目标阶段 | 目标平台 | 目标角色 |
|------|-----|---------|---------|---------|
| `scalar` | 通用 C89 | M0 | 全平台 | 正确性基线与回退路径 |
| `avx2` | x86_64 AVX2 | M1 | amd64 Linux/Windows | 前期性能主力 |
| `neon` | ARMv8 NEON | M2 | arm64 Linux | 中期低功耗主力 |
| `rvv` | RISC-V Vector | M3 | riscv64 Linux | 后期扩展目标 |

后端选择策略：

1. 启动默认按 `avx2 -> neon -> rvv -> scalar` 选择可用后端
2. 任一后端初始化失败必须自动回退到 `scalar`
3. 命令行支持强制后端：`--backend=scalar|avx2|neon|rvv`
4. 运行日志必须记录 `backend_name` 与 `arch_tag`

跨架构移植流程（固定）：

1. 保持 Frontend 与 `VNRenderOp` 不变
2. 在 `src/backend/<isa>` 新增后端实现并接入 `vn_backend_register`
3. 在目标架构重新编译整个工程
4. 通过 `scalar` 对照一致性测试与场景性能门槛

---

## 6. 资源格式与脚本规范（可实现版本）

### 6.1 `vnpak` v1.1 格式（兼容 v1.0 思路）

```text
[Header] 64 bytes
0x00 magic         "VNE\0"
0x04 version       0x00010001
0x08 res_count     u32
0x0C table_offset  u32
0x10 data_offset   u32
0x14 string_offset u32
0x18 header_crc32  u32
0x1C reserved[40]

[Resource Table] res_count * 24 bytes
0x00 type          u8
0x01 flags         u8
0x02 width         u16
0x04 height        u16
0x06 format        u8
0x07 mip_count     u8
0x08 name_off      u32
0x0C data_off      u32
0x10 data_size     u32
0x14 crc32         u32
```

### 6.2 离线打包器职责（必须完成）

1. 图像转换：PNG -> `RGBA16/CI8/IA8`
2. 调色板量化：可选 `median-cut`
3. 文本脚本编译：`*.vns.txt` -> `*.vns.bin`
4. 校验写入：每资源 CRC32
5. 生成清单：`manifest.json`（便于回归）

### 6.3 字节码规范（MVP 指令子集）

```text
0x01 BG        u16:tex_id, u16:duration_ms
0x02 SPRITE    u8:layer, u16:tex_id, i16:x, i16:y
0x03 TEXT      u16:str_id, u16:speed_ms
0x04 WAIT      u16:duration_ms
0x05 CHOICE    u8:count, [u16:str_id, u16:target_pc] * count
0x06 GOTO      u16:target_pc
0x07 CALL      u16:target_pc
0x08 RETURN
0x09 FADE      u8:layer_mask, u8:target_alpha, u16:duration_ms
0x0A BGM       u16:audio_id, u8:loop
0x0B SE        u16:audio_id
0xFF END
```

说明：`PARTICLE/SHAKE` 暂缓到 `M2`，先保证主流程闭环。

---

## 7. 帧循环与性能预算

### 7.1 固定时间步进（避免时序抖动）

```c
/* dt 固定 16ms，渲染可插值（MVP 阶段可不做插值） */
const vn_u32 FIXED_DT_MS = 16;

while (running) {
    poll_input();
    vm_step(&state, FIXED_DT_MS);
    build_render_ops(&state, op_buf, &op_count);

    renderer_begin_frame();
    renderer_submit(op_buf, op_count);
    renderer_end_frame();
}
```

### 7.2 单帧预算（600x800 @ 60fps -> 16.67ms）

| 模块 | 目标 | 预警线 |
|------|------|-------|
| 脚本 VM | <=0.25ms | >=0.60ms |
| 命令构建 | <=0.80ms | >=1.50ms |
| 光栅化 | <=9.00ms | >=13.00ms |
| 音频 | <=0.80ms | >=1.50ms |
| 输入/系统 | <=0.30ms | >=0.70ms |
| 总计 | <=11.15ms | >=16.00ms |

策略：结合 `R0/R1/R2` 档位保留安全余量，避免低端设备热降频后持续掉帧。

---

## 8. 基准测试方案（可复现）

### 8.1 标准场景集

1. `S0`: 静态背景 + 文本滚动
2. `S1`: 双立绘 + 背景淡入淡出
3. `S2`: 快速脚本跳转 + 选项弹出
4. `S3`: 高频 SE + BGM + 图层切换

每个场景固定 120 秒，前 20 秒热身，不计入统计。

### 8.2 指标采集

- `fps_avg`, `fps_p1`, `frame_time_p95`
- `cpu_user%`, `cpu_sys%`
- `rss_peak_mb`
- `startup_ms`（进程启动到首帧）

### 8.3 基线对照

- 对照组 A：同素材 WebGAL（Chromium）
- 对照组 B：同素材 Ren'Py
- 结论必须附场景、设备、命令行与版本号

### 8.4 采样实现建议（C89 兼容）

1. 帧时：`clock_gettime(CLOCK_MONOTONIC)`（平台不支持时回退 `gettimeofday`）
2. 内存：Linux 读取 `/proc/self/statm`，Windows 读取 `PROCESS_MEMORY_COUNTERS`
3. CPU 占用：优先系统 API，避免在主循环内执行高开销 shell 命令
4. 输出：每场景生成 `perf_<scene>.csv`，字段顺序固定，便于 CI 对比

推荐字段：

```text
scene,frame,frame_ms,vm_ms,build_ms,raster_ms,audio_ms,rss_mb
```

---

## 9. 路线图与里程碑（DoD）

### M0：最小可运行内核（第 1-2 周）

目标：跑通“背景 + 文本 + 单分支脚本”，并建立前后端分离骨架。

DoD：

1. 可加载 `vnpak` 并显示首屏
2. `TEXT/WAIT/GOTO/END` 正确执行
3. `600x800` 下稳定 30fps（x86_64，标量路径）并支持分辨率档位切换
4. 有 20 条单元测试（字节码解码、资源表解析）
5. Debug/Release 均可在 `-std=c89 -pedantic-errors` 下通过
6. Frontend 可稳定输出 `VNRenderOp[]`，`scalar` 后端可独立消费

### M1：前期目标（amd64 + AVX2， 第 3-6 周）

目标：在不改 Frontend 逻辑的前提下，完成 AVX2 后端并达到首发性能。

DoD：

1. 完成 `avx2` 后端并在 amd64 默认启用
2. 支持 `CHOICE/CALL/RETURN/FADE/BGM/SE`，可游玩 15 分钟 Demo
3. `R0(600x800)` 下 `S0-S3` 达到 >=60fps（x86_64），并将 `S10` 作为更重压力样本纳入 perf 报告
4. `avx2` 与 `scalar` 对照测试通过（差异图误差 <1%）
5. 切换 `--backend=scalar|avx2` 不要求改动 Frontend 代码

### M2：中期目标（arm64 + NEON， 第 7-10 周）

目标：在保持 Frontend 不变的条件下，交付 ARM 平台版本。

DoD：

1. 完成 `neon` 后端并在 arm64 默认启用
2. Zero 2W 在自适应档位下 `S0-S2` 达到 >=45fps，`S3` >=40fps
3. `neon` 与 `scalar` 对照测试通过（差异图误差 <1%）
4. 内存峰值 <=64MB
5. 发布文档、迁移指南、性能报告齐备
6. `scalar/avx2/neon` 均通过 C89 门禁（接口层不泄漏 C99+ 特性）

### M3：后期目标（riscv64 + RVV， 第 11-14 周）

目标：在同一 Frontend 与 Render IR 上完成 RVV 后端落地。

DoD：

1. 完成 `rvv` 后端并可在 riscv64 启动
2. `riscv64` 验证链分层完成：`cross-build -> qemu-scalar -> qemu-rvv -> native-riscv64`
3. riscv64 在自适应档位下 `S0-S2` 达到 >=35fps，`S3` >=30fps
4. `rvv` 与 `scalar` 对照测试通过（差异图误差 <1%）
5. 回退链稳定：`rvv` 失败自动切回 `scalar`
6. 发布《后端移植指南》：新增 ISA 后端仅需实现 `vn_backend.h` 约定接口

---

## 10. 14 天 MVP 任务分解

| 天数 | 任务 | 输出 |
|------|------|------|
| D1 | 初始化工程骨架（CMake + src/tests/tools） | 可构建空程序 |
| D2 | 定义 `VNRenderOp` 与后端 ABI（`vn_backend.h`） | Frontend/Backend 可独立编译 |
| D3 | `scalar` 后端最小实现（FillRect） | 首帧着色 |
| D4 | 纹理加载与 BG 绘制 | 背景显示 |
| D5 | VM 解码器（`TEXT/WAIT/GOTO/END`） | 脚本能跑 |
| D6 | 字体图集离线生成 + 文本渲染 | 逐字显示 |
| D7 | `vnpak` 读取器 + 资源索引 | 外部资源加载 |
| D8 | `CHOICE` UI 与输入 | 可分支 |
| D9 | `FADE` 过渡 | 场景切换 |
| D10 | BGM/SE 基础播放 | 音频可用 |
| D11 | 存档/读档 | 状态恢复 |
| D12 | `avx2` 后端原型接入与切换开关 | `--backend=scalar|avx2` |
| D13 | 场景 S0-S3 + S10 压力样本跑测 | 基线报告 |
| D14 | 修复阻塞缺陷 + 冻结 MVP | `v0.1.0-mvp` |

---

## 11. 风险矩阵与回退方案

| 风险 | 触发条件 | 影响 | 回退方案 |
|------|---------|------|---------|
| SIMD 后端收益不足 | AVX2/NEON/RVV 提升 <25% | `R0(600x800)` 难达 60fps | 启用自适应档位：`R1(450x600)` -> `R2(300x400)` |
| 前后端边界被破坏 | Frontend 直接依赖 ISA 私有代码 | 后续移植成本飙升 | 强制通过 `vn_backend.h` 审查门禁，违规 PR 阻塞 |
| RVV 工具链不稳定 | riscv64 编译器或仿真器版本不一致 | M3 延期 | 先交付 `riscv64+scalar`，RVV 以可选后端发布 |
| 资源格式频繁变更 | 两周内改动 >3 次 | 工具链不稳定 | 冻结 v1.1，新增字段仅向后兼容 |
| 音频线程卡顿 | 缓冲下溢 >3 次/分钟 | 爆音 | 增大缓冲 + 降低解码质量 |
| 启动时间超标 | >1.5s | 体验差 | 预索引 + 延迟加载非首屏资源 |
| 内存超预算 | RSS >64MB | 低端设备崩溃 | 强制 CI8/IA8 纹理策略 |

---

## 12. CI / 质量门禁

### 12.1 构建与测试命令（建议）

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_STANDARD=90 \
  -DCMAKE_C_EXTENSIONS=OFF \
  -DCMAKE_C_FLAGS="-std=c89 -pedantic-errors -Wall -Wextra -Werror"
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/vn_player --pak assets/demo.vnpak --script demo.vns.bin
```

### 12.2 Pipeline 必过项

1. Linux x86_64 Debug/Release 构建成功（`scalar + avx2`）
2. arm64/riscv64 交叉编译成功（至少 `scalar` 后端）
3. `riscv64` 必须分层验证：`cross-build` 阻塞、`qemu-scalar` 阻塞、`qemu-rvv` 在 `M3` 前可告警、原生 riscv64 perf 为发布前阻塞
4. 单元测试通过率 100%
5. 场景 `S0-S2` 自动跑测，产出 `perf.csv`
6. C89 合规检查通过（`-std=c89 -pedantic-errors` + 禁用 C99 特性扫描）
7. 后端一致性测试通过（`scalar` 对照 `avx2/neon/rvv` 差异图 <1%）
8. 打包器对样例资源生成确定性输出（同输入同哈希）

### 12.3 C89 合规清单（评审必查）

1. 所有 `for` 循环变量在循环外声明
2. 注释仅使用 `/* ... */`
3. 无混合声明与语句（块开头先声明）
4. 无 C99 头文件依赖（`stdint.h`/`stdbool.h`）
5. `types.h` 中提供固定宽度别名与尺寸检查

### 12.4 建议的自动扫描规则

```bash
# 禁用 C99/C11 特征（按需扩展）
grep -R -nE "\\bstdint\\.h\\b|\\bstdbool\\.h\\b|//|for \\(int |\\binline\\b" include src

# 公共头文件必须可单独编译
for h in include/*.h; do
  cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -c "$h" -o /tmp/$(basename "$h").o
done
```

### 12.5 后端矩阵测试（前后端分离门禁）

```text
Job A: x86_64 + scalar
Job B: x86_64 + avx2
Job C: arm64 + scalar (native)
Job D: arm64 + neon   (native)
Job E0: riscv64 + scalar/runtime/tests (cross-build)
Job E1: riscv64 + scalar/runtime/tests (qemu-user)
Job F0: riscv64 + rvv (qemu-user, M3 前 non-blocking)
Job F1: riscv64 + scalar/rvv + perf (native nightly)
```

要求：

1. Job A/B 为阻塞项（M1 起）
2. Job C/D 为阻塞项（M2 起）
3. Job E0/E1 为阻塞项，保证 `riscv64 + scalar` 不是“只会编译”
4. Job F0 为告警项（M3 前），M3 完成后转阻塞
5. Job F1 负责真机功能与 perf 证据链，不进入普通 PR 阶段
6. 任一后端失败必须验证 `scalar` 回退路径仍可运行

---

## 13. 工程治理补充规范

### 13.1 目录结构基线（M0 即冻结）

```text
n64gal/
├─ CMakeLists.txt
├─ cmake/
├─ include/
│  ├─ vn_types.h
│  ├─ vn_backend.h
│  ├─ vn_renderer.h
│  ├─ vn_vm.h
│  ├─ vn_pack.h
│  └─ vn_error.h
├─ src/
│  ├─ core/
│  ├─ frontend/
│  ├─ backend/
│  │  ├─ common/
│  │  ├─ scalar/
│  │  ├─ avx2/
│  │  ├─ neon/
│  │  └─ rvv/
│  ├─ vm/
│  ├─ pack/
│  ├─ audio/
│  └─ platform/
├─ tools/
│  ├─ packer/
│  └─ scriptc/
├─ assets/
│  ├─ scenes/
│  └─ demo/
├─ tests/
│  ├─ unit/
│  ├─ perf/
│  └─ golden/
└─ docs/
```

约束：

1. `include/` 只能放稳定 ABI 头文件
2. `src/` 内部头文件不允许被 `tools/` 直接引用
3. `tests/golden/` 统一保存视觉与字节码金样
4. `src/frontend` 不得依赖 `src/backend/*` 私有头文件
5. 新 ISA 后端仅允许在 `src/backend/<isa>` 下增量实现

### 13.2 错误码规范（统一返回语义）

```c
/* vn_error.h */
#define VN_OK                     0
#define VN_E_INVALID_ARG         -1
#define VN_E_IO                  -2
#define VN_E_FORMAT              -3
#define VN_E_UNSUPPORTED         -4
#define VN_E_NOMEM               -5
#define VN_E_SCRIPT_BOUNDS       -6
#define VN_E_RENDER_STATE        -7
#define VN_E_AUDIO_DEVICE        -8
```

规则：

1. 公共 API 统一使用 `int` 返回值（`0` 成功，负值失败）
2. 失败路径必须写日志并保留上下文（资源 ID、PC、帧号）
3. 禁止返回“魔法数”错误码；必须落在 `vn_error.h` 统一定义

### 13.3 日志与可观测性规范

日志级别：

1. `ERROR`：功能失败或数据损坏风险
2. `WARN`：降级运行（例如关闭 SIMD）
3. `INFO`：启动、场景切换、存读档
4. `TRACE`：开发态逐帧细节（Release 默认关闭）

推荐日志行格式：

```text
{ts_ms} {level} {module} code={err} frame={f} pc={pc} msg={text}
```

要求：

1. 每次崩溃前最后 256 行日志可导出
2. `S0-S3 + S10` 跑测需附日志摘要（错误数、警告数、平均帧时）
3. 日志写入失败不应阻塞主循环

### 13.4 存档格式（vnsave v1）

```text
[Header] 32 bytes
0x00 magic         "VNSV"
0x04 version       0x00010000
0x08 slot_id       u32
0x0C script_pc     u32
0x10 scene_id      u32
0x14 timestamp_s   u32
0x18 payload_crc32 u32
0x1C reserved      u32

[Payload]
- 图层状态（tex_id/x/y/alpha）
- 文本状态（当前行 ID、逐字偏移）
- 变量区（固定长度 KV）
```

兼容策略：

1. 只保证同主版本可读（`1.x` -> `1.x`）
2. 主版本升级需提供离线迁移器
3. 存档加载失败时回退到最近自动存档

### 13.5 分支策略与变更控制

1. `main`：始终可发布
2. `dev`：日常集成
3. `feat/*`：功能分支，必须通过 PR 合并
4. `fix/*`：缺陷修复分支，可走快速审查

提交信息约定：

```text
type(scope): summary

type = feat|fix|perf|refactor|docs|test|build|ci
```

PR 最低要求：

1. 关联里程碑（M0/M1/M2/M3）与任务 ID
2. 给出行为变化说明和回归风险
3. 附至少一个验证证据（测试输出或对比图）

### 13.6 发布清单（Release Checklist）

`v0.1.0-mvp` 发布前必须满足：

1. `S0-S3 + S10` 全通过，无 `ERROR` 级日志
2. C89 门禁和单测全通过
3. Demo 流程可连续游玩 15 分钟无崩溃
4. `README + docs/perf-report.md + docs/migration.md` 齐备
5. 产物含：可执行文件、`demo.vnpak`、许可证与版本信息

---

## 14. 性能深度优化路线

### 14.1 优化优先级（P0/P1/P2）

| 优先级 | 优化项 | 目标收益（场景） | 实现复杂度 | 默认启用条件 |
|------|------|----------------|----------|------------|
| P0 | 静态帧短路（Frame Reuse） | `S0/S1` CPU 下降 40%-80% | 低 | 连续 300 帧无视觉差异 |
| P0 | Dirty-Rect / Dirty-Tile 增量渲染 | `S1/S2` 光栅开销下降 20%-50% | 中 | 差异图误差 <1% |
| P0 | 命令列表缓存（Command Cache） | 命令构建耗时下降 30%-60% | 低 | 状态签名命中率 >70% |
| P0 | 动态分辨率档位（R0/R1/R2） | 高负载场景帧率提升 20%-45% | 低 | 连续超预算自动降档 |
| P1 | Tile 级遮挡剔除（Occlusion Early-Out） | 光栅阶段下降 15%-35% | 中 | 无遮挡误判 |
| P1 | Alpha 混合 LUT（5-bit 通道） | 混合热点下降 10%-25% | 中 | 与标量参考输出一致 |
| P2 | SIMD 内核细化（AVX2/NEON/RVV） | 光栅阶段再下降 15%-30% | 中高 | 单元测试 + 性能回归通过 |

说明：P0 为 `M1` 必做，P1 为 `M2` 推荐，P2 为平台冲刺项。

### 14.2 P0 实现细则（先做，收益最大）

#### 14.2.1 静态帧短路

判定条件：

1. 脚本 PC 未变化或仅处于 `WAIT`
2. 文本逐字动画已结束
3. 图层状态签名（纹理 ID、坐标、alpha）未变化
4. 输入状态未触发视觉变化（例如快进指示器）

参考流程（C89 风格伪代码）：

```c
if (scene_hash == prev_scene_hash && text_done && effect_idle) {
    renderer_present_cached();
    return;
}
build_render_ops();
raster_frame();
renderer_cache_frame();
```

#### 14.2.2 Dirty-Tile 增量渲染

1. 使用 `8x8` tile 脏标记位图
2. 仅对脏 tile 执行命令回放与光栅
3. 当前工程实现必须挂在 `frame reuse miss -> op cache/patched ops` 之后，不能只看粗粒度状态签名
4. 背景全切、`FADE` 生效或分辨率变化时强制全屏重绘
5. 当前工程化草案见 [`docs/api/dirty-tile-draft.md`](./docs/api/dirty-tile-draft.md)

要求：

1. 每帧输出 `dirty_tile_count`
2. 当脏 tile 比例 >60% 时自动回退全屏路径

#### 14.2.3 命令列表缓存

1. 以场景签名作为 key（图层状态 + 文本状态 + 转场状态）
2. 命中时跳过命令构建，直接重放缓存命令
3. 缓存上限建议 64 条，LRU 淘汰

#### 14.2.4 动态分辨率档位

1. 默认以 `R0(600x800)` 启动
2. 若 `frame_time_p95 > 16.67ms` 连续 120 帧，降到 `R1`；`R1` 仍超预算则降到 `R2`
3. 若 `frame_time_p95 < 13.0ms` 连续 300 帧，尝试升档
4. 切档时只重建渲染目标，不重置脚本状态与音频状态
5. 切档事件写入日志：`INFO perf resolution_switch from=R? to=R?`

### 14.3 P1 实现细则（热点优化）

#### 14.3.1 Tile 级遮挡剔除

1. 先按图层从上到下建立覆盖掩码
2. 发现某 tile 已被不透明层完全覆盖后，跳过底层绘制
3. 对 alpha <255 的像素路径不做剔除，避免误判

#### 14.3.2 Alpha 混合 LUT

1. 通道采用 5-bit 输入（与 `RGBA16` 对齐）
2. 预计算 `(src, dst, a)` 三维表或分段表
3. 以 LUT 结果替代乘加运算

验证方式：

1. 与标量参考混合结果逐像素比对
2. 允许误差阈值：每通道绝对误差 <=1

### 14.4 P2 实现细则（SIMD 冲刺）

策略：

1. 保留纯标量路径作为行为基线
2. 将 SIMD 限定在热点函数：`fill`, `blend`, `tex_fetch`, `combine`
3. AVX2、NEON、RVV 分别维护内核，不共享寄存器布局假设

三阶段推进顺序：

1. 前期：`avx2` 先行，优先覆盖 x86_64 首发性能目标
2. 中期：`neon` 对齐 `avx2` 的算子集合与误差阈值
3. 后期：`rvv` 在不改 Frontend 的情况下复用同一 Render IR 与测试集

编译开关建议：

```text
VN_SIMD=0   标量路径（默认可用）
VN_SIMD=1   自动检测（推荐发布）
VN_SIMD=2   强制 SIMD（仅测试）
```

### 14.5 性能验收 DoD（新增）

#### 14.5.1 指标门槛

1. `S0`: `frame_time_p95 <= 12.0ms`
2. `S1`: `frame_time_p95 <= 14.0ms`
3. `S2`: `frame_time_p95 <= 15.0ms`
4. `S3`: `frame_time_p95 <= 16.2ms`

#### 14.5.2 回归门禁

1. 任一场景 `p95` 退化超过 10% 即阻塞合并
2. 任一场景出现视觉回归（差异图 >1%）即阻塞合并
3. 任一优化默认开启前必须保留运行时开关，可随时降级

#### 14.5.3 数据产出

1. `docs/perf-report.md`：本次优化摘要
2. `tests/perf/*.csv`：原始帧级数据
3. `tests/golden/*`：对照图和差异图

---

## 15. 引擎生态设想与演进边界

这份白皮书的主线仍然是“先把运行时和跨架构后端做扎实”。
但如果项目目标是长期可用的视觉小说引擎，那么生态必须从一开始就定义边界，否则后续工具、编辑器、宿主接入和内容分发会各自演化出不兼容接口。

### 15.1 生态目标

生态不是“做一个大而全 IDE”，而是围绕稳定契约形成可协作的生产链：

1. 内容作者可以稳定地产生 `script + asset + pack`。
2. 宿主程序可以稳定地嵌入 `vn_runtime`，而不需要理解后端 ISA 细节。
3. 后端作者只需要实现统一 Backend API，不必改 Frontend、工具链或内容格式。
4. 测试、性能、兼容性证据可以随版本沉淀，而不是散落在 issue 和聊天记录里。

### 15.2 生态分层（建议冻结）

```text
Layer 0: Runtime Core
  vn_runtime / VM / Frontend IR / Backend ABI / Save / Pack reader

Layer 1: Creator Toolchain
  scriptc / packer / validator / perf capture / golden diff / migration tools

Layer 2: Host SDK
  C API / session API / input bridge / file bridge / telemetry hooks

Layer 3: Editor & Preview
  script preview runner / pack preview / live reload protocol / error diagnostics

Layer 4: Distribution & Community
  sample projects / templates / compatibility matrix / release bundles / extension registry
```

约束：

1. Layer 0 必须保持 C89 运行时约束。
2. Layer 1-4 允许使用更高层语言或脚本，但不得反向污染运行时 ABI。
3. 所有层之间都必须优先走“文件格式 + CLI + 文档契约”，避免过早绑定脆弱的进程内插件 ABI。

### 15.3 我们要做的生态，不做的生态

明确做：

1. 离线资源工具链：脚本编译、打包、校验、迁移、性能采样。
2. 样例工程与模板：让新项目在一天内跑起来。
3. 宿主 SDK：让外部程序能够嵌入播放器、会话循环、输入桥接和日志采集。
4. 预览协议：编辑器或外部工具可以拉起预览进程并获得结构化错误。
5. 兼容矩阵：版本、资源格式、后端能力、平台支持的证据链。

当前阶段不做：

1. 运行时热更新脚本 VM ABI。
2. 直接在运行时进程内加载第三方未审计插件。
3. 复杂协作文档系统、云端项目管理或在线编辑器。
4. 为单个编辑器深度耦合私有工程格式。

### 15.4 生态核心构件（建议命名）

1. `vn_runtime`：稳定宿主运行时库。
2. `vn_player`：CLI/调试包装器，用于冒烟、预览、性能与问题复现。
3. `vn_scriptc`：脚本文本 -> 字节码编译器。
4. `vn_packer`：资源打包与 `manifest` 生成器。
5. `vn_validate`：资源包、脚本、版本兼容性校验器。
6. `vn_migrate`：`vnpak/vnsave/script schema` 的迁移工具。
7. `vn_probe`：trace/perf/golden diff 采集工具。
8. `vn_host_sdk`：宿主接入头文件、示例与能力协商文档。
9. `vn_previewd`：预览进程协议入口，供编辑器或脚本工具驱动。

要求：

1. 这些构件优先以独立 CLI/库组合出现，而不是一次性做成单体应用。
2. 所有 CLI 输出都应支持稳定文本模式与机器可读模式（CSV/JSON/Markdown 至少其一）。
3. 任何新工具都不得绕开 `vnpak` / `scriptc` 主路径自造格式。

### 15.5 编辑器与预览协议（先协议、后 GUI）

如果未来要做编辑器，最先冻结的应是预览协议而不是界面样式。

建议最小协议：

1. 输入：工程目录、目标场景、平台后端、分辨率、trace 开关。
2. 输出：首帧状态、结构化错误、资源缺失列表、性能摘要、日志路径。
3. 控制：`reload scene`、`step frame`、`set choice`、`inject input`、`capture screenshot`。
4. 传输：初期可直接使用 CLI + 临时文件；中期再升级为本地 IPC。

这样做的意义：

1. 编辑器、CI 和自动化脚本共用同一条预览链。
2. Linux/Windows/arm64/x64/riscv64 不需要分别维护不同编辑器内核。
3. 出现错误时，可以先复现 CLI 命令，而不是先调 GUI。

### 15.6 宿主 SDK 设计原则

宿主 SDK 的目标不是暴露所有内部实现，而是提供最小稳定集：

1. 初始化与销毁。
2. 单步推进与查询结果。
3. 输入注入（键盘、脚本化输入、选择项）。
4. 文件与资源桥接。
5. 日志、trace、性能采样回调。

必须避免：

1. 让宿主直接操作 Backend 私有结构。
2. 让宿主依赖某个 ISA 特定后端头文件。
3. 把 VM 内部状态布局当成公开 ABI。

### 15.7 扩展机制：先文件级扩展，后插件 ABI

生态扩展建议分两步走。

第一步：文件级扩展

1. 导入器：`png -> rgba16/ci8/ia8`
2. 导出器：截图、trace、golden、perf summary
3. 校验器：pack/script/save compatibility check
4. 迁移器：旧版脚本/资源/存档升级

第二步：插件级扩展

1. 仅在 CLI 或独立工具进程中加载插件
2. 以 `manifest + version range + capability flags` 做能力协商
3. 运行时主进程仍默认拒绝第三方动态插件

原因：

1. 工具进程崩溃不会拖垮运行时。
2. 插件 ABI 变更压力留在工具链，不污染 `vn_runtime` 稳定面。
3. 更适合跨平台和跨架构分发。

### 15.8 版本与兼容矩阵

生态要成立，必须有统一的版本协商表。

至少维护以下版本面：

1. `runtime api version`
2. `backend abi version`
3. `script bytecode version`
4. `vnpak version`
5. `vnsave version`
6. `tool manifest version`
7. `preview protocol version`

兼容规则建议：

1. Runtime 与 Backend ABI 只追加字段，不做破坏性重排。
2. `script/vnpak/vnsave` 升级必须附迁移器或拒绝加载理由。
3. 所有 release 都必须附兼容矩阵和最小工具链版本。

### 15.9 生态路线图（M4 以后）

`M4-engine-ecosystem` 建议拆成四段：

1. `E1` 样例工程与模板：项目骨架、最小 demo、宿主接入示例。
2. `E2` Creator Toolchain：`validate/migrate/probe` 与统一 CLI 出口。
3. `E3` Preview Protocol：编辑器预览与自动化共享同一驱动层。
4. `E4` Extension Registry：导入器/导出器/校验器兼容清单与版本约束。

阶段性 DoD：

1. 新用户能在 30 分钟内从模板工程跑出首个场景。
2. 任意一次格式升级都有迁移命令和回退策略。
3. CI artifact 能附带性能、golden、兼容性证据，而不是只给“过/不过”。
4. 宿主示例在 Linux/Windows 上至少各有 1 个可编译工程。

### 15.10 生态部分的工程纪律

生态建设必须服从主线约束：

1. 不以牺牲 C89 运行时边界换取工具便利。
2. 不以编辑器需求倒逼 Runtime 暴露临时内部结构。
3. 不以“先做 GUI 再补协议”的方式推进。
4. 不在没有兼容矩阵和迁移路径时冻结新格式。
5. 所有生态改动都必须能写回 issue、文档和 CI 证据链。

---

## 16. 结论与立即行动项

### 16.1 结论

N64-RDP 思路对视觉小说引擎是成立的，但必须以“可验证交付”推进，而不是以“理论性能”推进。本版已经将项目约束、接口契约、验证方法、回退路径和里程碑 DoD 明确化，并将渲染链路细化为前后端分离架构与 `AVX2 -> NEON -> RVV` 分阶段后端路线，可直接转入工程实施。

### 16.2 立即行动项（本周）

1. 冻结目录结构与公共头文件（`vn_backend.h/vn_renderer.h/vn_vm.h/vn_pack.h`）
2. 完成 `tools/packer` 最小可用版本（图像 + 脚本）
3. 冻结 `vn_backend.h` 并完成 `scalar + avx2` 双后端切换
4. 实现 P0 四项优化开关（静态帧短路、Dirty-Tile、命令缓存、动态分辨率档位）
5. 建立基准场景 S0-S3，并补入更重的 S10 perf sample 与自动采样脚本

---

## 17. 开工实施手册（Day0-Day2）

### 17.1 Day0 启动检查（2 小时内完成）

启动前必须全部满足：

1. 仓库主干可构建（至少空工程可通过）
2. `vn_backend.h` 接口冻结（字段与函数签名不再变更）
3. C89 门禁接入本地构建命令
4. 目录结构按 `src/frontend` 与 `src/backend/*` 建好
5. Issue 看板创建完成并关联 Milestone

建议执行命令：

```bash
cmake -S . -B build -DCMAKE_C_STANDARD=90 -DCMAKE_C_EXTENSIONS=OFF
cmake --build build -j
```

### 17.2 最小目录落地（一次性建齐）

```text
include/
  vn_types.h
  vn_backend.h
  vn_renderer.h
  vn_vm.h
  vn_pack.h
  vn_error.h
src/
  frontend/
  backend/common/
  backend/scalar/
  backend/avx2/
  backend/neon/
  backend/rvv/
tests/
  unit/
  perf/
  golden/
```

说明：`avx2/neon/rvv` 目录可先放空实现桩，但目录必须先存在，便于并行开发。

### 17.3 首批代码骨架（必须先有）

首批必须落地的符号（未实现可返回 `VN_E_UNSUPPORTED`）：

1. `vn_backend_register`
2. `vn_backend_select`
3. `renderer_init/begin_frame/submit/end_frame/shutdown`
4. `build_render_ops`
5. `scalar` 后端 `init/submit_ops`

强约束：

1. Frontend 只能依赖 `include/*.h`
2. Backend 私有头文件不得被 Frontend include
3. 所有新文件默认开启 `-std=c89 -pedantic-errors`

### 17.4 Definition of Ready（DoR）

Issue 进入开发前必须满足：

1. 有明确输入与输出文件路径
2. 有验收命令（可在本地执行）
3. 有回退策略（失败时如何降级）
4. 有依赖 issue 并已确认状态
5. 有预计工时（1-5 天）

### 17.5 Definition of Done（统一补充）

除 issue 自身 DoD 外，还必须满足：

1. 单测新增或更新
2. `scalar` 对照验证不退化
3. 日志包含关键字段（模块、错误码、帧号）
4. 文档同步（至少更新 `issue.md` 进度）

---

## 18. 首周并行执行计划（可直接派工）

### 18.1 角色与责任边界

建议最小 4 角色并行：

1. `Owner-A (Frontend/VM)`：`ISSUE-002`
2. `Owner-B (Backend-Scalar/AVX2)`：`ISSUE-003`, `ISSUE-007`
3. `Owner-C (Tools/Pack)`：`ISSUE-004`
4. `Owner-D (QA/Perf/CI)`：`ISSUE-005`, `ISSUE-006`

规则：各 Owner 只能修改自己责任目录，跨目录改动必须先发 RFC 评论。

### 18.2 首周日程（W1）

| 日期 | 目标 | 责任人 | 验收输出 |
|---|---|---|---|
| D1 | 冻结 `vn_backend.h` + 建目录 | A/B | 接口头文件 + 空实现可编译 |
| D2 | `build_render_ops` 与 scalar 骨架 | A/B | S0 首帧可显示 |
| D3 | `vnpak` 读取 + demo 资源加载 | C | 可加载外部包 |
| D4 | C89 门禁 + perf 脚手架 | D | CI 本地跑通 |
| D5 | S0-S3 场景初版 + S10 perf sample 草案 | D + A | `perf_*.csv` 首版 |
| D6 | AVX2 原型接入 | B | `--backend=avx2` 可运行 |
| D7 | 周验收与回归修复 | 全员 | 周报 + 风险清单 |

### 18.3 首批 PR 切分（建议）

1. `PR-001`: `include/vn_backend.h` + 后端注册器（对应 ISSUE-001）
2. `PR-002`: Frontend Render IR 输出（对应 ISSUE-002）
3. `PR-003`: Scalar 后端最小实现（对应 ISSUE-003）
4. `PR-004`: Packer 与脚本编译最小链路（对应 ISSUE-004）
5. `PR-005`: C89 门禁与 perf 脚手架（对应 ISSUE-005/006）
6. `PR-006`: AVX2 原型与切换开关（对应 ISSUE-007）

每个 PR 限制：

1. 变更文件建议 <=12 个
2. 说明中必须附验收命令与输出摘要
3. 未附回退策略的 PR 不进入 review

### 18.4 每日站会模板（10 分钟）

每人仅汇报三项：

1. 昨天完成了什么（对应 issue 编号）
2. 今天要提交什么（目标 PR 编号）
3. 当前阻塞是什么（需要谁决策）

### 18.5 首周验收出口（必须达成）

1. `M0` 相关 issue 至少完成 4/6
2. `scalar` 后端可运行 `S0`
3. `vn_backend.h` 未发生破坏性变更
4. C89 门禁已默认开启
5. 已产出首份 `perf` CSV

---

**文档版本**：v1.7-executable-start-ready  
**日期**：2026-03-06  
**作者**：基于原始白皮书修订  
**许可**：CC BY-SA 4.0

---
