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
        return error("tool.validate.api_index_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_api_index_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.api_index_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        api_index = read_text(root, "docs/api/README.md")
        runtime_doc = read_text(root, "docs/api/runtime.md")
        backend_doc = read_text(root, "docs/api/backend.md")
        pack_doc = read_text(root, "docs/api/pack.md")
        save_doc = read_text(root, "docs/api/save.md")
        compat_log = read_text(root, "docs/api/compat-log.md")
        dirty_doc = read_text(root, "docs/api/dirty-tile-draft.md")
        errors_doc = read_text(root, "docs/errors.md")
        preview_doc = read_text(root, "docs/preview-protocol.md")
        host_sdk = read_text(root, "docs/host-sdk.md")
        platform_doc = read_text(root, "docs/platform-matrix.md")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
        issue = read_text(root, "issue.md")
    except FileNotFoundError as exc:
        return error("tool.validate.api_index_contracts.io", VN_E_IO, str(exc), "required api index document missing")
    except OSError:
        return error("tool.validate.api_index_contracts.io", VN_E_IO, "root", "failed reading api index documents")

    try:
        require_contains(api_index, "`runtime.md`", "api_index.runtime")
        require_contains(api_index, "`backend.md`", "api_index.backend")
        require_contains(api_index, "`pack.md`", "api_index.pack")
        require_contains(api_index, "`save.md`", "api_index.save")
        require_contains(api_index, "`../errors.md`", "api_index.errors")
        require_contains(api_index, "`dirty-tile-draft.md`", "api_index.dirty")
        require_contains(api_index, "`../preview-protocol.md`", "api_index.preview")
        require_contains(api_index, "`../host-sdk.md`", "api_index.host_sdk")
        require_contains(api_index, "`../platform-matrix.md`", "api_index.platform")
        require_contains(api_index, "`compat-log.md`", "api_index.compat_log")
        require_contains(api_index, "`scripts/check_api_docs_sync.sh`", "api_index.sync_gate")
        require_contains(api_index, "`compat-log.md`", "api_index.compat_log_rule")

        require_contains(runtime_doc, "## 1. 头文件", "runtime_doc.exists")
        require_contains(backend_doc, "## 1. 目标", "backend_doc.exists")
        require_contains(pack_doc, "## 1. 头文件", "pack_doc.exists")
        require_contains(save_doc, "## 1. 头文件", "save_doc.exists")
        require_contains(errors_doc, "## 1. 目标", "errors_doc.exists")
        require_contains(dirty_doc, "Dirty-Tile", "dirty_doc.exists")
        require_contains(preview_doc, "preview protocol", "preview_doc.exists")
        require_contains(host_sdk, "## Public Surface", "host_sdk.exists")
        require_contains(platform_doc, "## 目标平台", "platform_doc.exists")
        require_contains(compat_log, "compat", "compat_log.exists")

        require_contains(readme, "[`docs/api/README.md`](./docs/api/README.md)", "readme.api_index_link")
        require_contains(toolchain, "python3 tools/toolchain.py validate-api-index-contracts", "toolchain.validate_api_index")
        require_contains(issue, "`docs/api/README.md`", "issue.api_index")
    except ValueError as exc:
        return error("tool.validate.api_index_contracts.format", VN_E_FORMAT, str(exc), "api index contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.api_index_contracts.ok",
                f"root={root}",
                "api_index=present",
                "api_docs=linked",
                "compat_log=linked",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
