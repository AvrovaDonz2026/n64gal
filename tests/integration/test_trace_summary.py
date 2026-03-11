#!/usr/bin/env python3
import subprocess
import sys


def run_case(path):
    proc = subprocess.run(
        ["python3", "tools/probe/trace_summary.py", path],
        cwd=".",
        capture_output=True,
        text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def main():
    rc, out, err = run_case("tests/fixtures/runtime_trace/sample_trace.log")
    if rc != 0:
        print(f"trace_summary failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.probe.trace_summary.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1
    if "backend=avx2" not in out or "scene=S2" not in out:
        print("missing backend/scene in summary", file=sys.stderr)
        return 1
    if "samples=3" not in out or "p95_frame_ms=3.000" not in out:
        print("missing samples/p95 in summary", file=sys.stderr)
        return 1

    rc, out, err = run_case("tests/fixtures/vnsave/v1/sample.vnsave")
    if rc == 0:
        print("trace_summary should fail on non-trace file", file=sys.stderr)
        return 1
    if "trace_id=tool.probe.trace_summary.format" not in err:
        print("missing format failure trace_id", file=sys.stderr)
        return 1

    print("test_trace_summary ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
