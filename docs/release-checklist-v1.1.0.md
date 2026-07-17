# Release Checklist: v1.1.0

## Product Slice

- [ ] 自定义项目可稳定构建入口场景、脚本目录和图片资源。
- [ ] 背景、交叉淡化和 8 个立绘层进入统一 `VNRenderOp[]` 链路。
- [ ] 真实纹理采样、缓存、frame reuse 与 op-cache invalidation 覆盖完整。
- [ ] preview 截图、frame view、build-info v2 与 snapshot v2 行为已文档化。

## Compatibility

- [ ] Runtime API 保持 `public stable v1`，新增结构都有 `struct_size/version`。
- [ ] `preview protocol v1`、`vnpak v2`、`vnsave v1` 外层版本保持不变。
- [ ] v1.0 save fixture 可读取；旧 `S0/S1/S2/S3/S10` golden 与性能门限无变化。
- [x] `RVV/riscv64 native` 延期，不进入 `v1.1.0` 正式承诺。

## Validation

- [ ] `python3 tools/toolchain.py validate-all`
- [ ] `python3 tools/toolchain.py release-gate --with-soak --with-export --content-soak-summary build_release_content_soak/demo_soak_summary.md --content-soak-summary-json build_release_content_soak/demo_soak_summary.json --release-spec docs/release-publish-v1.1.0.json`
- [ ] `BUILD_DIR=build_release_content_soak python3 tools/toolchain.py release-soak --pack assets/demo/content-demo.vnpak --scenes Opening,Gallery --scene-duration-sec 450 --backend scalar`
- [ ] `./scripts/check_c89.sh`
- [ ] `./scripts/ci/run_cc_suite.sh`
- [ ] `CC=clang BUILD_DIR=build_ci_sanitizers ./scripts/ci/run_linux_x64_sanitizers.sh`
- [ ] sanitizer 入口默认以 `16 ms/frame` 对 `Opening,Gallery` 各运行 `450` 秒模拟帧；缩短验证可显式传入 `--frames-per-scene <N>`。
- [ ] `build_ci_sanitizers/sanitizer_summary.md` 与 `build_ci_sanitizers/sanitizer_summary.json` 均为 `success`，且记录的峰值 RSS 不超过 `64 MiB`。
- [ ] 四个正式平台的 correctness、host、preview、backend-hit 和 perf gate 全绿。
- [ ] Linux x64 ASan/UBSan 与 15 分钟 soak 无错误，峰值 RSS 不超过 64 MiB。

## Release Materials

- [ ] `docs/release-publish-v1.1.0.json` 显式列出 note/evidence/package/checklist 与全部资产。
- [ ] `assets/demo/demo.vnpak` 与 `assets/demo/content-demo.vnpak` 均进入 bundle 和 publish map。
- [ ] legacy soak 与 `Opening/Gallery` 真实内容 soak 的摘要均进入 bundle manifest，且 SHA256/bytes 可核对。
- [ ] release evidence 已回填真实 CI run、摘要路径、CRC 与性能结果。
- [ ] 工作树干净，release preflight、bundle 和 audit 全绿。
- [ ] tag、GitHub Release 和远端上传只在以上项目完成后执行。
