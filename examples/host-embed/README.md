# Host Embed Example

这个目录给出一个最小宿主集成示例，目标是直接通过 `vn_runtime.h` 驱动引擎，而不是依赖 `vn_player` 二进制。

## 文件

1. `session_loop.c`
   - 使用 `vn_runtime_session_create/step/is_done/set_choice/inject_input/destroy`
   - 演示宿主循环、分支注入与结果读取
2. `linux_tty_loop.c`
   - Linux TTY 包装层示例
   - 演示如何把非阻塞终端按键映射到 `VNInputEvent`
3. `windows_console_loop.c`
   - Windows Console 包装层示例
   - 演示如何把 `_kbhit/_getch` 输入映射到 `VNInputEvent`

## 本地编译（C89）

### 方式 A：直接 `cc`

```bash
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -Iinclude \
  examples/host-embed/session_loop.c \
  src/core/error.c \
  src/core/backend_registry.c \
  src/core/renderer.c \
  src/core/save.c \
  src/core/vm.c \
  src/core/pack.c \
  src/core/platform.c \
  src/core/runtime_cli.c \
  src/core/runtime_input.c \
  src/core/runtime_parse.c \
  src/core/runtime_persist.c \
  src/core/runtime_session_support.c \
  src/core/runtime_session_loop.c \
  src/core/dynamic_resolution.c \
  src/frontend/render_ops.c \
  src/frontend/dirty_tiles.c \
  src/backend/common/pixel_pipeline.c \
  src/backend/avx2/avx2_backend.c \
  src/backend/avx2/avx2_fill_fade.c \
  src/backend/avx2/avx2_textured.c \
  src/backend/neon/neon_backend.c \
  src/backend/rvv/rvv_backend.c \
  src/backend/scalar/scalar_backend.c \
  -o /tmp/n64gal_host_embed_example
```

### 方式 B：CMake / ctest（Linux）

```bash
cmake -S . -B build -DVN_BUILD_PLAYER=ON
cmake --build build --target example_host_embed
ctest --test-dir build --output-on-failure -R example_host_embed
```

### 方式 C：CMake / ctest（Windows x64 或 arm64）

```powershell
cmake -S . -B build -A x64 -DVN_BUILD_PLAYER=ON
cmake --build build --config Release --target example_host_embed
ctest --test-dir build -C Release --output-on-failure -R example_host_embed
```

## 运行前准备

```bash
./tools/scriptc/build_demo_scripts.sh
./tools/packer/make_demo_pack.sh
/tmp/n64gal_host_embed_example
```

## 宿主接入要点

1. 运行时推荐总是先调用 `vn_run_config_init`。
2. 宿主应自己维护主循环，不要假设 `vn_runtime_session_step` 会阻塞一整帧墙钟时间。
3. 分支选择通过 `vn_runtime_session_set_choice`、`vn_runtime_session_inject_input` 或 `choice_seq` 注入。
4. 默认包路径是 `assets/demo/demo.vnpak`；嵌入到外部项目时应显式设置 `pack_path`。

## CI

1. `example_host_embed` 已接入 `CMake + ctest`。
2. `example_host_embed_linux_tty` 与 `example_host_embed_windows_console` 也已接入构建。
3. 非目标平台会编译成 `skipped` stub，避免平台专用示例把主矩阵构建打坏。
