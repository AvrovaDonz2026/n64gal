#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_migration_contracts.py"]


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
        print(f"migration_contracts current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.migration_contracts.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_migration_contracts_") as temp_dir:
        temp_root = Path(temp_dir)
        (temp_root / "docs" / "api").mkdir(parents=True, exist_ok=True)

        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        shutil.copy2(ROOT / "issue.md", temp_root / "issue.md")
        shutil.copy2(ROOT / "docs" / "toolchain.md", temp_root / "docs" / "toolchain.md")
        shutil.copy2(ROOT / "docs" / "migration.md", temp_root / "docs" / "migration.md")
        shutil.copy2(ROOT / "docs" / "api" / "save.md", temp_root / "docs" / "api" / "save.md")
        shutil.copy2(ROOT / "docs" / "vnsave-version-policy.md", temp_root / "docs" / "vnsave-version-policy.md")
        shutil.copy2(ROOT / "docs" / "release-v0.1.0-alpha.md", temp_root / "docs" / "release-v0.1.0-alpha.md")
        shutil.copy2(ROOT / "docs" / "release-gap-v0.1.0-mvp.md", temp_root / "docs" / "release-gap-v0.1.0-mvp.md")

        broken = (temp_root / "docs" / "migration.md").read_text(encoding="utf-8")
        broken = broken.replace("当前已有最小 `v0 -> v1` 迁移命令与 probe/reject 规则",
                                "当前还没有迁移命令",
                                1)
        (temp_root / "docs" / "migration.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken migration contracts should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.migration_contracts.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_migration_contracts_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
