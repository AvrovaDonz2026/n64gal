#!/usr/bin/env python3
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_export.sh"]


def write_text(path: Path, text: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def content_soak_payload():
    return {
        "schema_version": 1,
        "status": "success",
        "source_state": "dirty",
        "pack": "assets/demo/content-demo.vnpak",
        "pack_sha256": hashlib.sha256(b"content-demo").hexdigest(),
        "scenes": ["Opening", "Gallery"],
        "frames_per_scene": 28125,
        "dt_ms": 16,
        "total_simulated_ms": 900000,
        "scene_results": [
            {"scene": scene, "status": "success", "frames_executed": 28125, "vm_ended": 1, "vm_error": 0}
            for scene in ("Opening", "Gallery")
        ],
    }


def main():
    with tempfile.TemporaryDirectory(prefix="n64gal_release_export_") as temp_dir:
        temp_root = Path(temp_dir)
        out_dir = temp_root / "export"
        release_spec = temp_root / "release_spec.json"
        release_note = temp_root / "release-v1.1.0.md"
        release_evidence = temp_root / "release-evidence-v1.1.0.md"
        release_package = temp_root / "release-package-v1.1.0.md"
        release_checklist = temp_root / "release-checklist-v1.1.0.md"
        content_demo = temp_root / "content-demo.vnpak"
        gate_summary = temp_root / "release_gate_summary.md"
        soak_summary = temp_root / "demo_soak_summary.md"
        content_soak_summary = temp_root / 'content"evidence' / "content_soak_summary.md"
        content_soak_summary_json = temp_root / 'content"evidence' / "content_soak_summary.json"
        ci_summary = temp_root / "ci_suite_summary.md"
        host_sdk_summary = temp_root / "host_sdk_smoke_summary.md"
        host_sdk_summary_json = temp_root / "host_sdk_smoke_summary.json"
        platform_summary = temp_root / "platform_evidence_summary.md"
        platform_summary_json = temp_root / "platform_evidence_summary.json"
        preview_summary = temp_root / "preview_evidence_summary.md"
        preview_summary_json = temp_root / "preview_evidence_summary.json"
        remote_release_json = temp_root / "github_release_v1.1.0.json"

        write_text(
            release_spec,
            '{"version":"v1.1.0","tag":"v1.1.0","release_url":"https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.1.0","draft":false,"prerelease":false,"release_note":"%s","release_evidence":"%s","release_package":"%s","release_checklist":"%s","assets":[{"name":"demo.vnpak","path":"%s"},{"name":"content-demo.vnpak","path":"%s"}]}\n'
            % (release_note, release_evidence, release_package, release_checklist, ROOT / "assets" / "demo" / "demo.vnpak", content_demo),
        )
        write_text(release_note, "# N64GAL v1.1.0\n")
        write_text(release_evidence, "# Release Evidence: v1.1.0\n")
        write_text(release_package, "# Release Package Plan: v1.1.0\n")
        write_text(release_checklist, "# Release Checklist: v1.1.0\n")
        content_demo.write_bytes(b"content-demo")
        write_text(gate_summary, "# Release Gate Summary\n")
        write_text(soak_summary, "# Demo Soak Summary\n")
        write_text(content_soak_summary, "# Demo Soak Summary\n\n- Status: `success`\n- Scenes: `Opening,Gallery`\n")
        write_text(content_soak_summary_json, json.dumps(content_soak_payload()) + "\n")
        write_text(ci_summary, "# CI Suite Summary\n\n- Status: `success`\n")
        write_text(host_sdk_summary, "# Host SDK Smoke Summary\n")
        write_text(host_sdk_summary_json, '{"status":"ok"}\n')
        write_text(platform_summary, "# Platform Evidence Summary\n")
        write_text(platform_summary_json, '{"status":"ok"}\n')
        write_text(preview_summary, "# Preview Evidence Summary\n")
        write_text(preview_summary_json, '{"status":"ok"}\n')
        write_text(
            remote_release_json,
            '{\n'
            '  "tag_name": "v1.1.0",\n'
            '  "html_url": "https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.1.0",\n'
            '  "draft": false,\n'
            '  "prerelease": false,\n'
            '  "assets": [\n'
            '    {"name": "demo.vnpak", "browser_download_url": "https://github.com/AvrovaDonz2026/n64gal/releases/download/v1.1.0/demo.vnpak", "size": 1853},\n'
            '    {"name": "content-demo.vnpak", "browser_download_url": "https://github.com/AvrovaDonz2026/n64gal/releases/download/v1.1.0/content-demo.vnpak", "size": 12}\n'
            '  ]\n'
            '}\n',
        )

        missing_content_proc = subprocess.run(
            SCRIPT + ["--out-dir", str(temp_root / "missing-content"), "--release-spec", str(release_spec)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if missing_content_proc.returncode != 2 or "trace_id=release.export.content_soak.required" not in missing_content_proc.stderr:
            print("v1.1 release export accepted missing real-content soak evidence", file=sys.stderr)
            return 1

        proc = subprocess.run(
            SCRIPT + [
                "--out-dir", str(out_dir),
                "--release-spec", str(release_spec),
                "--gate-summary", str(gate_summary),
                "--soak-summary", str(soak_summary),
                "--content-soak-summary", str(content_soak_summary),
                "--content-soak-summary-json", str(content_soak_summary_json),
                "--ci-suite-summary", str(ci_summary),
                "--host-sdk-summary", str(host_sdk_summary),
                "--host-sdk-summary-json", str(host_sdk_summary_json),
                "--platform-evidence-summary", str(platform_summary),
                "--platform-evidence-summary-json", str(platform_summary_json),
                "--preview-evidence-summary", str(preview_summary),
                "--preview-evidence-summary-json", str(preview_summary_json),
                "--remote-release-json", str(remote_release_json),
                "--remote-api-root", "https://invalid.invalid",
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            print(f"release export failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
            return 1
        if "trace_id=release.export.ok" not in proc.stdout:
            print("missing release export success trace", file=sys.stderr)
            return 1

        if not (out_dir / "bundle" / "release_bundle_manifest.json").exists():
            print("release export missing bundle manifest", file=sys.stderr)
            return 1
        if not (out_dir / "bundle" / "summaries" / "release_report.json").exists():
            print("release export bundle missing report json", file=sys.stderr)
            return 1
        if not (out_dir / "bundle" / "summaries" / "release_publish_map.json").exists():
            print("release export bundle missing publish map json", file=sys.stderr)
            return 1
        if not (out_dir / "bundle" / "summaries" / "release_remote_summary.json").exists():
            print("release export bundle missing remote summary json", file=sys.stderr)
            return 1
        if not (out_dir / "bundle" / "summaries" / "content_soak_summary.md").exists() or not (out_dir / "bundle" / "summaries" / "content_soak_summary.json").exists():
            print("release export bundle missing real-content soak summaries", file=sys.stderr)
            return 1
        if not (out_dir / "report" / "release_report.json").exists():
            print("release export missing report json", file=sys.stderr)
            return 1
        if not (out_dir / "publish" / "release_publish_map.json").exists():
            print("release export missing publish map json", file=sys.stderr)
            return 1
        if not (out_dir / "remote" / "release_remote_summary.json").exists():
            print("release export missing remote summary json", file=sys.stderr)
            return 1
        if not (out_dir / "release_export_summary.json").exists():
            print("release export missing summary json", file=sys.stderr)
            return 1
        bundle_index = (out_dir / "bundle" / "release_bundle_index.md").read_text(encoding="utf-8")
        if "`docs/release-v1.1.0.md`" not in bundle_index or "`content-demo.vnpak`" not in bundle_index:
            print("release export bundle index missing v1.1.0 materials", file=sys.stderr)
            return 1
        if "summaries/content_soak_summary.json" not in bundle_index:
            print("release export bundle index missing real-content soak summary", file=sys.stderr)
            return 1
        report_payload = json.loads((out_dir / "report" / "release_report.json").read_text(encoding="utf-8"))
        if report_payload.get("content_soak_summary_json") != str(content_soak_summary_json):
            print("release export report missing real-content soak JSON reference", file=sys.stderr)
            return 1
        export_payload = json.loads((out_dir / "release_export_summary.json").read_text(encoding="utf-8"))
        if export_payload.get("content_soak_summary") != str(content_soak_summary):
            print("release export summary did not safely encode the content evidence path", file=sys.stderr)
            return 1

    print("test_release_export_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
