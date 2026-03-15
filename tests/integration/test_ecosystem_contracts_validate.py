#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_ecosystem_contracts.py"]


def run_case(root_path: Path):
    proc = subprocess.run(
        TOOL + [str(root_path)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def main():
    rc, out, err = run_case(ROOT)
    if rc != 0:
        print(f"ecosystem_contracts current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.ecosystem_contracts.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_ecosystem_contracts_") as temp_dir:
        temp_root = Path(temp_dir)
        (temp_root / "docs").mkdir(parents=True, exist_ok=True)
        (temp_root / "tools" / "validate").mkdir(parents=True, exist_ok=True)
        (temp_root / "tests" / "fixtures" / "tool_manifest" / "valid").mkdir(parents=True, exist_ok=True)
        (temp_root / "tests" / "fixtures" / "tool_manifest" / "invalid").mkdir(parents=True, exist_ok=True)

        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        shutil.copy2(ROOT / "issue.md", temp_root / "issue.md")
        shutil.copy2(ROOT / "docs" / "toolchain.md", temp_root / "docs" / "toolchain.md")
        shutil.copy2(ROOT / "docs" / "compat-matrix.md", temp_root / "docs" / "compat-matrix.md")
        shutil.copy2(ROOT / "docs" / "extension-manifest.md", temp_root / "docs" / "extension-manifest.md")
        shutil.copy2(ROOT / "docs" / "ecosystem-governance.md", temp_root / "docs" / "ecosystem-governance.md")
        shutil.copy2(ROOT / "tools" / "validate" / "validate_manifest.py", temp_root / "tools" / "validate" / "validate_manifest.py")
        shutil.copy2(ROOT / "tests" / "fixtures" / "tool_manifest" / "valid" / "vnsave_migrate.json",
                     temp_root / "tests" / "fixtures" / "tool_manifest" / "valid" / "vnsave_migrate.json")
        shutil.copy2(ROOT / "tests" / "fixtures" / "tool_manifest" / "invalid" / "bad_kind.json",
                     temp_root / "tests" / "fixtures" / "tool_manifest" / "invalid" / "bad_kind.json")

        broken = (temp_root / "docs" / "extension-manifest.md").read_text(encoding="utf-8")
        broken = broken.replace("`importer/exporter/validator/migrator`", "`importer/exporter`", 1)
        (temp_root / "docs" / "extension-manifest.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken ecosystem contracts should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.ecosystem_contracts.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_ecosystem_contracts_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
