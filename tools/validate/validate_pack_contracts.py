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
        return error("tool.validate.pack_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_pack_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.pack_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        pack_doc = read_text(root, "docs/api/pack.md")
        pack_header = read_text(root, "include/vn_pack.h")
        runtime_cli = read_text(root, "src/core/runtime_cli.c")
        pack_test = read_text(root, "tests/unit/test_vnpak.c")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
    except FileNotFoundError as exc:
        return error("tool.validate.pack_contracts.io", VN_E_IO, str(exc), "required pack contract file missing")
    except OSError:
        return error("tool.validate.pack_contracts.io", VN_E_IO, "root", "failed reading pack contract files")

    try:
        require_contains(pack_doc, "`vnpak` 是当前最明确进入版本语义的格式面之一", "pack_doc.version_surface")
        require_contains(pack_doc, "当前运行时兼容读取 `v1/v2`。", "pack_doc.v1_v2")
        require_contains(pack_doc, "### `int vnpak_open(VNPak* pak, const char* path)`", "pack_doc.open_api")
        require_contains(pack_doc, "### `int vnpak_read_resource(const VNPak* pak, vn_u32 id, vn_u8* out_buf, vn_u32 out_size, vn_u32* out_read)`", "pack_doc.read_api")
        require_contains(pack_doc, "`assets/demo/demo.vnpak`", "pack_doc.demo_pack")
        require_contains(pack_doc, "`assets/demo/manifest.json`", "pack_doc.manifest")

        require_contains(pack_header, "int vnpak_open(VNPak* pak, const char* path);", "pack_header.open_api")
        require_contains(pack_header, "const ResourceEntry* vnpak_get(const VNPak* pak, vn_u32 id);", "pack_header.get_api")
        require_contains(pack_header, "int vnpak_read_resource(const VNPak* pak, vn_u32 id, vn_u8* out_buf, vn_u32 out_size, vn_u32* out_read);", "pack_header.read_api")
        require_contains(pack_header, "void vnpak_close(VNPak* pak);", "pack_header.close_api")

        require_contains(runtime_cli, "vnpak_open(", "runtime_cli.open_pack")
        require_contains(runtime_cli, "vnpak_read_resource(", "runtime_cli.read_pack")
        require_contains(runtime_cli, "vnpak_close(", "runtime_cli.close_pack")

        require_contains(pack_test, "write_demo_pack_v1", "pack_test.v1")
        require_contains(pack_test, "write_demo_pack_v2", "pack_test.v2")
        require_contains(pack_test, "VN_E_NOMEM", "pack_test.nmem")
        require_contains(pack_test, "VN_E_FORMAT", "pack_test.format")

        require_contains(readme, "docs/api/pack.md", "readme.pack_doc")
        require_contains(toolchain, "python3 tools/toolchain.py validate-pack-contracts", "toolchain.validate_pack")
    except ValueError as exc:
        return error("tool.validate.pack_contracts.format", VN_E_FORMAT, str(exc), "pack contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.pack_contracts.ok",
                f"root={root}",
                "pack_api=present",
                "pack_doc=present",
                "pack_tests=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
