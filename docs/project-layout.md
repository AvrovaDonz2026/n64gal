# Project Layout

## 1. 目标

这份文档定义建议的新项目目录结构，避免新用户从仓库源码里手抄路径。

## 2. 内容项目建议布局

```text
project/
  assets/
    scripts/
      Opening.vns.txt
    images/
      images.json
      *.png
  build/
    scripts/
      Opening.vns.bin
    *.vnpak
    manifest.json
    resource-map.json
  template.json
```

当前建议：

1. 脚本源码放在 `assets/scripts/*.vns.txt`
2. 编译产物统一放到 `build/scripts/*.vns.bin`
3. 图片输入和 manifest 放在 `assets/images/`
4. pack、resource map 和审计 manifest 统一落到 `build/`

关键目录锚点：

1. `assets/scripts/`
2. `build/scripts/`
3. `build/`

`template.json` v2 的内容项目字段为：

```json
{
  "template_version": 2,
  "kind": "content-project",
  "entry_scene": "Opening",
  "scenes": [
    {"name": "Opening", "script": "assets/scripts/Opening.vns.txt"}
  ],
  "images_manifest": "assets/images/images.json",
  "pack_output": "build/game.vnpak"
}
```

场景名必须匹配 ASCII `[A-Za-z][A-Za-z0-9_-]{0,62}`，单个项目最多声明 256 个场景。`S0/S1/S2/S3/S10` 继续使用场景 ID `0/1/2/3/10`；其他名称使用 FNV-1a 32 位 ID，构建时拒绝任何冲突。资源 ID 先按 `scenes[]`、再按 `images.json` 顺序分配，最后一个资源固定为 type `3` 的 `VNSC v1` catalog。

单个编译后脚本最多 65535 字节，以匹配 VM 的 16 位跳转目标和 call stack 返回地址；超限会在 validate/build 阶段拒绝。

单张图片转换后不得超过 32 MiB；单帧同时引用的唯一图片总量也必须控制在 32 MiB 内。后者取决于脚本运行状态，预算时应包含背景交叉淡化的两张背景和所有活跃立绘层。

脚本新增两条兼容指令：`BG <image|id> <duration_ms>` 和 `SPRITE <layer> <image|id|none> <x> <y>`。图片名称来自 `images.json`；原有纯数字参数与旧脚本编译命令保持兼容。

## 3. 宿主项目建议布局

```text
host-project/
  src/
    session_loop.c
    linux_tty_loop.c
    windows_console_loop.c
  template.json
```

当前建议：

1. 最小宿主循环放在 `src/session_loop.c`
2. 平台包装层按 OS 分文件
3. Pack 路径在宿主配置里显式给出，不依赖临时目录

## 4. 当前模板入口

1. `templates/minimal-vn/`
2. `templates/host-embed/`
