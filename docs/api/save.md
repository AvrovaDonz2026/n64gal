# Save API (`vn_save.h`)

## 1. 头文件

```c
#include "vn_save.h"
```

## 2. 设计目标

1. 在完整 save/load 子系统落地前，先固定最小公开 `vnsave` 探测面。
2. 把 `vnsave v1` 的 header/version 规则写成可执行 API。
3. 为 legacy `v0` 提供最小离线迁移路径。
4. 对未知、过新、损坏或 `pre-1.0` 存档给出结构化拒绝结果。
5. 保持 C89 兼容。

## 3. 当前版本承诺级别

`vn_save.h` 当前是 `pre-1.0` 的最小公开面：

1. 当前已经公开：
   - header/version 探测
   - 结构化拒绝
   - 最小 `v0 -> v1` 离线迁移函数
   - 基于 `vn_runtime.h` 的 runtime-specific session save/load draft wrapper
2. 当前仍未承诺：
   - 完整 save/load 运行时 ABI
   - 宿主侧长期存档兼容
   - 多版本自动迁移链
3. `v1.0.0` 前，宿主应把 `vn_save.h` 视为“探测/迁移工具接口”；runtime-specific session persistence 仍以 `vn_runtime.h` draft API 解释，而不是完整存档系统。

## 4. 常量与状态

### 版本与头大小

1. `VNSAVE_API_STABILITY`
   - 当前值：`pre-1.0 unstable`
2. `VNSAVE_VERSION_1`
   - 当前值：`0x00010000`
3. `VNSAVE_HEADER_SIZE_V0`
   - 当前值：`16`
4. `VNSAVE_HEADER_SIZE_V1`
   - 当前值：`32`

### 探测状态

1. `VNSAVE_STATUS_OK`
2. `VNSAVE_STATUS_BAD_MAGIC`
3. `VNSAVE_STATUS_TRUNCATED`
4. `VNSAVE_STATUS_PRE_1_0`
5. `VNSAVE_STATUS_NEWER_VERSION`
6. `VNSAVE_STATUS_INVALID_HEADER`

## 5. `VNSaveProbe`

字段：

1. `path`
2. `version`
3. `header_size`
4. `payload_size`
5. `slot_id`
6. `script_pc`
7. `scene_id`
8. `timestamp_s`
9. `payload_crc32`
10. `status`
11. `error_code`

规则：

1. `status` 提供结构化探测结果。
2. `error_code` 继续复用公共 `VN_*` 错误码。
3. 宿主应优先按 `error_code + status` 判断，而不是依赖文本字符串。
4. 若宿主需要在运行时读取当前 build 的 save 版本边界，也可通过 `vn_runtime_query_build_info(...)` 获取 `vnsave_latest_version + vnsave_api_stability`。

## 6. API

### `int vnsave_probe_file(const char* path, VNSaveProbe* out_probe)`

行为：

1. 读取并验证 `vnsave` header。
2. 支持识别：
   - 有效 `v1`
   - legacy `v0`
   - 更新版本
   - 魔数错误
   - 截断
   - 非法 header
3. 对未知/过新/损坏输入返回结构化错误，不做 best-effort 读取。

返回值：

1. `VN_OK`
2. `VN_E_IO`
3. `VN_E_FORMAT`
4. `VN_E_UNSUPPORTED`

### `int vnsave_migrate_v0_to_v1_file(const char* in_path, const char* out_path)`

行为：

1. 仅接受 legacy `v0` 输入。
2. 迁移时会：
   - 保留 `slot_id`
   - 保留 `script_pc`
   - 保留 `scene_id`
   - 原样拷贝 payload
   - 重写为 `v1` 头
   - 重新计算 `payload_crc32`
3. 当前为保证可重复输出，`timestamp_s` 固定写 `0`。
4. 若输入不是可迁移的 legacy `v0`，返回结构化错误。

### `const char* vnsave_status_name(vn_u32 status)`

提供稳定状态名，供日志、测试和工具输出复用。

## 7. 当前格式规则

### legacy `v0`（仅迁移输入）

当前仓库只把它当成 pre-1.0 迁移输入格式，不作为正式对外承诺：

```text
0x00 magic     "VNS0"
0x04 slot_id   u32
0x08 script_pc u32
0x0C scene_id  u32
0x10 payload   bytes...
```

### `v1`（当前正式目标格式）

当前对齐白皮书中的 `vnsave v1` 头：

```text
0x00 magic         "VNSV"
0x04 version       u32 = 0x00010000
0x08 slot_id       u32
0x0C script_pc     u32
0x10 scene_id      u32
0x14 timestamp_s   u32
0x18 payload_crc32 u32
0x1C reserved      u32 = 0
0x20 payload       bytes...
```

当前 `v1` probe 要求：

1. `magic == "VNSV"`
2. `version == VNSAVE_VERSION_1`
3. `reserved == 0`
4. `payload_crc32` 与实际 payload 一致

payload 说明：

1. 当前 `vnsave v1` payload 在 `vn_save.h` 层仍按 opaque bytes 处理
2. 当前仓库里 `vn_runtime_session_save_to_file(...)` 会把 runtime session snapshot draft 写进这个 payload
3. 这不等于“所有 `vnsave v1` 文件都必须是 runtime session dump”
4. 普通 `vnsave` 读者若不理解 payload 语义，仍应只按 header/version/CRC 解释

## 8. CLI 入口

当前最小迁移工具：

```bash
python3 tools/toolchain.py migrate-vnsave --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave
./build/vnsave_migrate --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave
```

说明：

1. 推荐优先走 `tools/toolchain.py`，会自动编译 helper
2. 若已经完成 CMake 构建，也可直接运行 `build/vnsave_migrate`
3. `tools/migrate/vnsave_migrate.c` 是源码入口，不是仓库内预置二进制

当前只承诺：

1. 接受 repo 内定义的 legacy `v0`
2. 产出 `v1`
3. 对其它输入给出结构化拒绝
4. runtime-specific 文件级 quick-save / quick-load 继续通过 `vn_runtime_session_save_to_file(...)` / `vn_runtime_session_load_from_file(...)` 暴露，而不是通过 `vn_save.h` 直接承诺完整 save/load ABI

## 9. 当前限制

1. 这不是完整 save/load API。
2. 这不是多版本迁移框架。
3. 当前只完成 `v0 -> v1` 最小路径。
4. 更复杂的历史存档兼容仍留给后续 `ISSUE-015` 收口。
