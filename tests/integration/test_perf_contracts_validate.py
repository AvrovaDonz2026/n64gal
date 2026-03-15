#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_perf_contracts.py"]


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
        print(f"perf_contracts current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.perf_contracts.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_perf_contracts_") as temp_dir:
        temp_root = Path(temp_dir)
        (temp_root / "docs").mkdir(parents=True, exist_ok=True)
        (temp_root / "tests" / "perf").mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        shutil.copy2(ROOT / "docs" / "toolchain.md", temp_root / "docs" / "toolchain.md")
        shutil.copy2(ROOT / "docs" / "perf-report.md", temp_root / "docs" / "perf-report.md")
        shutil.copy2(ROOT / "tests" / "perf" / "run_perf.sh", temp_root / "tests" / "perf" / "run_perf.sh")
        shutil.copy2(ROOT / "tests" / "perf" / "compare_perf.sh", temp_root / "tests" / "perf" / "compare_perf.sh")
        shutil.copy2(ROOT / "tests" / "perf" / "compare_kernel_bench.sh", temp_root / "tests" / "perf" / "compare_kernel_bench.sh")

        broken = (temp_root / "docs" / "perf-report.md").read_text(encoding="utf-8")
        broken = broken.replace("`host_cpu`", "`host_cpu_missing`", 1)
        (temp_root / "docs" / "perf-report.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken perf contracts should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.perf_contracts.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_perf_contracts_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
