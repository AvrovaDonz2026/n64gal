# Minimal VN Template

这个模板给内容项目提供最小可运行骨架。

## 目录

1. `template.json`
   - 模板版本与输出约定
2. `assets/scripts/`
   - 场景脚本源码
3. `assets/images/`
   - 打包输入图片与 `images.json`
4. `tools/build_assets.sh`
   - 单命令校验、编译脚本并生成 `.vnpak`
5. `build/`
   - 模板打包输出与编译脚本产物

## 快速开始

在仓库根目录执行：

```bash
./templates/minimal-vn/tools/build_assets.sh
cmake -S . -B build -DVN_BUILD_PLAYER=ON
cmake --build build
./build/vn_player --pack "$(pwd)/templates/minimal-vn/build/minimal.vnpak" --scene Opening --backend auto --frames 120 --dt-ms 16
```

## 当前约定

1. `template.json` v2 的 `entry_scene` 指向自定义 `Opening` 场景，场景使用真实图片资源的 `BG` 与 `SPRITE` 指令。
2. 打包输出固定写到 `templates/minimal-vn/build/minimal.vnpak`。
3. 脚本编译产物固定写到 `templates/minimal-vn/build/scripts/*.vns.bin`。
4. 构建同时写出 `build/resource-map.json` 和要求最低 runtime `v1.1.0` 的 `build/manifest.json`。
5. 模板不依赖仓库外临时路径，所有中间和最终产物都写回模板自己的 `build/` 目录。
