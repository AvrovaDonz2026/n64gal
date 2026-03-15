#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_report.sh"]


def main():
    out_dir = ROOT / "tests" / "integration" / "release_report_tmp"
    report_json = out_dir / "release_report.json"
    bundle_index = ROOT / "tests" / "integration" / "release_report_bundle.md"
    gate_summary = ROOT / "tests" / "integration" / "release_report_gate.md"
    soak_summary = ROOT / "tests" / "integration" / "release_report_soak.md"
    ci_summary = ROOT / "tests" / "integration" / "release_report_ci.md"

    if out_dir.exists():
        import shutil
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    for path in (bundle_index, gate_summary, soak_summary, ci_summary):
        try:
            path.unlink()
        except FileNotFoundError:
            pass
        path.write_text(f"# {path.name}\n", encoding="utf-8")

    proc = subprocess.run(
        SCRIPT + [
            "--out-dir", str(out_dir),
            "--bundle-index", str(bundle_index),
            "--gate-summary", str(gate_summary),
            "--soak-summary", str(soak_summary),
            "--ci-suite-summary", str(ci_summary),
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

    import shutil
    shutil.rmtree(out_dir)
    for path in (bundle_index, gate_summary, soak_summary, ci_summary):
        path.unlink()

    print("test_release_report_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
