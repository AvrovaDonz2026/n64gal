#!/usr/bin/env python3
import subprocess
import sys


def run_case(path):
    proc = subprocess.run(
        ["python3", "tools/probe/preview_summary.py", path],
        cwd=".",
        capture_output=True,
        text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def main():
    rc, out, err = run_case("tests/fixtures/preview_response/sample_ok.json")
    if rc != 0:
        print(f"preview_summary ok case failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.probe.preview.ok" not in out or "actual_backend=avx2" not in out:
        print("preview_summary missing success fields", file=sys.stderr)
        return 1

    rc, out, err = run_case("tests/fixtures/preview_response/sample_error.json")
    if rc == 0:
        print("preview_summary error case should fail", file=sys.stderr)
        return 1
    if "trace_id=tool.probe.preview.failed" not in err:
        print("preview_summary missing failure trace_id", file=sys.stderr)
        return 1

    print("test_preview_summary ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
