#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_save_contracts.py"]


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
        print(f"save_contracts current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.save_contracts.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_save_contracts_") as temp_dir:
        temp_root = Path(temp_dir)
        (temp_root / "docs" / "api").mkdir(parents=True, exist_ok=True)
        (temp_root / "include").mkdir(parents=True, exist_ok=True)
        (temp_root / "tools" / "probe").mkdir(parents=True, exist_ok=True)
        (temp_root / "tools" / "migrate").mkdir(parents=True, exist_ok=True)
        (temp_root / "tests" / "unit").mkdir(parents=True, exist_ok=True)

        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        shutil.copy2(ROOT / "docs" / "api" / "save.md", temp_root / "docs" / "api" / "save.md")
        shutil.copy2(ROOT / "docs" / "vnsave-version-policy.md", temp_root / "docs" / "vnsave-version-policy.md")
        shutil.copy2(ROOT / "docs" / "migration.md", temp_root / "docs" / "migration.md")
        shutil.copy2(ROOT / "docs" / "toolchain.md", temp_root / "docs" / "toolchain.md")
        shutil.copy2(ROOT / "include" / "vn_save.h", temp_root / "include" / "vn_save.h")
        shutil.copy2(ROOT / "tools" / "probe" / "vnsave_probe.c", temp_root / "tools" / "probe" / "vnsave_probe.c")
        shutil.copy2(ROOT / "tools" / "migrate" / "vnsave_migrate.c", temp_root / "tools" / "migrate" / "vnsave_migrate.c")
        shutil.copy2(ROOT / "tests" / "unit" / "test_vnsave.c", temp_root / "tests" / "unit" / "test_vnsave.c")

        broken = (temp_root / "docs" / "api" / "save.md").read_text(encoding="utf-8")
        broken = broken.replace("### `int vnsave_migrate_v0_to_v1_file(const char* in_path, const char* out_path)`",
                                "### `int broken_save_api(void)`",
                                1)
        (temp_root / "docs" / "api" / "save.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken save contracts should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.save_contracts.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_save_contracts_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
