#!/usr/bin/env python3
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_bundle.sh"]


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
    with tempfile.TemporaryDirectory(prefix="n64gal_release_bundle_") as temp_dir:
        out_dir = Path(temp_dir) / 'bundle"quoted'
        release_spec = Path(temp_dir) / "release_spec.json"
        gate_summary = Path(temp_dir) / "release_gate_summary.md"
        soak_summary = Path(temp_dir) / "demo_soak_summary.md"
        content_soak_summary = Path(temp_dir) / "content_soak_summary.md"
        content_soak_summary_json = Path(temp_dir) / "content_soak_summary.json"
        ci_summary = Path(temp_dir) / "ci_suite_summary.md"
        host_sdk_summary = Path(temp_dir) / "host_sdk_smoke_summary.md"
        host_sdk_summary_json = Path(temp_dir) / "host_sdk_smoke_summary.json"
        platform_summary = Path(temp_dir) / "platform_evidence_summary.md"
        platform_summary_json = Path(temp_dir) / "platform_evidence_summary.json"
        preview_summary = Path(temp_dir) / "preview_evidence_summary.md"
        preview_summary_json = Path(temp_dir) / "preview_evidence_summary.json"
        preview_request = Path(temp_dir) / "preview_request.txt"
        preview_response = Path(temp_dir) / "preview_response.json"
        preview_screenshot = Path(temp_dir) / "content_preview.ppm"
        report_md = Path(temp_dir) / "release_report.md"
        report_json = Path(temp_dir) / "release_report.json"
        publish_map_md = Path(temp_dir) / "release_publish_map.md"
        publish_map_json = Path(temp_dir) / "release_publish_map.json"
        remote_summary_md = Path(temp_dir) / "release_remote_summary.md"
        remote_summary_json = Path(temp_dir) / "release_remote_summary.json"
        release_note = Path(temp_dir) / "release-v1.0.0.md"
        release_evidence = Path(temp_dir) / "release-evidence-v1.0.0.md"
        release_package = Path(temp_dir) / "release-package-v1.0.0.md"
        release_checklist = Path(temp_dir) / "release-checklist-v1.0.0.md"
        content_demo = Path(temp_dir) / "content-demo.vnpak"

        write_text(
            release_spec,
            '{"version":"v1.0.0","tag":"v1.0.0","release_url":"https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.0.0","release_note":"%s","release_evidence":"%s","release_package":"%s","release_checklist":"%s","assets":[{"name":"demo.vnpak","path":"%s"},{"name":"content-demo.vnpak","path":"%s"}]}\n'
            % (release_note, release_evidence, release_package, release_checklist, ROOT / "assets" / "demo" / "demo.vnpak", content_demo),
        )
        write_text(gate_summary, "# Release Gate Summary\n\n- Status: `success`\n")
        write_text(soak_summary, "# Demo Soak Summary\n\n- Status: `success`\n")
        write_text(content_soak_summary, "# Demo Soak Summary\n\n- Status: `success`\n- Scenes: `Opening,Gallery`\n")
        write_text(content_soak_summary_json, json.dumps(content_soak_payload()) + "\n")
        write_text(ci_summary, "# CI Suite Summary\n\n- Status: `success`\n")
        write_text(host_sdk_summary, "# Host SDK Smoke Summary\n\n- Status: `success`\n")
        write_text(host_sdk_summary_json, "{\n  \"status\": \"success\"\n}\n")
        write_text(platform_summary, "# Platform Evidence Summary\n\n- Status: `success`\n")
        write_text(platform_summary_json, "{\n  \"status\": \"success\"\n}\n")
        write_text(preview_summary, "# Preview Evidence Summary\n\n- Status: `success`\n")
        write_text(preview_request, "scene_name=Opening\n")
        write_text(preview_response, '{"status":"ok"}\n')
        preview_screenshot.write_bytes(b"P6\n1 1\n255\n\x00\x00\x00")
        write_text(
            preview_summary_json,
            json.dumps(
                {
                    "status": "success",
                    "request": str(preview_request),
                    "response": str(preview_response),
                    "screenshot": str(preview_screenshot),
                },
                indent=2,
            ) + "\n",
        )
        write_text(report_md, "# Release Report\n")
        write_text(report_json, "{\n  \"report_md\": \"release_report.md\"\n}\n")
        write_text(publish_map_md, "# Release Publish Map\n")
        write_text(publish_map_json, "{\n  \"tag\": \"v0.1.0-alpha\"\n}\n")
        write_text(remote_summary_md, "# Release Remote Summary\n")
        write_text(remote_summary_json, "{\n  \"tag\": \"v0.1.0-alpha\"\n}\n")
        write_text(release_note, "# N64GAL v1.0.0\n")
        write_text(release_evidence, "# Release Evidence: v1.0.0\n")
        write_text(release_package, "# Release Package Plan: v1.0.0\n")
        write_text(release_checklist, "# Release Checklist: v1.0.0\n")
        content_demo.write_bytes(b"content-demo")

        proc = subprocess.run(
            SCRIPT + [
                "--out-dir", str(out_dir),
                "--release-spec", str(release_spec),
                "--release-gate-summary", str(gate_summary),
                "--demo-soak-summary", str(soak_summary),
                "--content-soak-summary", str(content_soak_summary),
                "--content-soak-summary-json", str(content_soak_summary_json),
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
        if "summaries/content_soak_summary.md" not in index_text or "summaries/content_soak_summary.json" not in index_text:
            print("release bundle index missing real-content soak summaries", file=sys.stderr)
            return 1
        if "summaries/release_report.json" not in index_text or "summaries/release_publish_map.json" not in index_text:
            print("release bundle index missing derived artifacts", file=sys.stderr)
            return 1
        if "summaries/release_remote_summary.json" not in index_text:
            print("release bundle index missing remote summary artifact", file=sys.stderr)
            return 1
        index_payload = json.loads(index_json_path.read_text(encoding="utf-8"))
        index_summaries = set(index_payload.get("summaries", []))
        if "summaries/release_gate_summary.md" not in index_summaries:
            print("release bundle json missing gate summary", file=sys.stderr)
            return 1
        if "summaries/host_sdk_smoke_summary.json" not in index_summaries:
            print("release bundle json missing host sdk json summary", file=sys.stderr)
            return 1
        if "summaries/content_soak_summary.json" not in index_summaries:
            print("release bundle json missing real-content soak summary", file=sys.stderr)
            return 1
        if "evidence/preview/content_preview.ppm" not in index_payload.get("evidence", []):
            print("release bundle json missing preview evidence artifacts", file=sys.stderr)
            return 1
        if "summaries/release_report.json" not in index_summaries or "summaries/release_publish_map.json" not in index_summaries:
            print("release bundle json missing derived artifact references", file=sys.stderr)
            return 1
        if "summaries/release_remote_summary.json" not in index_summaries:
            print("release bundle json missing remote summary reference", file=sys.stderr)
            return 1
        if index_payload.get("out_dir") != str(out_dir):
            print("release bundle index did not safely encode the output path", file=sys.stderr)
            return 1
        manifest_text = manifest_path.read_text(encoding="utf-8")
        if "release_bundle_manifest.json" not in index_text or "`demo.vnpak`" not in manifest_text or "`content-demo.vnpak`" not in manifest_text:
            print("release bundle manifest contents missing", file=sys.stderr)
            return 1
        if "`docs/release-v1.0.0.md`" not in index_text or "`docs/release-evidence-v1.0.0.md`" not in index_text:
            print("release bundle index missing versioned release docs", file=sys.stderr)
            return 1
        manifest_payload = json.loads(manifest_json_path.read_text(encoding="utf-8"))
        manifest_entries = {item.get("path"): item for item in manifest_payload.get("files", [])}
        if "demo.vnpak" not in manifest_entries or "content-demo.vnpak" not in manifest_entries or len(manifest_entries["content-demo.vnpak"].get("sha256", "")) != 64:
            print("release bundle manifest json missing digest entry", file=sys.stderr)
            return 1
        if "evidence/preview/content_preview.ppm" not in manifest_entries:
            print("release bundle manifest json missing preview screenshot", file=sys.stderr)
            return 1
        if "summaries/content_soak_summary.md" not in manifest_entries or "summaries/content_soak_summary.json" not in manifest_entries:
            print("release bundle manifest missing real-content soak summaries", file=sys.stderr)
            return 1
        if manifest_payload.get("out_dir") != str(out_dir):
            print("release bundle manifest did not safely encode the output path", file=sys.stderr)
            return 1
        if not (out_dir / "demo.vnpak").exists():
            print("release bundle missing demo.vnpak", file=sys.stderr)
            return 1
        if not (out_dir / "content-demo.vnpak").exists():
            print("release bundle missing content-demo.vnpak", file=sys.stderr)
            return 1
        if not (out_dir / "summaries" / "content_soak_summary.md").exists() or not (out_dir / "summaries" / "content_soak_summary.json").exists():
            print("release bundle missing real-content soak files", file=sys.stderr)
            return 1
        if not (out_dir / "evidence" / "preview" / "preview_request.txt").exists():
            print("release bundle missing preview request evidence", file=sys.stderr)
            return 1
        if not (out_dir / "evidence" / "preview" / "preview_response.json").exists():
            print("release bundle missing preview response evidence", file=sys.stderr)
            return 1
        if not (out_dir / "evidence" / "preview" / "content_preview.ppm").exists():
            print("release bundle missing preview screenshot evidence", file=sys.stderr)
            return 1

        invalid_payload = content_soak_payload()
        invalid_payload["total_simulated_ms"] = 64
        write_text(content_soak_summary_json, json.dumps(invalid_payload) + "\n")
        invalid_proc = subprocess.run(
            ["python3", "tools/validate/validate_content_soak_summary.py", str(content_soak_summary_json), "--summary-md", str(content_soak_summary), "--pack", str(content_demo)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if invalid_proc.returncode == 0 or "trace_id=tool.validate.content_soak.failed" not in invalid_proc.stderr:
            print("content soak validator accepted shortened evidence", file=sys.stderr)
            return 1
        invalid_payload = content_soak_payload()
        invalid_payload["pack_sha256"] = "1" * 64
        write_text(content_soak_summary_json, json.dumps(invalid_payload) + "\n")
        invalid_proc = subprocess.run(
            ["python3", "tools/validate/validate_content_soak_summary.py", str(content_soak_summary_json), "--summary-md", str(content_soak_summary), "--pack", str(content_demo)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if invalid_proc.returncode == 0 or "field=pack_sha256" not in invalid_proc.stderr:
            print("content soak validator accepted a stale pack digest", file=sys.stderr)
            return 1

    print("test_release_bundle_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
