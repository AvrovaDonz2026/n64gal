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


def require_file(root: Path, rel: str):
    if not (root / rel).exists():
        raise FileNotFoundError(rel)


def main(argv):
    root = Path(".")
    if len(argv) > 2:
        return error("tool.validate.host_sdk_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_host_sdk_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.host_sdk_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        host_sdk = read_text(root, "docs/host-sdk.md")
        compat = read_text(root, "docs/compat-matrix.md")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
        example_readme = read_text(root, "examples/host-embed/README.md")
        require_file(root, "examples/host-embed/session_loop.c")
        require_file(root, "examples/host-embed/linux_tty_loop.c")
        require_file(root, "examples/host-embed/windows_console_loop.c")
        require_file(root, "examples/host-embed/README.md")
    except FileNotFoundError as exc:
        return error("tool.validate.host_sdk_contracts.io", VN_E_IO, str(exc), "required host sdk artifact missing")
    except OSError:
        return error("tool.validate.host_sdk_contracts.io", VN_E_IO, "root", "failed reading host sdk docs")

    try:
        require_contains(host_sdk, "1. `vn_runtime.h`", "host_sdk.public_surface.runtime")
        require_contains(host_sdk, "2. `vn_error.h`", "host_sdk.public_surface.error")
        require_contains(host_sdk, "3. `vn_types.h`", "host_sdk.public_surface.types")
        require_contains(host_sdk, "| `runtime api` | `public v1-draft (pre-1.0)` |", "host_sdk.runtime_api")
        require_contains(host_sdk, "| `backend abi` | `runtime-internal v1-draft` |", "host_sdk.backend_abi")
        require_contains(host_sdk, "| `vnsave` | `pre-1.0 unstable` |", "host_sdk.vnsave")
        require_contains(host_sdk, "| `preview protocol` | `v1` |", "host_sdk.preview_protocol")
        require_contains(host_sdk, "vn_runtime_query_build_info(...)", "host_sdk.build_info_query")
        require_contains(host_sdk, "vn_runtime_session_capture_snapshot", "host_sdk.snapshot_capture")
        require_contains(host_sdk, "vn_runtime_session_create_from_snapshot", "host_sdk.snapshot_restore")
        require_contains(host_sdk, "vn_runtime_session_save_to_file", "host_sdk.snapshot_save_file")
        require_contains(host_sdk, "vn_runtime_session_load_from_file", "host_sdk.snapshot_load_file")
        require_contains(host_sdk, "runtime-session-only", "host_sdk.runtime_session_only")
        require_contains(host_sdk, "Linux TTY:", "host_sdk.example_linux")
        require_contains(host_sdk, "Windows Console:", "host_sdk.example_windows")
        require_contains(host_sdk, "trace_id + error_code + error_name", "host_sdk.machine_readable")

        require_contains(compat, "| `runtime api` | `public v1-draft (pre-1.0)` |", "compat.runtime_api")
        require_contains(compat, "| `backend abi` | `runtime-internal v1-draft` |", "compat.backend_abi")
        require_contains(compat, "| `vnsave` | `pre-1.0 unstable` |", "compat.vnsave")
        require_contains(compat, "| `preview protocol` | `v1` |", "compat.preview_protocol")

        require_contains(readme, "tools/toolchain.py validate-host-sdk-contracts", "readme.toolchain_validate_host_sdk")
        require_contains(toolchain, "python3 tools/toolchain.py validate-host-sdk-contracts", "toolchain.validate_host_sdk")

        require_contains(example_readme, "src/core/platform.c", "example_readme.platform")
        require_contains(example_readme, "src/core/dynamic_resolution.c", "example_readme.dynamic_resolution")
        require_contains(example_readme, "src/frontend/dirty_tiles.c", "example_readme.dirty_tiles")
        require_contains(example_readme, "src/backend/avx2/avx2_fill_fade.c", "example_readme.avx2_fill_fade")
        require_contains(example_readme, "src/backend/avx2/avx2_textured.c", "example_readme.avx2_textured")
    except ValueError as exc:
        return error("tool.validate.host_sdk_contracts.format", VN_E_FORMAT, str(exc), "host sdk contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.host_sdk_contracts.ok",
                f"root={root}",
                "runtime_api=v1-draft",
                "backend_abi=runtime-internal",
                "preview_protocol=v1",
                "vnsave=pre-1.0-unstable",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
