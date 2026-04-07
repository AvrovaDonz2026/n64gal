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
        ci_summary = Path(temp_dir) / "ci_suite_summary.md"
        release_spec = Path(temp_dir) / "release_spec.json"
        release_note = Path(temp_dir) / "release-v1.0.0.md"
        release_evidence = Path(temp_dir) / "release-evidence-v1.0.0.md"
        release_package = Path(temp_dir) / "release-package-v1.0.0.md"
        remote_release_json = Path(temp_dir) / "github_release_v1.0.0.json"
        ci_summary.write_text("# CI Suite Summary\n\n- Status: `success`\n", encoding="utf-8")
        release_note.write_text("# N64GAL v1.0.0\n", encoding="utf-8")
        release_evidence.write_text("# Release Evidence: v1.0.0\n", encoding="utf-8")
        release_package.write_text("# Release Package Plan: v1.0.0\n", encoding="utf-8")
        remote_release_json.write_text(
            '{\n'
            '  "tag_name": "v1.0.0",\n'
            '  "html_url": "https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.0.0",\n'
            '  "draft": false,\n'
            '  "prerelease": false,\n'
            '  "assets": [\n'
            '    {"name": "demo.vnpak", "browser_download_url": "https://github.com/AvrovaDonz2026/n64gal/releases/download/v1.0.0/demo.vnpak", "size": 1853}\n'
            '  ]\n'
            '}\n',
            encoding="utf-8",
        )
        release_spec.write_text(
            '{"version":"v1.0.0","tag":"v1.0.0","release_url":"https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.0.0","draft":false,"prerelease":false,"release_note":"%s","asset":{"path":"%s"}}\n'
            % (release_note, ROOT / "assets" / "demo" / "demo.vnpak"),
            encoding="utf-8",
        )
        proc = subprocess.run(
            SCRIPT
            + [
                "--allow-dirty",
                "--skip-cc-suite",
                "--out-dir",
                str(out_dir),
                "--remote-release-spec",
                str(release_spec),
                "--ci-suite-summary",
                str(ci_summary),
                "--soak-frames-per-scene",
                "2",
                "--soak-scenes",
                "S0",
                "--remote-release-json",
                str(remote_release_json),
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
        bundle_index = (out_dir / "export" / "bundle" / "release_bundle_index.md").read_text(encoding="utf-8")
        if "`docs/release-v1.0.0.md`" not in bundle_index:
            print("release preflight bundle index missing v1.0.0 note", file=sys.stderr)
            return 1

    print("test_release_preflight_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
