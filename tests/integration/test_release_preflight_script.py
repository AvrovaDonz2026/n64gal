#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_preflight.sh"]


def main():
    with tempfile.TemporaryDirectory(prefix="n64gal_release_preflight_") as temp_dir:
        out_dir = Path(temp_dir) / "preflight"
        proc = subprocess.run(
            SCRIPT
            + [
                "--allow-dirty",
                "--skip-cc-suite",
                "--out-dir",
                str(out_dir),
                "--soak-frames-per-scene",
                "2",
                "--soak-scenes",
                "S0",
                "--remote-release-json",
                "tests/fixtures/release_api/github_release_v0.1.0-alpha.json",
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            print(f"release preflight failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
            return 1
        if "trace_id=release.preflight.ok" not in proc.stdout:
            print("missing release preflight success trace", file=sys.stderr)
            return 1
        if not (out_dir / "release_preflight_summary.md").exists():
            print("release preflight summary missing", file=sys.stderr)
            return 1
        if not (out_dir / "export" / "bundle" / "summaries" / "release_remote_summary.json").exists():
            print("release preflight export missing remote summary", file=sys.stderr)
            return 1

    print("test_release_preflight_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
