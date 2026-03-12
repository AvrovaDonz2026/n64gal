#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_release_contracts.py"]


def run_case(root_path: Path):
    proc = subprocess.run(
        TOOL + ["--root", str(root_path)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def main():
    rc, out, err = run_case(ROOT)
    if rc != 0:
        print(f"release_contracts current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.release_contracts.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_contracts_") as temp_dir:
        temp_root = Path(temp_dir)
        shutil.copytree(ROOT / "docs", temp_root / "docs")
        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        broken = (temp_root / "docs" / "compat-matrix.md").read_text(encoding="utf-8")
        broken = broken.replace("| `tool manifest` | `planned v1` |", "| `tool manifest` | missing |")
        (temp_root / "docs" / "compat-matrix.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken contracts should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.release_contracts.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_release_contracts_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
