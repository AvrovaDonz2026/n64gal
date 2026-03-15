#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_preview_evidence.sh"]


def main():
    summary_path = ROOT / "tests" / "integration" / "release_preview_tmp.md"
    try:
        if summary_path.exists():
            summary_path.unlink()
    except FileNotFoundError:
        pass

    proc = subprocess.run(
        SCRIPT + ["--summary-out", str(summary_path)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"preview evidence failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1
    if "trace_id=release.preview.ok" not in proc.stdout:
        print("missing preview evidence success trace", file=sys.stderr)
        return 1
    if not summary_path.exists():
        print("preview evidence summary missing", file=sys.stderr)
        return 1

    summary_text = summary_path.read_text(encoding="utf-8")
    if "# Preview Evidence Summary" not in summary_text:
        print("preview evidence summary header missing", file=sys.stderr)
        return 1
    if "`trace_id=preview.ok`" not in summary_text:
        print("preview evidence summary missing preview ok trace", file=sys.stderr)
        return 1

    summary_path.unlink()
    print("test_release_preview_evidence ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
