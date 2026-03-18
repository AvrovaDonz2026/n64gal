#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_publish_map.sh"]


def write_text(path: Path, text: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def main():
    with tempfile.TemporaryDirectory(prefix="n64gal_release_publish_") as temp_dir:
        out_dir = Path(temp_dir) / "publish"
        bundle_index = Path(temp_dir) / "release_bundle_index.md"
        bundle_manifest = Path(temp_dir) / "release_bundle_manifest.json"
        report_json = Path(temp_dir) / "release_report.json"

        write_text(bundle_index, "# Release Bundle\n")
        write_text(bundle_manifest, '{"files":[{"path":"demo.vnpak","sha256":"deadbeef","bytes":1}]}\n')
        write_text(report_json, '{"report_md":"release_report.md"}\n')

        proc = subprocess.run(
            SCRIPT
            + [
                "--out-dir",
                str(out_dir),
                "--bundle-index",
                str(bundle_index),
                "--bundle-manifest",
                str(bundle_manifest),
                "--report-json",
                str(report_json),
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            print(f"release publish map failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
            return 1
        if "trace_id=release.publish_map.ok" not in proc.stdout:
            print("missing release publish map success trace", file=sys.stderr)
            return 1

        map_md = out_dir / "release_publish_map.md"
        map_json = out_dir / "release_publish_map.json"
        if not map_md.exists() or not map_json.exists():
            print("release publish map outputs missing", file=sys.stderr)
            return 1

        map_text = map_md.read_text(encoding="utf-8")
        if "# Release Publish Map" not in map_text or "`v0.1.0-alpha`" not in map_text:
            print("release publish map markdown content missing", file=sys.stderr)
            return 1

        map_json_text = map_json.read_text(encoding="utf-8")
        if '"tag": "v0.1.0-alpha"' not in map_json_text:
            print("release publish map json missing tag", file=sys.stderr)
            return 1
        if '"bundle_manifest":' not in map_json_text or '"sha256":' not in map_json_text:
            print("release publish map json missing bundle/asset details", file=sys.stderr)
            return 1

    print("test_release_publish_map_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
