#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_export.sh"]


def write_text(path: Path, text: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def main():
    with tempfile.TemporaryDirectory(prefix="n64gal_release_export_") as temp_dir:
        temp_root = Path(temp_dir)
        out_dir = temp_root / "export"
        gate_summary = temp_root / "release_gate_summary.md"
        soak_summary = temp_root / "demo_soak_summary.md"
        ci_summary = temp_root / "ci_suite_summary.md"
        host_sdk_summary = temp_root / "host_sdk_smoke_summary.md"
        host_sdk_summary_json = temp_root / "host_sdk_smoke_summary.json"
        platform_summary = temp_root / "platform_evidence_summary.md"
        platform_summary_json = temp_root / "platform_evidence_summary.json"
        preview_summary = temp_root / "preview_evidence_summary.md"
        preview_summary_json = temp_root / "preview_evidence_summary.json"

        write_text(gate_summary, "# Release Gate Summary\n")
        write_text(soak_summary, "# Demo Soak Summary\n")
        write_text(ci_summary, "# CI Suite Summary\n\n- Status: `success`\n")
        write_text(host_sdk_summary, "# Host SDK Smoke Summary\n")
        write_text(host_sdk_summary_json, '{"status":"ok"}\n')
        write_text(platform_summary, "# Platform Evidence Summary\n")
        write_text(platform_summary_json, '{"status":"ok"}\n')
        write_text(preview_summary, "# Preview Evidence Summary\n")
        write_text(preview_summary_json, '{"status":"ok"}\n')

        proc = subprocess.run(
            SCRIPT + [
                "--out-dir", str(out_dir),
                "--gate-summary", str(gate_summary),
                "--soak-summary", str(soak_summary),
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
            print(f"release export failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
            return 1
        if "trace_id=release.export.ok" not in proc.stdout:
            print("missing release export success trace", file=sys.stderr)
            return 1

        if not (out_dir / "bundle" / "release_bundle_manifest.json").exists():
            print("release export missing bundle manifest", file=sys.stderr)
            return 1
        if not (out_dir / "report" / "release_report.json").exists():
            print("release export missing report json", file=sys.stderr)
            return 1
        if not (out_dir / "publish" / "release_publish_map.json").exists():
            print("release export missing publish map json", file=sys.stderr)
            return 1
        if not (out_dir / "release_export_summary.json").exists():
            print("release export missing summary json", file=sys.stderr)
            return 1

    print("test_release_export_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
