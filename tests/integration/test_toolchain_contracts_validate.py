#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_toolchain_contracts.py"]


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
        print(f"toolchain_contracts current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.toolchain_contracts.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_toolchain_contracts_") as temp_dir:
        temp_root = Path(temp_dir)
        (temp_root / "tools").mkdir(parents=True, exist_ok=True)
        (temp_root / "docs").mkdir(parents=True, exist_ok=True)
        (temp_root / "tests" / "integration").mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / "tools" / "toolchain.py", temp_root / "tools" / "toolchain.py")
        shutil.copy2(ROOT / "docs" / "toolchain.md", temp_root / "docs" / "toolchain.md")
        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        shutil.copy2(ROOT / "tests" / "integration" / "test_toolchain_cli.py", temp_root / "tests" / "integration" / "test_toolchain_cli.py")

        broken = (temp_root / "docs" / "toolchain.md").read_text(encoding="utf-8")
        broken = broken.replace("python3 tools/toolchain.py probe-kernel-compare tests/fixtures/kernel_compare/sample_kernel_compare.csv\n", "")
        (temp_root / "docs" / "toolchain.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken toolchain contracts should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.toolchain_contracts.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_toolchain_contracts_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
