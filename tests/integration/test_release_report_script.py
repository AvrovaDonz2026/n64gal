#!/usr/bin/env python3
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_report.sh"]


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
    out_dir = ROOT / "tests" / "integration" / "release_report_tmp"
    report_json = out_dir / "release_report.json"
    release_spec = ROOT / "tests" / "integration" / "release_report_spec.json"
    bundle_index = ROOT / "tests" / "integration" / "release_report_bundle.md"
    bundle_manifest = ROOT / "tests" / "integration" / "release_report_bundle_manifest.json"
    gate_summary = ROOT / "tests" / "integration" / "release_report_gate.md"
    soak_summary = ROOT / "tests" / "integration" / "release_report_soak.md"
    content_soak_summary = ROOT / "tests" / "integration" / "release_report_content_soak.md"
    content_soak_summary_json = ROOT / "tests" / "integration" / "release_report_content_soak.json"
    ci_summary = ROOT / "tests" / "integration" / "release_report_ci.md"
    host_sdk_summary = ROOT / "tests" / "integration" / "release_report_host_sdk.md"
    platform_summary = ROOT / "tests" / "integration" / "release_report_platform.md"
    preview_summary = ROOT / "tests" / "integration" / "release_report_preview.md"
    release_note = ROOT / "tests" / "integration" / "release-v1.0.0.md"
    release_evidence = ROOT / "tests" / "integration" / "release-evidence-v1.0.0.md"
    release_package = ROOT / "tests" / "integration" / "release-package-v1.0.0.md"
    release_checklist = ROOT / "tests" / "integration" / "release-checklist-v1.0.0.md"
    content_demo = ROOT / "tests" / "integration" / "release_report_content-demo.vnpak"

    if out_dir.exists():
        import shutil
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    for path in (bundle_index, bundle_manifest, gate_summary, soak_summary, content_soak_summary, content_soak_summary_json, ci_summary, host_sdk_summary, platform_summary, preview_summary, release_spec, release_note, release_evidence, release_package, release_checklist, content_demo):
        try:
            path.unlink()
        except FileNotFoundError:
            pass
        if path.suffix == ".json":
            if path == release_spec:
                path.write_text(
                    '{"version":"v1.0.0","tag":"v1.0.0","release_url":"https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.0.0","release_note":"%s","assets":[{"name":"demo.vnpak","path":"assets/demo/demo.vnpak"},{"name":"content-demo.vnpak","path":"%s"}]}\n'
                    % (release_note.relative_to(ROOT), content_demo.relative_to(ROOT)),
                    encoding="utf-8",
                )
            else:
                path.write_text('{"files":[{"path":"demo.vnpak","sha256":"deadbeef","bytes":1}]}\n', encoding="utf-8")
        else:
            path.write_text(f"# {path.name}\n", encoding="utf-8")
    content_soak_summary.write_text("# Demo Soak Summary\n\n- Status: `success`\n- Scenes: `Opening,Gallery`\n", encoding="utf-8")
    content_soak_summary_json.write_text(json.dumps(content_soak_payload()) + "\n", encoding="utf-8")
    content_demo.write_bytes(b"content-demo")

    proc = subprocess.run(
        SCRIPT + [
            "--out-dir", str(out_dir),
            "--release-spec", str(release_spec),
            "--bundle-index", str(bundle_index),
            "--bundle-manifest", str(bundle_manifest),
            "--gate-summary", str(gate_summary),
            "--soak-summary", str(soak_summary),
            "--content-soak-summary", str(content_soak_summary),
            "--content-soak-summary-json", str(content_soak_summary_json),
            "--ci-suite-summary", str(ci_summary),
            "--host-sdk-summary", str(host_sdk_summary),
            "--platform-evidence-summary", str(platform_summary),
            "--preview-evidence-summary", str(preview_summary),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"release report failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1
    if "trace_id=release.report.ok" not in proc.stdout:
        print("missing release report success trace", file=sys.stderr)
        return 1

    report_path = out_dir / "release_report.md"
    if not report_path.exists():
        print("release report missing", file=sys.stderr)
        return 1
    if not report_json.exists():
        print("release report json missing", file=sys.stderr)
        return 1

    report_text = report_path.read_text(encoding="utf-8")
    if "# Release Report" not in report_text:
        print("release report header missing", file=sys.stderr)
        return 1
    if "docs/perf-report.md" not in report_text:
        print("release report missing perf doc index", file=sys.stderr)
        return 1
    if str(bundle_manifest) not in report_text:
        print("release report missing bundle manifest reference", file=sys.stderr)
        return 1
    if str(release_spec) not in report_text:
        print("release report missing release spec reference", file=sys.stderr)
        return 1
    if str(release_note.relative_to(ROOT)) not in report_text or str(release_evidence.relative_to(ROOT)) not in report_text:
        print("release report missing spec-driven release doc references", file=sys.stderr)
        return 1
    if str(host_sdk_summary) not in report_text or str(platform_summary) not in report_text or str(preview_summary) not in report_text:
            print("release report missing release-facing evidence references", file=sys.stderr)
            return 1
    if str(content_soak_summary) not in report_text or str(content_soak_summary_json) not in report_text:
        print("release report missing real-content soak references", file=sys.stderr)
        return 1

    report_payload = json.loads(report_json.read_text(encoding="utf-8"))
    if not report_payload.get("release_spec") or not report_payload.get("release_note") or not report_payload.get("release_evidence"):
        print("release report json missing spec-driven release doc fields", file=sys.stderr)
        return 1
    if report_payload.get("content_soak_summary") != str(content_soak_summary) or report_payload.get("content_soak_summary_json") != str(content_soak_summary_json):
        print("release report json missing real-content soak fields", file=sys.stderr)
        return 1

    import shutil
    shutil.rmtree(out_dir)
    for path in (bundle_index, bundle_manifest, gate_summary, soak_summary, content_soak_summary, content_soak_summary_json, ci_summary, host_sdk_summary, platform_summary, preview_summary, release_spec, release_note, release_evidence, release_package, release_checklist, content_demo):
        path.unlink()

    print("test_release_report_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
