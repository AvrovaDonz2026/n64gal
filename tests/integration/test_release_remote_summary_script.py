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

    with tempfile.TemporaryDirectory(prefix="n64gal_release_remote_summary_multi_") as temp_dir:
        temp_root = Path(temp_dir)
        spec = temp_root / "release_spec.json"
        remote = temp_root / "release.json"
        out_dir = temp_root / "out"
        spec.write_text(
            '{"version":"v1.1.0","repository":"AvrovaDonz2026/n64gal","tag":"v1.1.0","release_url":"https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.1.0","draft":false,"prerelease":false,"release_note":"docs/release-v1.1.0.md","assets":[{"name":"demo.vnpak","path":"assets/demo/demo.vnpak"},{"name":"content-demo.vnpak","path":"assets/demo/content-demo.vnpak"}]}\n',
            encoding="utf-8",
        )
        remote.write_text(
            '{"tag_name":"v1.1.0","html_url":"https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.1.0","draft":false,"prerelease":false,"assets":['
            '{"name":"demo.vnpak","browser_download_url":"https://github.com/AvrovaDonz2026/n64gal/releases/download/v1.1.0/demo.vnpak","size":10},'
            '{"name":"content-demo.vnpak","browser_download_url":"https://github.com/AvrovaDonz2026/n64gal/releases/download/v1.1.0/content-demo.vnpak","size":20}]}\n',
            encoding="utf-8",
        )
        proc = subprocess.run(
            SCRIPT + ["--release-spec", str(spec), "--release-json", str(remote), "--out-dir", str(out_dir)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            print(f"multi-asset remote summary failed rc={proc.returncode} stderr={proc.stderr}", file=sys.stderr)
            return 1
        summary_json = (out_dir / "release_remote_summary.json").read_text(encoding="utf-8")
        if '"assets": [' not in summary_json or '"name":"content-demo.vnpak"' not in summary_json:
            print("multi-asset remote summary output missing secondary asset", file=sys.stderr)
            return 1

    print("test_release_remote_summary_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
