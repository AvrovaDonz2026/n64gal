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
13. 结论与立即行动项

---

## 1. 执行摘要

本项目目标是构建一个在低功耗 CPU 上稳定运行的视觉小说引擎，采用 N64 RDP 的固定功能思想，以降低运行时复杂度、提升确定性和可移植性。

语言与实现约束：运行时代码统一采用 **C89（ANSI C）**，不依赖 C99/C11 特性。

与上一版不同，本版将“预测性能”改为“分阶段验证”：

- 先证明可运行：`320x240 / 60fps` 的完整剧情切片
- 再证明可维护：有稳定 ABI、回归测试、资源打包链路
- 最后证明可扩展：NEON/AVX2 优化和跨平台构建

本版不再直接宣称“10-50 倍能效提升”为既成事实，而是将其降级为假设，必须通过统一测试场景与对照组验证。

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

---

## 4. 可验证技术假设与验收标准

| 假设 ID | 假设内容 | 验证方法 | 通过标准 |
|--------|----------|---------|---------|
| H1 | 固定功能命令流可满足 VN 渲染需求 | 跑标准场景 S0-S3 | 全场景视觉正确，差异图误差 <1% |
| H2 | 标量实现即可达 30fps，SIMD 达 60fps | 关闭/开启 SIMD 对比 | `320x240` 下标量 >=30fps，SIMD >=60fps |
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
│ Command Builder (fixed-function RDP cmd)│
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

/* renderer.h */
typedef struct {
    vn_u16 width;
    vn_u16 height;
    vn_u32 flags;    /* bit0:simd, bit1:vsync */
} RendererConfig;

int renderer_init(const RendererConfig* cfg);
void renderer_begin_frame(void);
void renderer_submit(const RDPCommand* cmds, vn_u32 count);
void renderer_end_frame(void);
void renderer_shutdown(void);

/* vm.h */
typedef struct VNState VNState;
void vm_step(VNState* s, vn_u32 delta_ms);
int vm_is_waiting(const VNState* s);

/* pack.h */
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
    build_rdp_commands(&state, cmd_buf, &cmd_count);

    renderer_begin_frame();
    renderer_submit(cmd_buf, cmd_count);
    renderer_end_frame();
}
```

### 7.2 单帧预算（60fps -> 16.67ms）

| 模块 | 目标 | 预警线 |
|------|------|-------|
| 脚本 VM | <=0.20ms | >=0.50ms |
| 命令构建 | <=0.40ms | >=1.00ms |
| 光栅化 | <=4.00ms | >=7.00ms |
| 音频 | <=0.80ms | >=1.50ms |
| 输入/系统 | <=0.20ms | >=0.50ms |
| 总计 | <=6.00ms | >=10.00ms |

策略：保留至少 `6ms` 冗余，避免低端设备热降频后跌帧。

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

---

## 9. 路线图与里程碑（DoD）

### M0：最小可运行内核（第 1-2 周）

目标：跑通“背景 + 文本 + 单分支脚本”。

DoD：

1. 可加载 `vnpak` 并显示首屏
2. `TEXT/WAIT/GOTO/END` 正确执行
3. `320x240` 下稳定 60fps（x86_64）
4. 有 20 条单元测试（字节码解码、资源表解析）
5. Debug/Release 均可在 `-std=c89 -pedantic-errors` 下通过

### M1：完整 VN 主流程（第 3-5 周）

目标：可制作并游玩 15 分钟 Demo。

DoD：

1. 支持 `CHOICE/CALL/RETURN/FADE/BGM/SE`
2. 存档/读档可恢复脚本位置和图层状态
3. 通过场景 `S0-S3`，无功能级崩溃
4. ABI 冻结（renderer/vm/pack 三大接口）

### M2：性能与平台（第 6-8 周）

目标：ARM64 可发布版本。

DoD：

1. NEON 路径可编译且默认启用
2. Zero 2W 上 `S0-S2` 达到 >=50fps，`S3` >=45fps
3. 内存峰值 <=64MB
4. 发布文档、迁移指南、性能报告齐备
5. SIMD 与标量路径均通过 C89 门禁（接口层不泄漏 C99+ 特性）

---

## 10. 14 天 MVP 任务分解

| 天数 | 任务 | 输出 |
|------|------|------|
| D1 | 初始化工程骨架（CMake + src/tests/tools） | 可构建空程序 |
| D2 | 定义 `RDPCommand` 与命令缓冲 | 命令序列可打印 |
| D3 | 标量光栅化最小实现（FillRect） | 首帧着色 |
| D4 | 纹理加载与 BG 绘制 | 背景显示 |
| D5 | VM 解码器（`TEXT/WAIT/GOTO/END`） | 脚本能跑 |
| D6 | 字体图集离线生成 + 文本渲染 | 逐字显示 |
| D7 | `vnpak` 读取器 + 资源索引 | 外部资源加载 |
| D8 | `CHOICE` UI 与输入 | 可分支 |
| D9 | `FADE` 过渡 | 场景切换 |
| D10 | BGM/SE 基础播放 | 音频可用 |
| D11 | 存档/读档 | 状态恢复 |
| D12 | 性能计时器与日志导出 | CSV 指标 |
| D13 | 场景 S0-S3 跑测 | 基线报告 |
| D14 | 修复阻塞缺陷 + 冻结 MVP | `v0.1.0-mvp` |

---

## 11. 风险矩阵与回退方案

| 风险 | 触发条件 | 影响 | 回退方案 |
|------|---------|------|---------|
| SIMD 收益不足 | AVX2/NEON 提升 <25% | 难达 60fps | 保留标量路径 + 降分辨率到 256x192 |
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

1. Linux x86_64 Debug/Release 双构建成功
2. 单元测试通过率 100%
3. 场景 `S0-S2` 自动跑测，产出 `perf.csv`
4. C89 合规检查通过（`-std=c89 -pedantic-errors` + 禁用 C99 特性扫描）
5. 打包器对样例资源生成确定性输出（同输入同哈希）

### 12.3 C89 合规清单（评审必查）

1. 所有 `for` 循环变量在循环外声明
2. 注释仅使用 `/* ... */`
3. 无混合声明与语句（块开头先声明）
4. 无 C99 头文件依赖（`stdint.h`/`stdbool.h`）
5. `types.h` 中提供固定宽度别名与尺寸检查

---

## 13. 结论与立即行动项

### 13.1 结论

N64-RDP 思路对视觉小说引擎是成立的，但必须以“可验证交付”推进，而不是以“理论性能”推进。本版已经将项目约束、接口契约、验证方法、回退路径和里程碑 DoD 明确化，能直接转入工程实施。

### 13.2 立即行动项（本周）

1. 冻结目录结构与公共头文件（`renderer.h/vm.h/pack.h`）
2. 完成 `vnpak` 打包器最小可用版本（图像 + 脚本）
3. 跑通 `M0` 演示脚本（至少 30 秒可交互流程）
4. 建立基准场景 S0-S3 与自动采样脚本

---

**文档版本**：v1.1-executable  
**日期**：2026-03-05  
**作者**：基于原始白皮书修订  
**许可**：CC BY-SA 4.0

---
