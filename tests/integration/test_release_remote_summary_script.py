#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_remote_summary.sh"]


def main():
    with tempfile.TemporaryDirectory(prefix="n64gal_release_remote_summary_") as temp_dir:
        out_dir = Path(temp_dir) / "remote"
        fixture = ROOT / "tests" / "fixtures" / "release_api" / "github_release_v0.1.0-alpha.json"
        release_spec = ROOT / "docs" / "release-publish-v0.1.0-alpha.json"
        proc = subprocess.run(
            SCRIPT + ["--release-spec", str(release_spec), "--release-json", str(fixture), "--out-dir", str(out_dir)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            print(f"release remote summary failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
            return 1
        if "trace_id=release.remote_summary.ok" not in proc.stdout:
            print("missing release remote summary success trace", file=sys.stderr)
            return 1
        if not (out_dir / "release_remote_summary.md").exists() or not (out_dir / "release_remote_summary.json").exists():
            print("release remote summary outputs missing", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_release_remote_http_") as temp_dir:
        temp_root = Path(temp_dir)
        served = temp_root / "github_release_v0.1.0-alpha.json"
        served.write_text(fixture.read_text(encoding="utf-8"), encoding="utf-8")
        api_served = temp_root / "repos" / "AvrovaDonz2026" / "n64gal" / "releases" / "tags" / "v0.1.0-alpha"
        api_served.parent.mkdir(parents=True, exist_ok=True)
        api_served.write_text(fixture.read_text(encoding="utf-8"), encoding="utf-8")
        url = served.resolve().as_uri()
        out_dir = temp_root / "remote_url"
        proc = subprocess.run(
            SCRIPT + ["--release-spec", str(release_spec), "--release-json-url", url, "--out-dir", str(out_dir)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )

        if proc.returncode != 0:
            print(f"release remote summary url failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
            return 1
        if "trace_id=release.remote_summary.ok" not in proc.stdout:
            print("missing release remote summary url success trace", file=sys.stderr)
            return 1
        if not (out_dir / "release_remote_state.json").exists():
            print("release remote summary url missing fetched json", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_release_remote_api_") as temp_dir:
        temp_root = Path(temp_dir)
        api_served = temp_root / "repos" / "AvrovaDonz2026" / "n64gal" / "releases" / "tags" / "v0.1.0-alpha"
        api_served.parent.mkdir(parents=True, exist_ok=True)
        api_served.write_text(fixture.read_text(encoding="utf-8"), encoding="utf-8")
        api_root = temp_root.resolve().as_uri()
        out_dir = temp_root / "remote_api"
        proc = subprocess.run(
            SCRIPT + ["--release-spec", str(release_spec), "--github-repo", "AvrovaDonz2026/n64gal", "--api-root", api_root, "--out-dir", str(out_dir)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )

        if proc.returncode != 0:
            print(f"release remote summary api failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
            return 1
        if "trace_id=release.remote_summary.ok" not in proc.stdout:
            print("missing release remote summary api success trace", file=sys.stderr)
            return 1
        if not (out_dir / "release_remote_state.json").exists():
            print("release remote summary api missing fetched json", file=sys.stderr)
            return 1

    print("test_release_remote_summary_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
