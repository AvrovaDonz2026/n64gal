#!/usr/bin/env python3
from pathlib import Path
import sys


VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3


def error(trace_id, error_code, field, message):
    error_name = {
        VN_E_INVALID_ARG: "VN_E_INVALID_ARG",
        VN_E_IO: "VN_E_IO",
        VN_E_FORMAT: "VN_E_FORMAT",
    }.get(error_code, "VN_E_UNKNOWN")
    parts = [f"trace_id={trace_id}", f"error_code={error_code}", f"error_name={error_name}"]
    if field:
        parts.append(f"field={field}")
    parts.append(f"message={message}")
    print(" ".join(parts), file=sys.stderr)
    return 1


def read_text(root: Path, rel: str):
    path = root / rel
    if not path.exists():
        raise FileNotFoundError(rel)
    return path.read_text(encoding="utf-8")


def require_contains(text: str, needle: str, field: str):
    if needle not in text:
        raise ValueError(field)


def main(argv):
    root = Path(".")
    if len(argv) > 2:
        return error("tool.validate.compat_matrix.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_compat_matrix.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.compat_matrix.io", VN_E_IO, "root", "root directory not found")

    try:
        compat = read_text(root, "docs/compat-matrix.md")
        roadmap = read_text(root, "docs/release-roadmap-1.0.0.md")
        checklist = read_text(root, "docs/release-checklist-v1.0.0.md")
        host_sdk = read_text(root, "docs/host-sdk.md")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
        issue = read_text(root, "issue.md")
    except FileNotFoundError as exc:
        return error("tool.validate.compat_matrix.io", VN_E_IO, str(exc), "required compatibility contract file missing")
    except OSError:
        return error("tool.validate.compat_matrix.io", VN_E_IO, "root", "failed reading compatibility contract files")

    try:
        require_contains(compat, "| `v1.0.0` 首版承诺 | 是 | 是 | 是 | 是 | 否 |", "compat.platform_row")
        require_contains(compat, "| `rvv` | `qemu-first` | 否 | 转 `post-1.0` |", "compat.backend_rvv")
        require_contains(compat, "| `avx2_asm` | force-only 实验 | 否 | 不进入 auto 优先级 |", "compat.backend_avx2_asm")
        require_contains(compat, "| `vnsave` | `format v1 stable; generic ABI not public` | 首次引入 `v1` |", "compat.vnsave")
        require_contains(compat, "| `preview protocol` | `v1` | 稳定 `v1` 基面 |", "compat.preview")
        require_contains(compat, "3. `RVV/riscv64 native`、`avx2_asm`、`JIT` 都不进入首个正式版默认承诺", "compat.current_conclusion")

        require_contains(roadmap, "`v1.0.0` **先不包含 RVV / riscv64 native 承诺**", "roadmap.no_rvv")
        require_contains(roadmap, "`riscv64/RVV` 转入 `post-1.0` 轨道", "roadmap.post_1_0")
        require_contains(roadmap, "`x64/arm64 + Linux/Windows`", "roadmap.platform_scope")

        require_contains(checklist, "`RVV/riscv64 native` 转入 `post-1.0`", "checklist.post_1_0")
        require_contains(checklist, "`compat-matrix.md` 与 `README` / release 文档口径一致", "checklist.compat_matrix")
        require_contains(checklist, "`1.0.0` / `post-1.0` 范围边界固定", "checklist.boundary")

        require_contains(host_sdk, "| `vnsave` | `format v1 stable; generic ABI not public` |", "host_sdk.vnsave")
        require_contains(host_sdk, "| `preview protocol` | `v1` |", "host_sdk.preview")
        require_contains(host_sdk, "| `backend abi` | `internal, not public ABI` |", "host_sdk.backend_abi")

        require_contains(readme, "docs/compat-matrix.md", "readme.compat_doc")
        require_contains(readme, "首个 `v1.0.0` 正式版当前只承诺前四项；`riscv64 + Linux` 继续保留在长期路线图中，但按 `post-1.0` 处理。", "readme.post_1_0_scope")

        require_contains(issue, "`M3-riscv64-rvv` 继续推进，但已从 `1.0.0` 阻塞项降为 `post-1.0` 轨道", "issue.post_1_0")
        require_contains(issue, "`v1.0.0` 当前明确先不包含 `RVV/riscv64 native` 承诺。", "issue.no_rvv")

        require_contains(toolchain, "python3 tools/toolchain.py validate-compat-matrix", "toolchain.validate_compat")
    except ValueError as exc:
        return error("tool.validate.compat_matrix.format", VN_E_FORMAT, str(exc), "compatibility matrix drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.compat_matrix.ok",
                f"root={root}",
                "v1_scope=x64_arm64_linux_windows",
                "rvv=post_1_0",
                "compat_matrix=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
