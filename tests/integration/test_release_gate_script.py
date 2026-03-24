#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_gate.sh"]


def main():
    summary_path = ROOT / "tests" / "integration" / "release_gate_tmp.md"
    summary_json_path = ROOT / "tests" / "integration" / "release_gate_tmp.json"
    soak_summary_path = ROOT / "tests" / "integration" / "release_gate_soak_tmp.md"
    soak_summary_json_path = ROOT / "tests" / "integration" / "release_gate_soak_tmp.json"
    summary_path_runner = ROOT / "tests" / "integration" / "release_gate_runner_tmp.md"
    summary_json_path_runner = ROOT / "tests" / "integration" / "release_gate_runner_tmp.json"
    soak_summary_path_runner = ROOT / "tests" / "integration" / "release_gate_soak_runner_tmp.md"
    soak_summary_json_path_runner = ROOT / "tests" / "integration" / "release_gate_soak_runner_tmp.json"
    ci_summary_path = ROOT / "tests" / "integration" / "release_gate_ci_suite_summary.md"
    runner_bin = ROOT / "build_release_soak" / "vn_player"
    bundle_dir = ROOT / "tests" / "integration" / "release_gate_bundle_tmp"
    export_dir = ROOT / "tests" / "integration" / "release_gate_export_tmp"
    remote_fixture = ROOT / "tests" / "fixtures" / "release_api" / "github_release_v0.1.0-alpha.json"
    for path in (
        summary_path,
        summary_json_path,
        soak_summary_path,
        soak_summary_json_path,
        summary_path_runner,
        summary_json_path_runner,
        soak_summary_path_runner,
        soak_summary_json_path_runner,
        ci_summary_path,
    ):
        try:
            if path.exists():
                path.unlink()
        except FileNotFoundError:
            pass
    if bundle_dir.exists():
        import shutil
        shutil.rmtree(bundle_dir)
    if export_dir.exists():
        import shutil
        shutil.rmtree(export_dir)
    ci_summary_path.write_text("# CI Suite Summary\n\n- Status: `success`\n", encoding="utf-8")

    proc = subprocess.run(
        SCRIPT + [
            "--allow-dirty",
            "--skip-cc-suite",
            "--with-soak",
            "--soak-frames-per-scene", "2",
            "--soak-scenes", "S0",
            "--soak-summary-out", str(soak_summary_path),
            "--soak-summary-json-out", str(soak_summary_json_path),
            "--summary-out", str(summary_path),
            "--summary-json-out", str(summary_json_path),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"release gate script failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1
    if "trace_id=release.gate.ok" not in proc.stdout:
        print("missing release gate success trace", file=sys.stderr)
        return 1
    if not summary_path.exists():
        print("release gate summary missing", file=sys.stderr)
        return 1
    if not summary_json_path.exists():
        print("release gate json summary missing", file=sys.stderr)
        return 1

    summary_text = summary_path.read_text(encoding="utf-8")
    if "# Release Gate Summary" not in summary_text:
        print("release gate summary header missing", file=sys.stderr)
        return 1
    if "validate-all" not in summary_text:
        print("release gate summary missing validate-all step", file=sys.stderr)
        return 1
    if "with_soak" not in summary_text:
        print("release gate summary missing soak flag", file=sys.stderr)
        return 1
    if "## Soak Summary" not in summary_text:
        print("release gate summary missing embedded soak summary", file=sys.stderr)
        return 1
    if not soak_summary_path.exists():
        print("release gate soak summary missing", file=sys.stderr)
        return 1
    if not soak_summary_json_path.exists():
        print("release gate soak json summary missing", file=sys.stderr)
        return 1

    proc = subprocess.run(
        SCRIPT + [
            "--allow-dirty",
            "--skip-cc-suite",
            "--with-soak",
            "--soak-skip-build",
            "--soak-skip-pack",
            "--soak-runner-bin", str(runner_bin),
            "--soak-frames-per-scene", "2",
            "--soak-scenes", "S0",
            "--soak-summary-out", str(soak_summary_path_runner),
            "--soak-summary-json-out", str(soak_summary_json_path_runner),
            "--summary-out", str(summary_path_runner),
            "--summary-json-out", str(summary_json_path_runner),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"release gate runner-bin failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1
    if not summary_path_runner.exists() or not summary_json_path_runner.exists():
        print("release gate runner-bin gate summaries missing", file=sys.stderr)
        return 1
    if not soak_summary_path_runner.exists() or not soak_summary_json_path_runner.exists():
        print("release gate runner-bin soak summaries missing", file=sys.stderr)
        return 1

    proc = subprocess.run(
        SCRIPT + [
            "--allow-dirty",
            "--skip-cc-suite",
            "--with-soak",
            "--with-bundle",
            "--soak-skip-build",
            "--soak-skip-pack",
            "--soak-runner-bin", str(runner_bin),
            "--soak-frames-per-scene", "2",
            "--soak-scenes", "S0",
            "--soak-summary-json-out", str(soak_summary_json_path_runner),
            "--summary-out", str(summary_path_runner),
            "--summary-json-out", str(summary_json_path_runner),
            "--ci-suite-summary", str(ci_summary_path),
            "--bundle-out-dir", str(bundle_dir),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"release gate bundle failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1
    if not (bundle_dir / "release_bundle_index.md").exists():
        print("release gate bundle index missing", file=sys.stderr)
        return 1

    proc = subprocess.run(
        SCRIPT + [
            "--allow-dirty",
            "--skip-cc-suite",
            "--with-soak",
            "--with-export",
            "--soak-skip-build",
            "--soak-skip-pack",
            "--soak-runner-bin", str(runner_bin),
            "--soak-frames-per-scene", "2",
            "--soak-scenes", "S0",
            "--summary-out", str(summary_path_runner),
            "--summary-json-out", str(summary_json_path_runner),
            "--ci-suite-summary", str(ci_summary_path),
            "--export-out-dir", str(export_dir),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"release gate export failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1
    if not (export_dir / "release_export_summary.md").exists():
        print("release gate export summary missing", file=sys.stderr)
        return 1

    proc = subprocess.run(
        SCRIPT + [
            "--allow-dirty",
            "--skip-cc-suite",
            "--with-soak",
            "--with-export",
            "--soak-skip-build",
            "--soak-skip-pack",
            "--soak-runner-bin", str(runner_bin),
            "--soak-frames-per-scene", "2",
            "--soak-scenes", "S0",
            "--summary-out", str(summary_path_runner),
            "--summary-json-out", str(summary_json_path_runner),
            "--ci-suite-summary", str(ci_summary_path),
            "--export-out-dir", str(export_dir),
            "--remote-release-json", str(remote_fixture),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"release gate remote export failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1
    if not (export_dir / "remote" / "release_remote_summary.json").exists():
        print("release gate remote export summary missing", file=sys.stderr)
        return 1

    summary_path.unlink()
    summary_json_path.unlink()
    soak_summary_path.unlink()
    soak_summary_json_path.unlink()
    summary_path_runner.unlink()
    summary_json_path_runner.unlink()
    soak_summary_path_runner.unlink()
    soak_summary_json_path_runner.unlink()
    ci_summary_path.unlink()
    import shutil
    shutil.rmtree(bundle_dir)
    shutil.rmtree(export_dir)
    print("test_release_gate_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
