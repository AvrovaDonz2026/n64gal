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
        return error("tool.validate.platform_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_platform_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.platform_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        platform_doc = read_text(root, "docs/platform-matrix.md")
        host_sdk = read_text(root, "docs/host-sdk.md")
        api_index = read_text(root, "docs/api/README.md")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
        issue = read_text(root, "issue.md")
    except FileNotFoundError as exc:
        return error("tool.validate.platform_contracts.io", VN_E_IO, str(exc), "required platform contract file missing")
    except OSError:
        return error("tool.validate.platform_contracts.io", VN_E_IO, "root", "failed reading platform contract files")

    try:
        require_contains(platform_doc, "| `amd64` | Linux | `avx2 -> scalar` |", "platform_doc.linux_x64")
        require_contains(platform_doc, "| `amd64` | Windows | `avx2 -> scalar` |", "platform_doc.windows_x64")
        require_contains(platform_doc, "| `arm64` | Linux | `neon -> scalar` |", "platform_doc.linux_arm64")
        require_contains(platform_doc, "| `arm64` | Windows | `neon -> scalar` |", "platform_doc.windows_arm64")
        require_contains(platform_doc, "| `riscv64` | Linux | `rvv -> scalar` |", "platform_doc.riscv64")
        require_contains(platform_doc, "`scripts/ci/run_cc_suite.sh`", "platform_doc.cc_suite")
        require_contains(platform_doc, "`scripts/ci/run_windows_suite.ps1 -PlatformLabel windows-x64 -CMakePlatform x64`", "platform_doc.windows_suite")
        require_contains(platform_doc, "`scripts/ci/build_riscv64_cross.sh`", "platform_doc.riscv_cross")
        require_contains(platform_doc, "`scripts/ci/run_riscv64_qemu_suite.sh`", "platform_doc.riscv_qemu")
        require_contains(platform_doc, "`docs/host-sdk.md`", "platform_doc.host_sdk_link")
        require_contains(platform_doc, "`docs/api/runtime.md`", "platform_doc.runtime_link")

        require_contains(host_sdk, "[`docs/platform-matrix.md`](./platform-matrix.md)", "host_sdk.platform_link")
        require_contains(host_sdk, "| `vnpak` | `v2` 当前默认，兼容读取 `v1` |", "host_sdk.vnpak")
        require_contains(host_sdk, "| `preview protocol` | `v1` |", "host_sdk.preview")

        require_contains(api_index, "`../platform-matrix.md`", "api_index.platform_link")
        require_contains(api_index, "Linux/Windows/riscv64 平台矩阵", "api_index.platform_desc")

        require_contains(readme, "[`docs/platform-matrix.md`](./docs/platform-matrix.md)", "readme.platform_link")
        require_contains(readme, "首个 `v1.0.0` 正式版当前只承诺前四项；`riscv64 + Linux` 继续保留在长期路线图中，但按 `post-1.0` 处理。", "readme.post_1_0")

        require_contains(issue, "`docs/platform-matrix.md`", "issue.platform_doc")
        require_contains(issue, "`x64/arm64 + Linux/Windows`", "issue.v1_scope")

        require_contains(toolchain, "python3 tools/toolchain.py validate-platform-contracts", "toolchain.validate_platform")
    except ValueError as exc:
        return error("tool.validate.platform_contracts.format", VN_E_FORMAT, str(exc), "platform contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.platform_contracts.ok",
                f"root={root}",
                "platform_matrix=present",
                "host_sdk_linked=present",
                "v1_scope=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
