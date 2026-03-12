#!/usr/bin/env python3
import subprocess
import sys


def run_case(path):
    proc = subprocess.run(
        ["python3", "tools/probe/kernel_compare_summary.py", path],
        cwd=".",
        capture_output=True,
        text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def main():
    rc, out, err = run_case("tests/fixtures/kernel_compare/sample_kernel_compare.csv")
    if rc != 0:
        print(f"kernel_compare_summary ok case failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.probe.kernel_compare.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1
    if "baseline=scalar" not in out or "candidate=avx2" not in out:
        print("missing baseline/candidate summary", file=sys.stderr)
        return 1

    rc, out, err = run_case("tests/fixtures/runtime_trace/sample_trace.log")
    if rc == 0:
        print("kernel_compare_summary should fail on non-csv file", file=sys.stderr)
        return 1
    if "trace_id=tool.probe.kernel_compare.format" not in err:
        print("missing format failure trace_id", file=sys.stderr)
        return 1

    print("test_kernel_compare_summary ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
