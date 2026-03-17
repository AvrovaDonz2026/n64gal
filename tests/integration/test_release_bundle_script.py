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
        gate_summary = Path(temp_dir) / "release_gate_summary.md"
        soak_summary = Path(temp_dir) / "demo_soak_summary.md"
        ci_summary = Path(temp_dir) / "ci_suite_summary.md"
        host_sdk_summary = Path(temp_dir) / "host_sdk_smoke_summary.md"
        host_sdk_summary_json = Path(temp_dir) / "host_sdk_smoke_summary.json"
        platform_summary = Path(temp_dir) / "platform_evidence_summary.md"
        platform_summary_json = Path(temp_dir) / "platform_evidence_summary.json"
        preview_summary = Path(temp_dir) / "preview_evidence_summary.md"
        preview_summary_json = Path(temp_dir) / "preview_evidence_summary.json"

        write_text(gate_summary, "# Release Gate Summary\n\n- Status: `success`\n")
        write_text(soak_summary, "# Demo Soak Summary\n\n- Status: `success`\n")
        write_text(ci_summary, "# CI Suite Summary\n\n- Status: `success`\n")
        write_text(host_sdk_summary, "# Host SDK Smoke Summary\n\n- Status: `success`\n")
        write_text(host_sdk_summary_json, "{\n  \"status\": \"success\"\n}\n")
        write_text(platform_summary, "# Platform Evidence Summary\n\n- Status: `success`\n")
        write_text(platform_summary_json, "{\n  \"status\": \"success\"\n}\n")
        write_text(preview_summary, "# Preview Evidence Summary\n\n- Status: `success`\n")
        write_text(preview_summary_json, "{\n  \"status\": \"success\"\n}\n")

        proc = subprocess.run(
            SCRIPT + [
                "--out-dir", str(out_dir),
                "--release-gate-summary", str(gate_summary),
                "--demo-soak-summary", str(soak_summary),
                "--ci-suite-summary", str(ci_summary),
                "--host-sdk-summary", str(host_sdk_summary),
                "--host-sdk-summary-json", str(host_sdk_summary_json),
                "--platform-evidence-summary", str(platform_summary),
                "--platform-evidence-summary-json", str(platform_summary_json),
                "--preview-evidence-summary", str(preview_summary),
                "--preview-evidence-summary-json", str(preview_summary_json),
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
        if not index_path.exists():
            print("release bundle index missing", file=sys.stderr)
            return 1
        if not index_json_path.exists():
            print("release bundle json index missing", file=sys.stderr)
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
        index_json_text = index_json_path.read_text(encoding="utf-8")
        if '"summaries/release_gate_summary.md"' not in index_json_text:
            print("release bundle json missing gate summary", file=sys.stderr)
            return 1
        if '"summaries/host_sdk_smoke_summary.json"' not in index_json_text:
            print("release bundle json missing host sdk json summary", file=sys.stderr)
            return 1
        if not (out_dir / "demo.vnpak").exists():
            print("release bundle missing demo.vnpak", file=sys.stderr)
            return 1

    print("test_release_bundle_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
