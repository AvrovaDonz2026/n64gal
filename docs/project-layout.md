# Project Layout

## 1. 目标

这份文档定义建议的新项目目录结构，避免新用户从仓库源码里手抄路径。

## 2. 内容项目建议布局

```text
project/
  assets/
    scripts/
      S0.vns.txt
    images/
      images.json
      *.png
  build/
    scripts/
      S0.vns.bin
    *.vnpak
    manifest.json
  template.json
```

当前建议：

1. 脚本源码放在 `assets/scripts/*.vns.txt`
2. 编译产物统一放到 `build/scripts/*.vns.bin`
3. 图片输入和 manifest 放在 `assets/images/`
4. pack 产物统一落到 `build/`

关键目录锚点：

1. `assets/scripts/`
2. `build/scripts/`
3. `build/`

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
