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
        return error("tool.validate.migration_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_migration_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.migration_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        migration = read_text(root, "docs/migration.md")
        save_doc = read_text(root, "docs/api/save.md")
        save_policy = read_text(root, "docs/vnsave-version-policy.md")
        release_alpha = read_text(root, "docs/release-v0.1.0-alpha.md")
        release_gap = read_text(root, "docs/release-gap-v0.1.0-mvp.md")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
        issue = read_text(root, "issue.md")
    except FileNotFoundError as exc:
        return error("tool.validate.migration_contracts.io", VN_E_IO, str(exc), "required migration contract file missing")
    except OSError:
        return error("tool.validate.migration_contracts.io", VN_E_IO, "root", "failed reading migration contract files")

    try:
        require_contains(migration, "当前已有最小 `v0 -> v1` 迁移命令与 probe/reject 规则", "migration.minimal_migrate")
        require_contains(migration, "当前 `vn_save.h` 只解决“识别/拒绝/最小迁移”，不等于完整 save/load", "migration.save_scope")
        require_contains(migration, "`vnsave` 迁移不在 `v0.1.0-alpha` 范围内", "migration.alpha_scope")
        require_contains(migration, "`v1.0.0`：", "migration.v1_scope")
        require_contains(migration, "最小 probe API 见 [`docs/api/save.md`](./api/save.md)", "migration.save_doc_link")

        require_contains(save_doc, "当前只完成 `v0 -> v1` 最小路径。", "save_doc.minimal_path")
        require_contains(save_policy, "`v0 -> v1` 迁移命令", "save_policy.migrate_command")

        require_contains(release_alpha, "`v0.1.0-alpha`", "release_alpha.present")
        require_contains(release_alpha, "`JIT`；当前仍是文档化实验方向，不是 release blocker。", "release_alpha.jit_boundary")
        require_contains(release_gap, "把 `vnsave` 版本策略继续从“文档规则”推进到“实现与错误契约”", "release_gap.vnsave_path")

        require_contains(readme, "./tools/migrate/vnsave_migrate --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave", "readme.migrate_cmd")
        require_contains(toolchain, "python3 tools/toolchain.py migrate-vnsave --in tests/fixtures/vnsave/v0/sample.vnsave --out /tmp/sample.v1.vnsave", "toolchain.migrate_cmd")
        require_contains(toolchain, "python3 tools/toolchain.py validate-migration-contracts", "toolchain.validate_migration")

        require_contains(issue, "提供 `vnsave v0 -> v1` 迁移命令", "issue.migrate_done")
        require_contains(issue, "release 文档附带迁移说明", "issue.release_migration")
    except ValueError as exc:
        return error("tool.validate.migration_contracts.format", VN_E_FORMAT, str(exc), "migration contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.migration_contracts.ok",
                f"root={root}",
                "migration_doc=present",
                "minimal_v0_to_v1=present",
                "alpha_boundary=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
