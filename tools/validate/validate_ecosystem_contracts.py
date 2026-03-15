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
        return error("tool.validate.ecosystem_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_ecosystem_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.ecosystem_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        manifest_doc = read_text(root, "docs/extension-manifest.md")
        governance_doc = read_text(root, "docs/ecosystem-governance.md")
        compat_doc = read_text(root, "docs/compat-matrix.md")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
        issue = read_text(root, "issue.md")
        validator = read_text(root, "tools/validate/validate_manifest.py")
        valid_manifest = read_text(root, "tests/fixtures/tool_manifest/valid/vnsave_migrate.json")
        invalid_manifest = read_text(root, "tests/fixtures/tool_manifest/invalid/bad_kind.json")
    except FileNotFoundError as exc:
        return error("tool.validate.ecosystem_contracts.io", VN_E_IO, str(exc), "required ecosystem contract file missing")
    except OSError:
        return error("tool.validate.ecosystem_contracts.io", VN_E_IO, "root", "failed reading ecosystem contract files")

    try:
        require_contains(manifest_doc, "| `manifest_version` | yes |", "manifest_doc.version")
        require_contains(manifest_doc, "| `kind` | yes | `importer/exporter/validator/migrator` |", "manifest_doc.kind")
        require_contains(manifest_doc, "| `stability` | yes | `experimental/preview/stable` |", "manifest_doc.stability")
        require_contains(manifest_doc, "`runtime api`", "manifest_doc.runtime_api")
        require_contains(manifest_doc, "`tool manifest`", "manifest_doc.tool_manifest")
        require_contains(manifest_doc, "`read.vnsave`", "manifest_doc.read_vnsave")
        require_contains(manifest_doc, "`migrate.vnsave`", "manifest_doc.migrate_vnsave")
        require_contains(manifest_doc, "当前不承诺：", "manifest_doc.non_goals")

        require_contains(governance_doc, "1. 先文件级扩展，后插件 ABI", "governance_doc.file_first")
        require_contains(governance_doc, "2. 先工具进程扩展，后运行时主进程扩展", "governance_doc.process_first")
        require_contains(governance_doc, "格式变更必须绑定迁移策略或拒绝加载理由", "governance_doc.migration_rule")
        require_contains(governance_doc, "未声明 manifest 的第三方扩展", "governance_doc.manifest_required")
        require_contains(governance_doc, "不允许“尽力读取但语义未定义”", "governance_doc.no_best_effort")

        require_contains(compat_doc, "| `tool manifest` | `planned v1` | 固定最小 manifest 面 |", "compat_doc.tool_manifest")

        require_contains(validator, '"importer"', "validator.kind_importer")
        require_contains(validator, '"exporter"', "validator.kind_exporter")
        require_contains(validator, '"validator"', "validator.kind_validator")
        require_contains(validator, '"migrator"', "validator.kind_migrator")
        require_contains(validator, '"tool_manifest"', "validator.tool_manifest_key")
        require_contains(validator, '"emit.report"', "validator.emit_report")

        require_contains(valid_manifest, '"kind": "migrator"', "valid_manifest.kind")
        require_contains(valid_manifest, '"tool_manifest": "v1"', "valid_manifest.tool_manifest")
        require_contains(valid_manifest, '"stability": "preview"', "valid_manifest.stability")
        require_contains(invalid_manifest, '"kind": "runtime-plugin"', "invalid_manifest.bad_kind")

        require_contains(readme, "docs/extension-manifest.md", "readme.extension_manifest")
        require_contains(readme, "docs/ecosystem-governance.md", "readme.ecosystem_governance")
        require_contains(toolchain, "python3 tools/toolchain.py validate-ecosystem-contracts", "toolchain.validate_ecosystem")

        require_contains(issue, "定义扩展 `manifest` 字段与版本范围", "issue.manifest_task")
        require_contains(issue, "建立生态变更审查规则（格式变更/版本变更/迁移器要求）", "issue.governance_task")
    except ValueError as exc:
        return error("tool.validate.ecosystem_contracts.format", VN_E_FORMAT, str(exc), "ecosystem contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.ecosystem_contracts.ok",
                f"root={root}",
                "manifest_contract=present",
                "governance_contract=present",
                "compat_tool_manifest=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
