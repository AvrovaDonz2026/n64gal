#!/usr/bin/env python3
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/ci/run_linux_x64_sanitizers.sh"]
EXPECTED_TESTS = {
    "test_vm",
    "test_scene_catalog",
    "test_resource_texture_backend",
    "test_runtime_session",
    "test_runtime_cli_errors",
}


def run_script(build_dir, summary_md, summary_json, extra_args):
    env = os.environ.copy()
    env["BUILD_DIR"] = str(build_dir)
    return subprocess.run(
        SCRIPT
        + [
            "--frames-per-scene",
            "2",
            "--resolution",
            "64x48",
            "--summary-out",
            str(summary_md),
            "--summary-json-out",
            str(summary_json),
        ]
        + extra_args,
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
    )


def copy_prebuilt_binaries(source_dir, build_dir):
    for name in sorted(EXPECTED_TESTS | {"vn_player"}):
        source = source_dir / name
        if not source.is_file():
            raise RuntimeError(f"missing prebuilt sanitizer binary: {source}")
        target = build_dir / name
        shutil.copy2(source, target)
        target.chmod(target.stat().st_mode | 0o111)


def main():
    prebuilt_text = os.environ.get("VN_SANITIZER_PREBUILT_DIR", "")
    with tempfile.TemporaryDirectory(prefix="n64gal_sanitizer_test_") as temp_text:
        temp = Path(temp_text)
        build_dir = temp / "build"
        build_dir.mkdir()
        base_args = ["--skip-pack"]
        if prebuilt_text:
            copy_prebuilt_binaries(Path(prebuilt_text), build_dir)
            base_args.append("--skip-build")

        summary_md = temp / "summary.md"
        summary_json = temp / "summary.json"
        proc = run_script(build_dir, summary_md, summary_json, base_args)
        if proc.returncode != 0:
            print(
                f"short sanitizer run failed rc={proc.returncode}\n"
                f"stdout={proc.stdout}\nstderr={proc.stderr}",
                file=sys.stderr,
            )
            return 1
        if "trace_id=ci.sanitizers.ok" not in proc.stdout:
            print("missing sanitizer success trace", file=sys.stderr)
            return 1
        if not summary_md.is_file() or not summary_json.is_file():
            print("sanitizer summaries are missing", file=sys.stderr)
            return 1

        summary_text = summary_md.read_text(encoding="utf-8")
        payload = json.loads(summary_json.read_text(encoding="utf-8"))
        if "# Linux x64 Sanitizer Summary" not in summary_text:
            print("sanitizer markdown header missing", file=sys.stderr)
            return 1
        if payload.get("status") != "success" or payload.get("frames_per_scene") != 2:
            print("sanitizer JSON status or shortened frame count mismatch", file=sys.stderr)
            return 1
        if payload.get("sanitizers") != ["address", "undefined"] or not payload.get("strict_c89"):
            print("sanitizer JSON compiler contract mismatch", file=sys.stderr)
            return 1
        if "quarantine_size_mb=8" not in payload.get("asan_options", ""):
            print("sanitizer JSON ASan allocator policy missing", file=sys.stderr)
            return 1
        if (
            payload.get("requested_scene_duration_sec") != 450
            or payload.get("frames_override") is not True
            or payload.get("simulated_ms_per_scene") != 32
            or payload.get("total_simulated_ms") != 64
        ):
            print("sanitizer JSON simulated duration mismatch", file=sys.stderr)
            return 1
        if payload.get("scenes") != ["Opening", "Gallery"]:
            print("sanitizer JSON scene contract mismatch", file=sys.stderr)
            return 1
        records = payload.get("records", [])
        test_names = {record.get("name") for record in records if record.get("kind") == "unit"}
        soak_names = {record.get("name") for record in records if record.get("kind") == "soak"}
        if test_names != EXPECTED_TESTS or soak_names != {"Opening", "Gallery"}:
            print("sanitizer measured-process coverage mismatch", file=sys.stderr)
            return 1
        if any(record.get("status") != "success" for record in records):
            print("sanitizer success summary contains a failed record", file=sys.stderr)
            return 1
        if any(record.get("peak_rss_kib", 0) > 64 * 1024 for record in records):
            print("sanitizer success summary exceeds RSS limit", file=sys.stderr)
            return 1

        low_md = temp / "low_limit.md"
        low_json = temp / "low_limit.json"
        low_proc = run_script(
            build_dir,
            low_md,
            low_json,
            ["--skip-build", "--skip-pack", "--max-rss-mib", "1"],
        )
        if low_proc.returncode == 0:
            print("one MiB RSS limit should fail", file=sys.stderr)
            return 1
        low_payload = json.loads(low_json.read_text(encoding="utf-8"))
        if low_payload.get("status") != "failed":
            print("RSS gate failure summary did not report failed status", file=sys.stderr)
            return 1
        if not any(record.get("status") == "rss-limit" for record in low_payload.get("records", [])):
            print("RSS gate failure summary lacks rss-limit record", file=sys.stderr)
            return 1

    print("test_linux_x64_sanitizer_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
