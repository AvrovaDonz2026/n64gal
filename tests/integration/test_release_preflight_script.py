#!/usr/bin/env python3
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_preflight.sh"]


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
    missing_summary = subprocess.run(
        SCRIPT + ["--allow-dirty", "--skip-cc-suite"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if missing_summary.returncode != 2 or "trace_id=release.preflight.ci_summary.required" not in missing_summary.stderr:
        print("release preflight accepted --skip-cc-suite without an explicit summary", file=sys.stderr)
        return 1

    workflow_text = (ROOT / ".github" / "workflows" / "release-preflight.yml").read_text(encoding="utf-8")
    required_workflow_text = (
        "- name: Native CC suite",
        "run: ./scripts/ci/run_cc_suite.sh",
        "--skip-cc-suite",
        "--ci-suite-summary build_ci_cc/ci_suite_summary.md",
        "--content-soak-summary build_release_preflight/content_soak_summary.md",
        "--content-soak-summary-json build_release_preflight/content_soak_summary.json",
    )
    if any(token not in workflow_text for token in required_workflow_text):
        print("release preflight workflow does not produce and pass the CI suite summary", file=sys.stderr)
        return 1
    if workflow_text.index("- name: Real-content soak") > workflow_text.index("- name: Release preflight"):
        print("release preflight workflow runs real-content evidence after the dependent export", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_release_preflight_") as temp_dir:
        out_dir = Path(temp_dir) / "preflight"
        ci_summary = Path(temp_dir) / "ci_suite_summary.md"
        content_soak_summary = Path(temp_dir) / 'content"evidence' / "content_soak_summary.md"
        content_soak_summary_json = Path(temp_dir) / 'content"evidence' / "content_soak_summary.json"
        release_spec = Path(temp_dir) / "release_spec.json"
        release_note = Path(temp_dir) / "release-v1.0.0.md"
        release_evidence = Path(temp_dir) / "release-evidence-v1.0.0.md"
        release_package = Path(temp_dir) / "release-package-v1.0.0.md"
        release_checklist = Path(temp_dir) / "release-checklist-v1.0.0.md"
        content_demo = Path(temp_dir) / "content-demo.vnpak"
        remote_release_json = Path(temp_dir) / "github_release_v1.0.0.json"
        ci_summary.write_text("# CI Suite Summary\n\n- Status: `success`\n", encoding="utf-8")
        content_soak_summary.parent.mkdir(parents=True, exist_ok=True)
        content_soak_summary.write_text("# Demo Soak Summary\n\n- Status: `success`\n- Scenes: `Opening,Gallery`\n", encoding="utf-8")
        content_soak_summary_json.write_text(json.dumps(content_soak_payload()) + "\n", encoding="utf-8")
        release_note.write_text("# N64GAL v1.0.0\n", encoding="utf-8")
        release_evidence.write_text("# Release Evidence: v1.0.0\n", encoding="utf-8")
        release_package.write_text("# Release Package Plan: v1.0.0\n", encoding="utf-8")
        release_checklist.write_text("# Release Checklist: v1.0.0\n", encoding="utf-8")
        content_demo.write_bytes(b"content-demo")
        remote_release_json.write_text(
            '{\n'
            '  "tag_name": "v1.0.0",\n'
            '  "html_url": "https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.0.0",\n'
            '  "draft": false,\n'
            '  "prerelease": false,\n'
            '  "assets": [\n'
            '    {"name": "demo.vnpak", "browser_download_url": "https://github.com/AvrovaDonz2026/n64gal/releases/download/v1.0.0/demo.vnpak", "size": 1853},\n'
            '    {"name": "content-demo.vnpak", "browser_download_url": "https://github.com/AvrovaDonz2026/n64gal/releases/download/v1.0.0/content-demo.vnpak", "size": 12}\n'
            '  ]\n'
            '}\n',
            encoding="utf-8",
        )
        release_spec.write_text(
            '{"version":"v1.0.0","tag":"v1.0.0","release_url":"https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.0.0","draft":false,"prerelease":false,"release_note":"%s","assets":[{"name":"demo.vnpak","path":"%s"},{"name":"content-demo.vnpak","path":"%s"}]}\n'
            % (release_note, ROOT / "assets" / "demo" / "demo.vnpak", content_demo),
            encoding="utf-8",
        )
        proc = subprocess.run(
            SCRIPT
            + [
                "--allow-dirty",
                "--skip-cc-suite",
                "--out-dir",
                str(out_dir),
                "--release-spec",
                str(release_spec),
                "--ci-suite-summary",
                str(ci_summary),
                "--content-soak-summary",
                str(content_soak_summary),
                "--content-soak-summary-json",
                str(content_soak_summary_json),
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
        if not (out_dir / "export" / "bundle" / "summaries" / "content_soak_summary.json").exists():
            print("release preflight export missing real-content soak summary", file=sys.stderr)
            return 1
        preflight_text = (out_dir / "release_preflight_summary.md").read_text(encoding="utf-8")
        if str(content_soak_summary) not in preflight_text or str(content_soak_summary_json) not in preflight_text:
            print("release preflight summary missing real-content soak references", file=sys.stderr)
            return 1
        preflight_payload = json.loads((out_dir / "release_preflight_summary.json").read_text(encoding="utf-8"))
        gate_payload = json.loads((out_dir / "gate" / "release_gate_summary.json").read_text(encoding="utf-8"))
        if preflight_payload.get("content_soak_summary") != str(content_soak_summary) or gate_payload.get("content_soak_summary_json") != str(content_soak_summary_json):
            print("release preflight/gate JSON did not safely encode content evidence paths", file=sys.stderr)
            return 1
        bundle_index = (out_dir / "export" / "bundle" / "release_bundle_index.md").read_text(encoding="utf-8")
        if "`docs/release-v1.0.0.md`" not in bundle_index:
            print("release preflight bundle index missing v1.0.0 note", file=sys.stderr)
            return 1

    print("test_release_preflight_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
