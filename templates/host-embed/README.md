# Host Embed Template

这个模板给宿主接入方提供最小 Session API 工程骨架。

## 目录

1. `template.json`
   - 模板版本与入口说明
2. `src/session_loop.c`
   - 最小宿主循环
3. `src/linux_tty_loop.c`
   - Linux TTY 输入包装层
4. `src/windows_console_loop.c`
   - Windows Console 输入包装层

## 快速开始

先生成最小 pack：

```bash
./templates/minimal-vn/tools/build_assets.sh
```

再在仓库根目录执行：

```bash
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -Iinclude \
  templates/host-embed/src/session_loop.c \
  src/core/error.c \
  src/core/backend_registry.c \
  src/core/renderer.c \
  src/core/save.c \
  src/core/vm.c \
  src/core/pack.c \
  src/core/platform.c \
  src/core/runtime_cli.c \
  src/core/runtime_persist.c \
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
  -o /tmp/host_embed_template
/tmp/host_embed_template
```
