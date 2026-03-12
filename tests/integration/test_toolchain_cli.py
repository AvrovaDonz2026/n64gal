#!/usr/bin/env python3
import os
import subprocess
import sys


ROOT = "."
TOOL = ["python3", "tools/toolchain.py"]


def run_case(args):
    proc = subprocess.run(TOOL + args, cwd=ROOT, capture_output=True, text=True)
    return proc.returncode, proc.stdout, proc.stderr


def main():
    out_path = "tests/integration/toolchain_tmp.vnsave"
    if os.path.exists(out_path):
        os.remove(out_path)

    rc, out, err = run_case(["--help"])
    if rc != 2 or "validate-manifest" not in err or "probe-vnsave" not in err:
        print("toolchain help output mismatch", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-manifest", "tests/fixtures/tool_manifest/valid/vnsave_migrate.json"])
    if rc != 0 or "trace_id=tool.validate.manifest.ok" not in out:
        print(f"validate-manifest failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-release-contracts"])
    if rc != 0 or "trace_id=tool.validate.release_contracts.ok" not in out:
        print(f"validate-release-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-toolchain-contracts"])
    if rc != 0 or "trace_id=tool.validate.toolchain_contracts.ok" not in out:
        print(f"validate-toolchain-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-host-sdk-contracts"])
    if rc != 0 or "trace_id=tool.validate.host_sdk_contracts.ok" not in out:
        print(f"validate-host-sdk-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-preview-contracts"])
    if rc != 0 or "trace_id=tool.validate.preview_contracts.ok" not in out:
        print(f"validate-preview-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["probe-vnsave", "--in", "tests/fixtures/vnsave/v1/sample.vnsave"])
    if rc != 0 or "trace_id=tool.probe.vnsave.ok" not in out:
        print(f"probe-vnsave failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["probe-trace-summary", "tests/fixtures/runtime_trace/sample_trace.log"])
    if rc != 0 or "trace_id=tool.probe.trace_summary.ok" not in out:
        print(f"probe-trace-summary failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["probe-preview", "--scene=S2", "--frames=2", "--command=step_frame:2"])
    if rc != 0 or "trace_id=tool.probe.preview.ok" not in out:
        print(f"probe-preview failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["probe-perf-summary", "tests/fixtures/perf_summary/sample_perf_summary.csv"])
    if rc != 0 or "trace_id=tool.probe.perf_summary.ok" not in out:
        print(f"probe-perf-summary failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["probe-perf-compare", "tests/fixtures/perf_compare/sample_perf_compare.csv"])
    if rc != 0 or "trace_id=tool.probe.perf_compare.ok" not in out:
        print(f"probe-perf-compare failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["probe-kernel-bench", "tests/fixtures/kernel_bench/sample_kernel_bench.csv"])
    if rc != 0 or "trace_id=tool.probe.kernel_bench.ok" not in out:
        print(f"probe-kernel-bench failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["probe-kernel-compare", "tests/fixtures/kernel_compare/sample_kernel_compare.csv"])
    if rc != 0 or "trace_id=tool.probe.kernel_compare.ok" not in out:
        print(f"probe-kernel-compare failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["migrate-vnsave", "--in", "tests/fixtures/vnsave/v0/sample.vnsave", "--out", out_path])
    if rc != 0 or "trace_id=tool.vnsave_migrate.ok" not in out:
        print(f"migrate-vnsave failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1
    if not os.path.exists(out_path):
        print("migrate-vnsave did not write output", file=sys.stderr)
        return 1

    rc, out, err = run_case(["unknown"])
    if rc != 2 or "trace_id=tool.toolchain.invalid" not in err:
        print("unknown command handling mismatch", file=sys.stderr)
        return 1

    if os.path.exists(out_path):
        os.remove(out_path)
    print("test_toolchain_cli ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
