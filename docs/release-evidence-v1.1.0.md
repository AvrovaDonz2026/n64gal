# Release Evidence: v1.1.0

## Status

`v1.1.0` 尚未发布。本文件定义必须回填的证据，不把计划状态表述为已通过。

## Required Evidence

1. 四个正式平台：Linux x64、Windows x64、Linux arm64、Windows arm64。
2. scalar/AVX2/NEON 真实纹理 golden，以及旧 `S0/S1/S2/S3/S10` golden 无变化。
3. project validate/build、preview 截图 CRC、host embed、save compatibility 和 15 分钟 soak。
   - save compatibility 必须包含 `tests/fixtures/vnsave/v1/runtime-v1.0.0-s3.vnsave`，该文件由 `v1.0.0` tag 生成并冻结。
   - soak 同时保留 legacy 五场景，并运行 `content-demo.vnpak` 的 `Opening,Gallery` 共 900 秒模拟时间。
4. Linux x64 ASan/UBSan 与四平台现有 hard perf gate。
5. `ci-matrix`、release gate、bundle、report、publish map 和 remote summary 的可引用结果。

## Required Assets

1. Legacy benchmark demo: `assets/demo/demo.vnpak`
2. Real-content demo: `assets/demo/content-demo.vnpak`

## Deferred Evidence

`RVV/riscv64 native` 不属于本版本正式证据范围。现有 cross/QEMU correctness 继续维护，但不替代原生设备性能证据。
