# Pack API (`vn_pack.h`)

## 1. 头文件

```c
#include "vn_pack.h"
```

## 2. 设计目标

1. 资源读取保持 C89 兼容。
2. 支持 `vnpak` 版本兼容读取（v1/v2）。
3. 在运行时做资源一致性校验，避免坏包静默运行。

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

### v1（兼容）

1. 头：8 字节（magic/version/count）
2. entry：14 字节（无 CRC）

### v2（当前默认）

1. 头：8 字节（magic/version/count）
2. entry：18 字节（追加 `crc32`）

## 6. 打包与 Manifest

默认打包命令：

```bash
./tools/packer/make_demo_pack.sh
```

产物：

1. `assets/demo/demo.vnpak`
2. `assets/demo/manifest.json`

`manifest.json` 用于离线回归与打包产物审计，不参与运行时读取流程。
