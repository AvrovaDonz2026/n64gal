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
        return error("tool.validate.save_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_save_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.save_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        save_doc = read_text(root, "docs/api/save.md")
        policy_doc = read_text(root, "docs/vnsave-version-policy.md")
        migration_doc = read_text(root, "docs/migration.md")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
        save_header = read_text(root, "include/vn_save.h")
        probe_tool = read_text(root, "tools/probe/vnsave_probe.c")
        migrate_tool = read_text(root, "tools/migrate/vnsave_migrate.c")
        save_test = read_text(root, "tests/unit/test_vnsave.c")
    except FileNotFoundError as exc:
        return error("tool.validate.save_contracts.io", VN_E_IO, str(exc), "required save contract file missing")
    except OSError:
        return error("tool.validate.save_contracts.io", VN_E_IO, "root", "failed reading save contract files")

    try:
        require_contains(save_doc, "`VNSAVE_VERSION_1`", "save_doc.version")
        require_contains(save_doc, "`VNSAVE_PUBLIC_SAVELOAD_SCOPE`", "save_doc.public_scope")
        require_contains(save_doc, "`VNSAVE_STATUS_PRE_1_0`", "save_doc.status_pre_1_0")
        require_contains(save_doc, "### `int vnsave_probe_file(const char* path, VNSaveProbe* out_probe)`", "save_doc.probe_api")
        require_contains(save_doc, "### `int vnsave_migrate_v0_to_v1_file(const char* in_path, const char* out_path)`", "save_doc.migrate_api")
        require_contains(save_doc, "### `const char* vnsave_status_name(vn_u32 status)`", "save_doc.status_name_api")
        require_contains(save_doc, "legacy `v0`", "save_doc.legacy_v0")
        require_contains(save_doc, "`v1`（当前正式目标格式）", "save_doc.v1_format")
        require_contains(save_doc, "runtime-specific session save/load", "save_doc.runtime_wrapper")
        require_contains(save_doc, "payload 在 `vn_save.h` 层仍按 opaque bytes 处理", "save_doc.payload_opaque")

        require_contains(policy_doc, "`v0.x` 阶段没有对外稳定 `vnsave` 兼容承诺", "policy.pre_1_0")
        require_contains(policy_doc, "`vnsave v1` 的公开承诺不早于 `v1.0.0`", "policy.v1_gate")
        require_contains(policy_doc, "`v1.0.0`：首次引入正式 `vnsave v1`", "policy.v1_summary")
        require_contains(policy_doc, "runtime-specific session persistence", "policy.runtime_specific")
        require_contains(policy_doc, "`runtime-session-only`", "policy.runtime_session_only")

        require_contains(migration_doc, "docs/api/save.md", "migration.save_doc")
        require_contains(migration_doc, "`v0 -> v1`", "migration.v0_to_v1")
        require_contains(migration_doc, "runtime-specific quick-save / quick-load", "migration.runtime_specific")

        require_contains(readme, "python3 tools/toolchain.py migrate-vnsave --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave", "readme.migrate_cmd")
        require_contains(readme, "./build/vnsave_migrate --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave", "readme.migrate_build_cmd")
        require_contains(readme, "python3 tools/toolchain.py probe-vnsave --in tests/fixtures/vnsave/v1/sample.vnsave", "readme.probe_cmd")
        require_contains(readme, "./build/vnsave_probe --in tests/fixtures/vnsave/v1/sample.vnsave", "readme.probe_build_cmd")
        require_contains(readme, "docs/api/save.md", "readme.save_doc")
        require_contains(readme, "docs/vnsave-version-policy.md", "readme.save_policy")

        require_contains(toolchain, "python3 tools/toolchain.py validate-save-contracts", "toolchain.validate_save")
        require_contains(toolchain, "python3 tools/toolchain.py probe-vnsave --in tests/fixtures/vnsave/v1/sample.vnsave", "toolchain.probe_vnsave")
        require_contains(toolchain, "python3 tools/toolchain.py migrate-vnsave --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave", "toolchain.migrate_vnsave")
        require_contains(toolchain, "./build/vnsave_probe --in tests/fixtures/vnsave/v1/sample.vnsave", "toolchain.probe_build")
        require_contains(toolchain, "./build/vnsave_migrate --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave", "toolchain.migrate_build")

        require_contains(save_header, "#define VNSAVE_VERSION_1 0x00010000u", "save_header.version")
        require_contains(save_header, '#define VNSAVE_PUBLIC_SAVELOAD_SCOPE "runtime-session-only"', "save_header.public_scope")
        require_contains(save_header, "#define VNSAVE_STATUS_PRE_1_0 3u", "save_header.status")
        require_contains(save_header, "int vnsave_probe_file(const char* path, VNSaveProbe* out_probe);", "save_header.probe_api")
        require_contains(save_header, "int vnsave_migrate_v0_to_v1_file(const char* in_path, const char* out_path);", "save_header.migrate_api")
        require_contains(save_header, "const char* vnsave_status_name(vn_u32 status);", "save_header.status_name")

        require_contains(probe_tool, "trace_id=tool.probe.vnsave.failed", "probe_tool.failed")
        require_contains(probe_tool, "trace_id=tool.probe.vnsave.ok", "probe_tool.ok")
        require_contains(migrate_tool, "trace_id=tool.vnsave_migrate.input.unsupported", "migrate_tool.unsupported")
        require_contains(migrate_tool, "trace_id=tool.vnsave_migrate.ok", "migrate_tool.ok")

        require_contains(save_test, "VNSAVE_STATUS_PRE_1_0", "save_test.pre_1_0")
        require_contains(save_test, "VNSAVE_STATUS_NEWER_VERSION", "save_test.newer_version")
        require_contains(save_test, "vnsave_status_name", "save_test.status_name")
    except ValueError as exc:
        return error("tool.validate.save_contracts.format", VN_E_FORMAT, str(exc), "save contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.save_contracts.ok",
                f"root={root}",
                "save_api=present",
                "save_policy=present",
                "probe_tool=present",
                "migrate_tool=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
