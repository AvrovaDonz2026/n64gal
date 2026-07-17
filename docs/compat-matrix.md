# Compatibility Matrix

## 1. 目标

这份文档把 N64GAL 当前对外可见的兼容面收口成统一矩阵，避免 `README`、release note、host SDK、migration 文档分别给出不同强度的承诺。

## 2. 平台兼容矩阵

| Scope | Linux x64 | Windows x64 | Linux arm64 | Windows arm64 | Linux riscv64 |
|---|---|---|---|---|---|
| `v0.1.0-alpha` | 已验证 | 已验证 | 已验证 | 已验证 | `qemu-first` |
| `v1.0.0` 首版承诺 | 是 | 是 | 是 | 是 | 否 |
| `v1.1.0` 正式承诺 | 是 | 是 | 是 | 是 | 否 |
| 后续版本 | 继续维护 | 继续维护 | 继续维护 | 继续维护 | 等待原生设备证据 |

说明：

1. `Linux riscv64` 当前继续开发，但不进入首个 `v1.0.0` 正式版承诺。
2. `avx2_asm` 不构成单独平台承诺，它仍是 force-only 实验后端。
3. `v1.1.0` 不把 post-1.0 自动解释为 RVV native；在没有设备前只维护 cross/QEMU correctness。

## 3. 后端兼容矩阵

| Backend | 当前状态 | `v1.0.0/v1.1.0` 承诺 | 备注 |
|---|---|---|---|
| `scalar` | 稳定基线 | 是 | 所有平台回退基线 |
| `avx2` | x64 主线 | 是 | `x64` 默认优先后端 |
| `neon` | arm64 主线 | 是 | `arm64` 默认优先后端 |
| `rvv` | `qemu-first` | 否 | 转 `post-1.0` |
| `avx2_asm` | force-only 实验 | 否 | 不进入 auto 优先级 |

`v1.1.0` 的真实资源纹理先共用参考采样语义；AVX2/NEON 保留现有 clear/fade 优化。RVV 不新增 native 发布承诺。

## 4. 格式与接口兼容矩阵

| Surface | 当前状态 | `v1.x` 承诺 | 当前规则 |
|---|---|---|---|
| `runtime api` | `public stable v1` | 稳定公开面 | 已文档化接口冻结，后续仅允许兼容追加 |
| `backend abi` | `internal, not public ABI` | 继续内部化 | 不对宿主承诺私有 ABI |
| `error codes` | 公开 `VN_*` + `vn_error_name(int)` | 稳定基础错误语义 | 新错误码可追加，已公开语义不应偷偷改写 |
| `script bytecode` | `v1` | 继续声明式兼容 | 只保证读取已声明兼容版本 |
| `vnpak` | 默认写 `v2`，读 `v1/v2` | 明确固定读写边界 | 当前默认输出 `v2` |
| `vnsave` | `format v1 stable; generic ABI not public` | 首次引入 `v1` | 当前已有 probe/migrate 与 runtime-specific session persistence；仍没有冻结的通用 save/load 承诺 |
| `preview protocol` | `v1` | 稳定 `v1` 基面 | 后续仅追加字段 |
| `tool manifest` | `planned v1` | 固定最小 manifest 面 | 当前以 `extension-manifest.md` 约束字段 |

`v1.1.0` 对这些 surface 的策略是兼容追加，而不是升级主版本：Runtime API v1、Preview Protocol v1、`vnpak v2` 默认写入格式和 `vnsave v1` 外层格式均保持不变。

## 5. 版本承诺级别

| Level | 含义 |
|---|---|
| `已验证` | 已有 CI / 测试 / artifact 证据 |
| `首版承诺` | 计划纳入首个 `v1.0.0` 正式版范围 |
| `post-1.0` | 继续开发，但不阻塞首个正式版 |
| `实验` | 可强制启用，但不纳入默认发布承诺 |

## 6. 当前结论

当前项目的对外兼容边界应按以下原则解读：

1. `v0.1.0-alpha` 重点是“可运行、可验证、可引用证据”，不是长期 ABI 冻结
2. `v1.0.0` 首版优先收口 `x64/arm64 + Linux/Windows`
3. `RVV/riscv64 native`、`avx2_asm`、`JIT` 都不进入首个正式版默认承诺
4. `vnsave` 从 `v1.0.0` 才开始进入正式格式语义
5. `v1.1.0` 在相同版本边界内加入真实内容渲染切片；四个正式平台不变，RVV native 继续延期
