#!/usr/bin/env python3
import subprocess
import sys


def run_case(path):
    proc = subprocess.run(
        ["python3", "tools/probe/perf_summary.py", path],
        cwd=".",
        capture_output=True,
        text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def main():
    rc, out, err = run_case("tests/fixtures/perf_summary/sample_perf_summary.csv")
    if rc != 0:
        print(f"perf_summary ok case failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.probe.perf_summary.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1
    if "scenes=S1,S3,S10" not in out or "actual_backend=avx2" not in out:
        print("missing scene/backend summary", file=sys.stderr)
        return 1

    rc, out, err = run_case("tests/fixtures/runtime_trace/sample_trace.log")
    if rc == 0:
        print("perf_summary should fail on non-csv file", file=sys.stderr)
        return 1
    if "trace_id=tool.probe.perf_summary.format" not in err:
        print("missing format failure trace_id", file=sys.stderr)
        return 1

    print("test_perf_summary ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
