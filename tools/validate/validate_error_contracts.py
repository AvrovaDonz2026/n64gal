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
        return error("tool.validate.error_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_error_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.error_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        errors_doc = read_text(root, "docs/errors.md")
        error_header = read_text(root, "include/vn_error.h")
        error_impl = read_text(root, "src/core/error.c")
        error_test = read_text(root, "tests/unit/test_error_codes.c")
        preview_cli = read_text(root, "src/tools/preview_cli.c")
        runtime_parse = read_text(root, "src/core/runtime_parse.c")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
    except FileNotFoundError as exc:
        return error("tool.validate.error_contracts.io", VN_E_IO, str(exc), "required error contract file missing")
    except OSError:
        return error("tool.validate.error_contracts.io", VN_E_IO, "root", "failed reading error contract files")

    try:
        require_contains(errors_doc, "| `-1` | `VN_E_INVALID_ARG` |", "errors_doc.invalid_arg")
        require_contains(errors_doc, "| `-8` | `VN_E_AUDIO_DEVICE` |", "errors_doc.audio_device")
        require_contains(errors_doc, "const char* vn_error_name(int error_code);", "errors_doc.api")
        require_contains(errors_doc, "`trace_id`", "errors_doc.trace_id")
        require_contains(errors_doc, "`VN_E_UNKNOWN`", "errors_doc.unknown")

        require_contains(error_header, "#define VN_E_INVALID_ARG        -1", "error_header.invalid_arg")
        require_contains(error_header, "#define VN_E_AUDIO_DEVICE       -8", "error_header.audio_device")
        require_contains(error_header, "const char* vn_error_name(int error_code);", "error_header.api")

        require_contains(error_impl, 'return "VN_E_INVALID_ARG";', "error_impl.invalid_arg")
        require_contains(error_impl, 'return "VN_E_AUDIO_DEVICE";', "error_impl.audio_device")
        require_contains(error_impl, 'return "VN_E_UNKNOWN";', "error_impl.unknown")

        require_contains(error_test, 'expect_name(VN_E_INVALID_ARG, "VN_E_INVALID_ARG")', "error_test.invalid_arg")
        require_contains(error_test, 'expect_name(VN_E_AUDIO_DEVICE, "VN_E_AUDIO_DEVICE")', "error_test.audio_device")
        require_contains(error_test, 'expect_name(-999, "VN_E_UNKNOWN")', "error_test.unknown")

        require_contains(preview_cli, "vn_error_name(", "preview_cli.uses_public_error_name")
        require_contains(runtime_parse, "vn_error_name(", "runtime_parse.uses_public_error_name")

        require_contains(readme, "docs/errors.md", "readme.errors_doc")
        require_contains(toolchain, "python3 tools/toolchain.py validate-error-contracts", "toolchain.validate_error")
    except ValueError as exc:
        return error("tool.validate.error_contracts.format", VN_E_FORMAT, str(exc), "error contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.error_contracts.ok",
                f"root={root}",
                "error_api=present",
                "error_doc=present",
                "error_impl=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
