#!/usr/bin/env python3
from pathlib import Path
import sys


VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3


def error(trace_id, error_code, field, message):
    error_name = {
        VN_E_INVALID_ARG: "VN_E_INVALID_ARG",
        VN_E_IO: "VN_E_IO",
        VN_E_FORMAT: "VN_E_FORMAT",
    }.get(error_code, "VN_E_UNKNOWN")
    parts = [f"trace_id={trace_id}", f"error_code={error_code}", f"error_name={error_name}"]
    if field:
        parts.append(f"field={field}")
    parts.append(f"message={message}")
    print(" ".join(parts), file=sys.stderr)
    return 1


def read_text(root: Path, rel: str):
    path = root / rel
    if not path.exists():
        raise FileNotFoundError(rel)
    return path.read_text(encoding="utf-8")


def require_contains(text: str, needle: str, field: str):
    if needle not in text:
        raise ValueError(field)


def main(argv):
    root = Path(".")
    if len(argv) > 2:
        return error("tool.validate.perf_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_perf_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.perf_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        readme = read_text(root, "README.md")
        perf_doc = read_text(root, "docs/perf-report.md")
        toolchain = read_text(root, "docs/toolchain.md")
        run_perf = read_text(root, "tests/perf/run_perf.sh")
        compare_perf = read_text(root, "tests/perf/compare_perf.sh")
        compare_kernel = read_text(root, "tests/perf/compare_kernel_bench.sh")
    except FileNotFoundError as exc:
        return error("tool.validate.perf_contracts.io", VN_E_IO, str(exc), "required perf contract file missing")
    except OSError:
        return error("tool.validate.perf_contracts.io", VN_E_IO, "root", "failed reading perf contract files")

    try:
        require_contains(perf_doc, "`perf_summary.csv`", "perf_doc.summary_csv")
        require_contains(perf_doc, "`compare/perf_compare.csv`", "perf_doc.compare_csv")
        require_contains(perf_doc, "`kernel_compare.csv`", "perf_doc.kernel_compare_csv")
        require_contains(perf_doc, "`host_cpu`", "perf_doc.host_cpu")
        require_contains(perf_doc, "`requested_backend`", "perf_doc.requested_backend")
        require_contains(perf_doc, "`actual_backend`", "perf_doc.actual_backend")
        require_contains(perf_doc, "check_perf_thresholds.sh", "perf_doc.check_perf_thresholds")
        require_contains(perf_doc, "check_kernel_thresholds.sh", "perf_doc.check_kernel_thresholds")

        require_contains(run_perf, "requested_backend,actual_backend,host_cpu", "run_perf.summary_header")
        require_contains(run_perf, "perf_host_cpu.txt", "run_perf.host_cpu_txt")

        require_contains(compare_perf, "COMPARE_CSV=\"$OUT_DIR/perf_compare.csv\"", "compare_perf.compare_csv")
        require_contains(compare_kernel, "COMPARE_CSV=\"$OUT_DIR/kernel_compare.csv\"", "compare_kernel.compare_csv")

        require_contains(toolchain, "probe-perf-summary", "toolchain.probe_perf_summary")
        require_contains(toolchain, "probe-perf-compare", "toolchain.probe_perf_compare")
        require_contains(toolchain, "probe-kernel-bench", "toolchain.probe_kernel_bench")
        require_contains(toolchain, "probe-kernel-compare", "toolchain.probe_kernel_compare")

        require_contains(readme, "probe-perf-summary", "readme.probe_perf_summary")
        require_contains(readme, "probe-perf-compare", "readme.probe_perf_compare")
        require_contains(readme, "probe-kernel-bench", "readme.probe_kernel_bench")
        require_contains(readme, "probe-kernel-compare", "readme.probe_kernel_compare")
    except ValueError as exc:
        return error("tool.validate.perf_contracts.format", VN_E_FORMAT, str(exc), "perf contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.perf_contracts.ok",
                f"root={root}",
                "perf_summary=required",
                "perf_compare=required",
                "kernel_compare=required",
                "host_cpu=required",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
