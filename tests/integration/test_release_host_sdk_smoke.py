#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_host_sdk_smoke.sh"]


def main():
    summary_path = ROOT / "tests" / "integration" / "host_sdk_smoke_tmp.md"
    summary_json_path = ROOT / "tests" / "integration" / "host_sdk_smoke_tmp.json"
    try:
        if summary_path.exists():
            summary_path.unlink()
    except FileNotFoundError:
        pass
    try:
        if summary_json_path.exists():
            summary_json_path.unlink()
    except FileNotFoundError:
        pass

    proc = subprocess.run(
        SCRIPT + ["--summary-out", str(summary_path), "--summary-json-out", str(summary_json_path)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"host sdk smoke failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1
    if "trace_id=release.host_sdk.ok" not in proc.stdout:
        print("missing host sdk smoke success trace", file=sys.stderr)
        return 1
    if not summary_path.exists():
        print("host sdk smoke summary missing", file=sys.stderr)
        return 1
    if not summary_json_path.exists():
        print("host sdk smoke json summary missing", file=sys.stderr)
        return 1

    summary_text = summary_path.read_text(encoding="utf-8")
    if "# Host SDK Smoke Summary" not in summary_text:
        print("host sdk summary header missing", file=sys.stderr)
        return 1
    if "host_embed ok" not in summary_text:
        print("host sdk summary missing host_embed result", file=sys.stderr)
        return 1

    summary_path.unlink()
    summary_json_path.unlink()
    print("test_release_host_sdk_smoke ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
