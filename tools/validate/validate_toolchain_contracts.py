#!/usr/bin/env python3
import re
from pathlib import Path
import sys


VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3

COMMAND_RE = re.compile(r'^\s+"  ([a-z0-9-]+)(?: .*)?",?$')
README_COMMANDS = {
    "validate-all",
    "probe-vnsave",
    "migrate-vnsave",
}


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


def extract_toolchain_commands(toolchain_py: str):
    commands = []
    for line in toolchain_py.splitlines():
        match = COMMAND_RE.match(line)
        if match:
            commands.append(match.group(1))
    return commands


def main(argv):
    root = Path(".")
    if len(argv) > 2:
        return error("tool.validate.toolchain_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_toolchain_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.toolchain_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        toolchain_py = read_text(root, "tools/toolchain.py")
        toolchain_md = read_text(root, "docs/toolchain.md")
        readme = read_text(root, "README.md")
        toolchain_test = read_text(root, "tests/integration/test_toolchain_cli.py")
    except FileNotFoundError as exc:
        return error("tool.validate.toolchain_contracts.io", VN_E_IO, str(exc), "required toolchain file missing")
    except OSError:
        return error("tool.validate.toolchain_contracts.io", VN_E_IO, "root", "failed reading toolchain files")

    commands = extract_toolchain_commands(toolchain_py)
    if not commands:
        return error("tool.validate.toolchain_contracts.format", VN_E_FORMAT, "tools/toolchain.py", "failed to extract commands")

    for command in commands:
        quoted = f'python3 tools/toolchain.py {command}'
        if quoted not in toolchain_md:
            return error("tool.validate.toolchain_contracts.format", VN_E_FORMAT, "docs/toolchain.md", f"missing command example: {command}")
        if command in README_COMMANDS and quoted not in readme:
            return error("tool.validate.toolchain_contracts.format", VN_E_FORMAT, "README.md", f"missing command example: {command}")
        if f'["{command}"' not in toolchain_test:
            return error("tool.validate.toolchain_contracts.format", VN_E_FORMAT, "tests/integration/test_toolchain_cli.py", f"missing toolchain CLI coverage: {command}")

    print(
        " ".join(
            [
                "trace_id=tool.validate.toolchain_contracts.ok",
                f"root={root}",
                f"command_count={len(commands)}",
                f"commands={','.join(commands)}",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
