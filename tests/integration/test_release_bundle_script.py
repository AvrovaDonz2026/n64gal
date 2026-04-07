#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_bundle.sh"]


def write_text(path: Path, text: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def main():
    with tempfile.TemporaryDirectory(prefix="n64gal_release_bundle_") as temp_dir:
        out_dir = Path(temp_dir) / "bundle"
        release_spec = Path(temp_dir) / "release_spec.json"
        gate_summary = Path(temp_dir) / "release_gate_summary.md"
        soak_summary = Path(temp_dir) / "demo_soak_summary.md"
        ci_summary = Path(temp_dir) / "ci_suite_summary.md"
        host_sdk_summary = Path(temp_dir) / "host_sdk_smoke_summary.md"
        host_sdk_summary_json = Path(temp_dir) / "host_sdk_smoke_summary.json"
        platform_summary = Path(temp_dir) / "platform_evidence_summary.md"
        platform_summary_json = Path(temp_dir) / "platform_evidence_summary.json"
        preview_summary = Path(temp_dir) / "preview_evidence_summary.md"
        preview_summary_json = Path(temp_dir) / "preview_evidence_summary.json"
        report_md = Path(temp_dir) / "release_report.md"
        report_json = Path(temp_dir) / "release_report.json"
        publish_map_md = Path(temp_dir) / "release_publish_map.md"
        publish_map_json = Path(temp_dir) / "release_publish_map.json"
        remote_summary_md = Path(temp_dir) / "release_remote_summary.md"
        remote_summary_json = Path(temp_dir) / "release_remote_summary.json"
        release_note = Path(temp_dir) / "release-v1.0.0.md"
        release_evidence = Path(temp_dir) / "release-evidence-v1.0.0.md"
        release_package = Path(temp_dir) / "release-package-v1.0.0.md"

        write_text(
            release_spec,
            '{"version":"v1.0.0","tag":"v1.0.0","release_url":"https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.0.0","release_note":"%s","asset":{"path":"%s"}}\n'
            % (release_note, ROOT / "assets" / "demo" / "demo.vnpak"),
        )
        write_text(gate_summary, "# Release Gate Summary\n\n- Status: `success`\n")
        write_text(soak_summary, "# Demo Soak Summary\n\n- Status: `success`\n")
        write_text(ci_summary, "# CI Suite Summary\n\n- Status: `success`\n")
        write_text(host_sdk_summary, "# Host SDK Smoke Summary\n\n- Status: `success`\n")
        write_text(host_sdk_summary_json, "{\n  \"status\": \"success\"\n}\n")
        write_text(platform_summary, "# Platform Evidence Summary\n\n- Status: `success`\n")
        write_text(platform_summary_json, "{\n  \"status\": \"success\"\n}\n")
        write_text(preview_summary, "# Preview Evidence Summary\n\n- Status: `success`\n")
        write_text(preview_summary_json, "{\n  \"status\": \"success\"\n}\n")
        write_text(report_md, "# Release Report\n")
        write_text(report_json, "{\n  \"report_md\": \"release_report.md\"\n}\n")
        write_text(publish_map_md, "# Release Publish Map\n")
        write_text(publish_map_json, "{\n  \"tag\": \"v0.1.0-alpha\"\n}\n")
        write_text(remote_summary_md, "# Release Remote Summary\n")
        write_text(remote_summary_json, "{\n  \"tag\": \"v0.1.0-alpha\"\n}\n")
        write_text(release_note, "# N64GAL v1.0.0\n")
        write_text(release_evidence, "# Release Evidence: v1.0.0\n")
        write_text(release_package, "# Release Package Plan: v1.0.0\n")

        proc = subprocess.run(
            SCRIPT + [
                "--out-dir", str(out_dir),
                "--release-spec", str(release_spec),
                "--release-gate-summary", str(gate_summary),
                "--demo-soak-summary", str(soak_summary),
                "--ci-suite-summary", str(ci_summary),
                "--host-sdk-summary", str(host_sdk_summary),
                "--host-sdk-summary-json", str(host_sdk_summary_json),
                "--platform-evidence-summary", str(platform_summary),
                "--platform-evidence-summary-json", str(platform_summary_json),
                "--preview-evidence-summary", str(preview_summary),
                "--preview-evidence-summary-json", str(preview_summary_json),
                "--report-md", str(report_md),
                "--report-json", str(report_json),
                "--publish-map-md", str(publish_map_md),
                "--publish-map-json", str(publish_map_json),
                "--remote-summary-md", str(remote_summary_md),
                "--remote-summary-json", str(remote_summary_json),
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            print(f"release bundle failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
            return 1
        if "trace_id=release.bundle.ok" not in proc.stdout:
            print("missing release bundle success trace", file=sys.stderr)
            return 1

        index_path = out_dir / "release_bundle_index.md"
        index_json_path = out_dir / "release_bundle_index.json"
        manifest_path = out_dir / "release_bundle_manifest.md"
        manifest_json_path = out_dir / "release_bundle_manifest.json"
        if not index_path.exists():
            print("release bundle index missing", file=sys.stderr)
            return 1
        if not index_json_path.exists():
            print("release bundle json index missing", file=sys.stderr)
            return 1
        if not manifest_path.exists() or not manifest_json_path.exists():
            print("release bundle manifest missing", file=sys.stderr)
            return 1
        index_text = index_path.read_text(encoding="utf-8")
        if "summaries/release_gate_summary.md" not in index_text:
            print("release bundle index missing gate summary", file=sys.stderr)
            return 1
        if "summaries/platform_evidence_summary.md" not in index_text:
            print("release bundle index missing platform summary", file=sys.stderr)
            return 1
        if "summaries/preview_evidence_summary.json" not in index_text:
            print("release bundle index missing preview json summary", file=sys.stderr)
            return 1
        if "summaries/release_report.json" not in index_text or "summaries/release_publish_map.json" not in index_text:
            print("release bundle index missing derived artifacts", file=sys.stderr)
            return 1
        if "summaries/release_remote_summary.json" not in index_text:
            print("release bundle index missing remote summary artifact", file=sys.stderr)
            return 1
        index_json_text = index_json_path.read_text(encoding="utf-8")
        if '"summaries/release_gate_summary.md"' not in index_json_text:
            print("release bundle json missing gate summary", file=sys.stderr)
            return 1
        if '"summaries/host_sdk_smoke_summary.json"' not in index_json_text:
            print("release bundle json missing host sdk json summary", file=sys.stderr)
            return 1
        if '"summaries/release_report.json"' not in index_json_text or '"summaries/release_publish_map.json"' not in index_json_text:
            print("release bundle json missing derived artifact references", file=sys.stderr)
            return 1
        if '"summaries/release_remote_summary.json"' not in index_json_text:
            print("release bundle json missing remote summary reference", file=sys.stderr)
            return 1
        manifest_text = manifest_path.read_text(encoding="utf-8")
        if "release_bundle_manifest.json" not in index_text or "`demo.vnpak`" not in manifest_text:
            print("release bundle manifest contents missing", file=sys.stderr)
            return 1
        if "`docs/release-v1.0.0.md`" not in index_text or "`docs/release-evidence-v1.0.0.md`" not in index_text:
            print("release bundle index missing versioned release docs", file=sys.stderr)
            return 1
        manifest_json_text = manifest_json_path.read_text(encoding="utf-8")
        if '"path":"demo.vnpak"' not in manifest_json_text or '"sha256":"' not in manifest_json_text:
            print("release bundle manifest json missing digest entry", file=sys.stderr)
            return 1
        if not (out_dir / "demo.vnpak").exists():
            print("release bundle missing demo.vnpak", file=sys.stderr)
            return 1

    print("test_release_bundle_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
