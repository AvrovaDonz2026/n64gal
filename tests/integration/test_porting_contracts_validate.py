#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_porting_contracts.py"]


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
        print(f"porting_contracts current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.porting_contracts.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_porting_contracts_") as temp_dir:
        temp_root = Path(temp_dir)
        (temp_root / "docs" / "api").mkdir(parents=True, exist_ok=True)
        (temp_root / "include").mkdir(parents=True, exist_ok=True)
        (temp_root / "tests" / "unit").mkdir(parents=True, exist_ok=True)

        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        shutil.copy2(ROOT / "issue.md", temp_root / "issue.md")
        shutil.copy2(ROOT / "docs" / "toolchain.md", temp_root / "docs" / "toolchain.md")
        shutil.copy2(ROOT / "docs" / "backend-porting.md", temp_root / "docs" / "backend-porting.md")
        shutil.copy2(ROOT / "docs" / "api" / "backend.md", temp_root / "docs" / "api" / "backend.md")
        shutil.copy2(ROOT / "include" / "vn_backend.h", temp_root / "include" / "vn_backend.h")
        shutil.copy2(ROOT / "include" / "vn_renderer.h", temp_root / "include" / "vn_renderer.h")
        shutil.copy2(ROOT / "tests" / "unit" / "test_backend_priority.c", temp_root / "tests" / "unit" / "test_backend_priority.c")
        shutil.copy2(ROOT / "tests" / "unit" / "test_renderer_fallback.c", temp_root / "tests" / "unit" / "test_renderer_fallback.c")
        shutil.copy2(ROOT / "tests" / "unit" / "test_renderer_dirty_submit.c", temp_root / "tests" / "unit" / "test_renderer_dirty_submit.c")
        shutil.copy2(ROOT / "tests" / "unit" / "test_backend_consistency.c", temp_root / "tests" / "unit" / "test_backend_consistency.c")
        shutil.copy2(ROOT / "tests" / "unit" / "test_runtime_golden.c", temp_root / "tests" / "unit" / "test_runtime_golden.c")

        broken = (temp_root / "docs" / "backend-porting.md").read_text(encoding="utf-8")
        broken = broken.replace("`test_runtime_golden`", "`test_runtime_old`", 1)
        (temp_root / "docs" / "backend-porting.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken porting contracts should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.porting_contracts.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_porting_contracts_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
