#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_demo_soak.sh"]


def main():
    summary_path = ROOT / "tests" / "integration" / "release_soak_tmp.md"
    try:
        if summary_path.exists():
            summary_path.unlink()
    except FileNotFoundError:
        pass

    proc = subprocess.run(
        SCRIPT + [
            "--skip-pack",
            "--frames-per-scene", "4",
            "--scenes", "S0,S1",
            "--summary-out", str(summary_path),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"release soak failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1
    if "trace_id=release.soak.ok" not in proc.stdout:
        print("missing release soak success trace", file=sys.stderr)
        return 1
    if not summary_path.exists():
        print("release soak summary missing", file=sys.stderr)
        return 1

    summary_text = summary_path.read_text(encoding="utf-8")
    if "# Demo Soak Summary" not in summary_text:
        print("release soak summary header missing", file=sys.stderr)
        return 1
    if "`S0`" not in summary_text or "`S1`" not in summary_text:
        print("release soak summary missing scenes", file=sys.stderr)
        return 1

    summary_path.unlink()
    print("test_release_soak_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
