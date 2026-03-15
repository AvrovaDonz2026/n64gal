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
    summary_path = "tests/integration/toolchain_release_gate_tmp.md"
    try:
        if os.path.exists(out_path):
            os.remove(out_path)
    except FileNotFoundError:
        pass
    try:
        if os.path.exists(summary_path):
            os.remove(summary_path)
    except FileNotFoundError:
        pass

    rc, out, err = run_case(["--help"])
    if rc != 2 or "validate-all" not in err or "validate-manifest" not in err or "probe-vnsave" not in err:
        print("toolchain help output mismatch", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-all"])
    if rc != 0 or "trace_id=tool.toolchain.validate_all.ok" not in out:
        print(f"validate-all failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-release-audit", "--allow-dirty"])
    if rc != 0 or "trace_id=tool.validate.release_audit.ok" not in out:
        print(f"validate-release-audit failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-release-docs"])
    if rc != 0 or "trace_id=tool.validate.release_docs.ok" not in out:
        print(f"validate-release-docs failed rc={rc} out={out} err={err}", file=sys.stderr)
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

    rc, out, err = run_case(["validate-backend-contracts"])
    if rc != 0 or "trace_id=tool.validate.backend_contracts.ok" not in out:
        print(f"validate-backend-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-api-index-contracts"])
    if rc != 0 or "trace_id=tool.validate.api_index_contracts.ok" not in out:
        print(f"validate-api-index-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-compat-matrix"])
    if rc != 0 or "trace_id=tool.validate.compat_matrix.ok" not in out:
        print(f"validate-compat-matrix failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-ecosystem-contracts"])
    if rc != 0 or "trace_id=tool.validate.ecosystem_contracts.ok" not in out:
        print(f"validate-ecosystem-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-error-contracts"])
    if rc != 0 or "trace_id=tool.validate.error_contracts.ok" not in out:
        print(f"validate-error-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-host-sdk-contracts"])
    if rc != 0 or "trace_id=tool.validate.host_sdk_contracts.ok" not in out:
        print(f"validate-host-sdk-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-migration-contracts"])
    if rc != 0 or "trace_id=tool.validate.migration_contracts.ok" not in out:
        print(f"validate-migration-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-pack-contracts"])
    if rc != 0 or "trace_id=tool.validate.pack_contracts.ok" not in out:
        print(f"validate-pack-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-platform-contracts"])
    if rc != 0 or "trace_id=tool.validate.platform_contracts.ok" not in out:
        print(f"validate-platform-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-preview-contracts"])
    if rc != 0 or "trace_id=tool.validate.preview_contracts.ok" not in out:
        print(f"validate-preview-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-perf-contracts"])
    if rc != 0 or "trace_id=tool.validate.perf_contracts.ok" not in out:
        print(f"validate-perf-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-porting-contracts"])
    if rc != 0 or "trace_id=tool.validate.porting_contracts.ok" not in out:
        print(f"validate-porting-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-runtime-contracts"])
    if rc != 0 or "trace_id=tool.validate.runtime_contracts.ok" not in out:
        print(f"validate-runtime-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-save-contracts"])
    if rc != 0 or "trace_id=tool.validate.save_contracts.ok" not in out:
        print(f"validate-save-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["validate-template-contracts"])
    if rc != 0 or "trace_id=tool.validate.template_contracts.ok" not in out:
        print(f"validate-template-contracts failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["release-gate", "--allow-dirty", "--skip-cc-suite", "--summary-out", summary_path])
    if rc != 0 or "trace_id=release.gate.ok" not in out:
        print(f"release-gate failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1
    if not os.path.exists(summary_path):
        print("release-gate did not write summary", file=sys.stderr)
        return 1

    rc, out, err = run_case(["release-host-sdk-smoke", "--summary-out", summary_path])
    if rc != 0 or "trace_id=release.host_sdk.ok" not in out:
        print(f"release-host-sdk-smoke failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["release-preview-evidence", "--summary-out", summary_path])
    if rc != 0 or "trace_id=release.preview.ok" not in out:
        print(f"release-preview-evidence failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case([
        "release-gate",
        "--allow-dirty",
        "--skip-cc-suite",
        "--with-soak",
        "--soak-skip-build",
        "--soak-skip-pack",
        "--soak-runner-bin", "build_release_soak/vn_player",
        "--soak-frames-per-scene", "2",
        "--soak-scenes", "S0",
        "--summary-out", summary_path,
    ])
    if rc != 0 or "trace_id=release.gate.ok" not in out:
        print(f"release-gate runner-bin failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["release-soak", "--skip-pack", "--frames-per-scene", "2", "--scenes", "S0", "--summary-out", summary_path])
    if rc != 0 or "trace_id=release.soak.ok" not in out:
        print(f"release-soak failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["release-soak", "--skip-pack", "--skip-build", "--runner-bin", "build_release_soak/vn_player", "--frames-per-scene", "2", "--scenes", "S0", "--summary-out", summary_path])
    if rc != 0 or "trace_id=release.soak.ok" not in out:
        print(f"release-soak runner-bin failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["release-bundle", "--out-dir", "tests/integration/toolchain_release_bundle_tmp", "--gate-summary", summary_path, "--soak-summary", summary_path, "--ci-summary", summary_path])
    if rc != 0 or "trace_id=release.bundle.ok" not in out:
        print(f"release-bundle failed rc={rc} out={out} err={err}", file=sys.stderr)
        return 1

    rc, out, err = run_case(["release-report", "--out-dir", "tests/integration/toolchain_release_report_tmp", "--bundle-index", "tests/integration/toolchain_release_bundle_tmp/release_bundle_index.md", "--gate-summary", summary_path, "--soak-summary", summary_path, "--ci-suite-summary", summary_path])
    if rc != 0 or "trace_id=release.report.ok" not in out:
        print(f"release-report failed rc={rc} out={out} err={err}", file=sys.stderr)
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

    try:
        if os.path.exists(out_path):
            os.remove(out_path)
    except FileNotFoundError:
        pass
    try:
        if os.path.exists(summary_path):
            os.remove(summary_path)
    except FileNotFoundError:
        pass
    if os.path.isdir("tests/integration/toolchain_release_bundle_tmp"):
        import shutil
        shutil.rmtree("tests/integration/toolchain_release_bundle_tmp")
    if os.path.isdir("tests/integration/toolchain_release_report_tmp"):
        import shutil
        shutil.rmtree("tests/integration/toolchain_release_report_tmp")
    print("test_toolchain_cli ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
