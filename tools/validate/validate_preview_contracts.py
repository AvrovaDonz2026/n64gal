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
        return error("tool.validate.preview_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_preview_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.preview_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        preview_doc = read_text(root, "docs/preview-protocol.md")
        preview_header = read_text(root, "include/vn_preview.h")
        preview_main = read_text(root, "src/tools/previewd_main.c")
        preview_test = read_text(root, "tests/integration/test_preview_protocol.c")
        toolchain = read_text(root, "docs/toolchain.md")
        readme = read_text(root, "README.md")
    except FileNotFoundError as exc:
        return error("tool.validate.preview_contracts.io", VN_E_IO, str(exc), "required preview contract file missing")
    except OSError:
        return error("tool.validate.preview_contracts.io", VN_E_IO, "root", "failed reading preview contract files")

    try:
        require_contains(preview_doc, "当前对外协议版本固定为 `v1`。", "preview_doc.version")
        require_contains(preview_doc, "`preview_protocol != v1` 必须被显式拒绝。", "preview_doc.reject_version")
        require_contains(preview_doc, "| `trace_id` | 稳定错误/成功归类 ID |", "preview_doc.trace_id")
        require_contains(preview_doc, "`inject_input` 在 `v1` 当前支持 `choice`、`key`、`trace_toggle`、`quit` 四类事件", "preview_doc.inject_input")
        require_contains(preview_doc, "`vn_previewd` 可执行程序", "preview_doc.previewd_entry")
        require_contains(preview_doc, "`vn_preview_run_cli(int argc, char** argv)`", "preview_doc.preview_cli_entry")

        require_contains(preview_header, "int vn_preview_run_cli(int argc, char** argv);", "preview_header.api")
        require_contains(preview_main, "return vn_preview_run_cli(argc, argv);", "preview_main.forward")

        require_contains(preview_test, 'preview_protocol=v1\\n', "preview_test.protocol")
        require_contains(preview_test, "preview.ok", "preview_test.trace_ok")
        require_contains(preview_test, "preview.request.unsupported", "preview_test.trace_err")
        require_contains(preview_test, "inject_input.key", "preview_test.inject_input_key")

        require_contains(toolchain, "probe-preview", "toolchain.preview_probe")
        require_contains(readme, "probe-preview", "readme.preview_probe")
    except ValueError as exc:
        return error("tool.validate.preview_contracts.format", VN_E_FORMAT, str(exc), "preview contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.preview_contracts.ok",
                f"root={root}",
                "preview_protocol=v1",
                "entry=vn_preview_run_cli",
                "tool_probe=probe-preview",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
