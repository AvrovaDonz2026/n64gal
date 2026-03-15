#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_gate.sh"]


def main():
    summary_path = ROOT / "tests" / "integration" / "release_gate_tmp.md"
    try:
        if summary_path.exists():
            summary_path.unlink()
    except FileNotFoundError:
        pass

    proc = subprocess.run(
        SCRIPT + ["--allow-dirty", "--skip-cc-suite", "--summary-out", str(summary_path)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"release gate script failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1
    if "trace_id=release.gate.ok" not in proc.stdout:
        print("missing release gate success trace", file=sys.stderr)
        return 1
    if not summary_path.exists():
        print("release gate summary missing", file=sys.stderr)
        return 1

    summary_text = summary_path.read_text(encoding="utf-8")
    if "# Release Gate Summary" not in summary_text:
        print("release gate summary header missing", file=sys.stderr)
        return 1
    if "validate-all" not in summary_text:
        print("release gate summary missing validate-all step", file=sys.stderr)
        return 1

    summary_path.unlink()
    print("test_release_gate_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
