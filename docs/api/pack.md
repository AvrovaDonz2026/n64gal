# Pack API (`vn_pack.h`)

## 1. 头文件

```c
#include "vn_pack.h"
```

## 2. 设计目标

1. 资源读取保持 C89 兼容。
2. 支持 `vnpak` 版本兼容读取（v1/v2）。
3. 在运行时做资源一致性校验，避免坏包静默运行。
4. `v1.1.0` 在不升级 `vnpak v2` 的前提下追加场景目录和真实图片语义。

## 2.1 当前版本承诺级别

`vnpak` 是当前最明确进入版本语义的格式面之一：

1. 当前默认写出 `v2`。
2. 当前运行时兼容读取 `v1/v2`。
3. `v1.0.0` 必须明确写出实际支持的 `vnpak` 版本范围。
4. 未知、过新或损坏的包必须结构化拒绝，而不是 best-effort 读取。

## 3. 结构体

### `ResourceEntry`

字段：

1. `type`, `flags`
2. `width`, `height`
3. `data_off`, `data_size`
4. `crc32`

说明：

1. v1 包没有每资源 CRC，读取时 `crc32=0`。
2. v2 包包含每资源 CRC32，读取资源时会校验。

### `VNPak`

字段：

1. `path`
2. `version`
3. `resource_count`
4. `header_size`
5. `entry_size`
6. `file_size`
7. `entries`

## 4. API 函数

### `int vnpak_open(VNPak* pak, const char* path)`

打开资源包并加载索引表。

一致性校验（open 阶段）：

1. 魔数与版本合法。
2. 资源表不越界。
3. `data_off/data_size` 不越界且无溢出。
4. 资源数据区不得互相重叠。
5. 未声明兼容的版本必须返回格式错误。

### `const ResourceEntry* vnpak_get(const VNPak* pak, vn_u32 id)`

按资源 ID 获取表项。

### `int vnpak_read_resource(const VNPak* pak, vn_u32 id, vn_u8* out_buf, vn_u32 out_size, vn_u32* out_read)`

读取资源内容。

行为：

1. 缓冲区不足返回 `VN_E_NOMEM`。
2. v2 包会执行 CRC32 校验，校验失败返回 `VN_E_FORMAT`。

### `void vnpak_close(VNPak* pak)`

释放 `entries` 并清理状态。

## 5. `vnpak` 格式版本

当前公开常量：

1. `VNPAK_VERSION_1`
2. `VNPAK_VERSION_2`
3. `VNPAK_READ_MIN_VERSION`
4. `VNPAK_READ_MAX_VERSION`
5. `VNPAK_WRITE_DEFAULT_VERSION`

### v1（兼容）

1. 头：8 字节（magic/version/count）
2. entry：14 字节（无 CRC）

### v2（当前默认）

1. 头：8 字节（magic/version/count）
2. entry：18 字节（追加 `crc32`）

当前运行时版本协商约定：

1. `VNPAK_READ_MIN_VERSION = 1`
2. `VNPAK_READ_MAX_VERSION = 2`
3. `VNPAK_WRITE_DEFAULT_VERSION = 2`
4. 宿主和工具若不想只靠文档推断，也可通过 `vn_runtime_query_build_info(...)` 读取这组范围

## 6. 图像格式与 Flags

当前资源 type：

1. `VN_RESOURCE_TYPE_IMAGE = 1`
2. `VN_RESOURCE_TYPE_SCRIPT = 2`
3. `VN_RESOURCE_TYPE_SCENE_CATALOG = 3`

`ResourceEntry.flags` 低 4 位用于图像像素格式：

1. `1`: `RGBA16`
2. `2`: `CI8`
3. `3`: `IA8`

`type=2`（脚本资源）时 `flags=0`。

运行时会严格校验图片尺寸和 payload 大小，并在第一次进入当前 render op 集合时通过 vnpak CRC 读取到 32 MiB LRU 缓存。所有后端使用最近邻采样；图片自身 alpha 与 render op alpha 相乘后再混合。

工具与 runtime 共用以下硬边界：单个 pack 最多 4096 个资源，单张转换后图片 payload 不得超过 32 MiB。项目验证阶段会拒绝超限输入，不生成 runtime 无法打开的包。

同一帧 `VNRenderOp[]` 引用的唯一图片 working set 总量也不得超过 32 MiB。runtime 会在加载前整体预检，超限返回 `VN_E_NOMEM` 且不会留下部分 pinned 状态；内容项目应按“当前背景 + 过渡中的上一背景 + 活跃立绘层”计算峰值。

### `VNSC v1` 场景目录

内容项目在 pack 中追加且只追加一个 `type=3` 资源：

1. header：`"VNSC" + u16 version + u16 scene_count + u32 entry_scene_id`
2. entry：`u32 scene_id + u16 script_resource_id + u8 name_len + u8 reserved + name bytes`
3. 整数均为 little-endian，`reserved` 必须为 0
4. 场景名和 ID 必须唯一，script resource 必须存在且为 `type=2`
5. 没有 `VNSC` 的历史包继续走 legacy 场景映射

### `CI8` 资源载荷布局（当前约定）

1. 调色板：256 项 `RGBA16`（每项 2 字节，大端序）
2. 索引面：`width * height` 字节

即总大小为 `512 + width*height`。

## 7. 打包与 Manifest

默认打包命令：

```bash
./tools/packer/make_demo_pack.sh
```

产物：

1. `assets/demo/demo.vnpak`
2. `assets/demo/manifest.json`
3. `assets/demo/content-demo.vnpak`
4. 可选输入：`assets/demo/images/images.json`

`demo.vnpak` 保留 v1 回归资源布局；`content-demo.vnpak` 由 `assets/demo/content-project.json` 重建，包含自定义 `Opening/Gallery`、三种图片格式和 `VNSC` catalog。

`images.json` 示例字段：

1. `name`：资源名
2. `source`：相对 `images.json` 的 PNG 路径
3. `format`：`rgba16|ci8|ia8`

`manifest.json` 用于离线回归与打包产物审计，不参与运行时读取流程。
