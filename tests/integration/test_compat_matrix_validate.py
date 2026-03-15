#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_compat_matrix.py"]


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
        print(f"compat_matrix current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.compat_matrix.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_compat_matrix_") as temp_dir:
        temp_root = Path(temp_dir)
        (temp_root / "docs").mkdir(parents=True, exist_ok=True)

        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        shutil.copy2(ROOT / "issue.md", temp_root / "issue.md")
        shutil.copy2(ROOT / "docs" / "toolchain.md", temp_root / "docs" / "toolchain.md")
        shutil.copy2(ROOT / "docs" / "compat-matrix.md", temp_root / "docs" / "compat-matrix.md")
        shutil.copy2(ROOT / "docs" / "release-roadmap-1.0.0.md", temp_root / "docs" / "release-roadmap-1.0.0.md")
        shutil.copy2(ROOT / "docs" / "release-checklist-v1.0.0.md", temp_root / "docs" / "release-checklist-v1.0.0.md")
        shutil.copy2(ROOT / "docs" / "host-sdk.md", temp_root / "docs" / "host-sdk.md")

        broken = (temp_root / "docs" / "compat-matrix.md").read_text(encoding="utf-8")
        broken = broken.replace("| `v1.0.0` 首版承诺 | 是 | 是 | 是 | 是 | 否 |",
                                "| `v1.0.0` 首版承诺 | 是 | 是 | 是 | 是 | 是 |",
                                1)
        (temp_root / "docs" / "compat-matrix.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken compat matrix should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.compat_matrix.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_compat_matrix_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
