#!/usr/bin/env python3
import csv
import os
import sys


VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3

REQUIRED_COLUMNS = {
    "kernel",
    "backend",
    "samples",
    "avg_ms",
    "p95_ms",
    "mpix_per_s",
}


def error(trace_id, error_code, message, field=""):
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


def main(argv):
    if len(argv) != 2 or argv[1] in ("-h", "--help"):
        print("usage: kernel_bench_summary.py <kernel_bench.csv>", file=sys.stderr)
        return 2

    path = argv[1]
    if not os.path.exists(path):
        return error("tool.probe.kernel_bench.io", VN_E_IO, "kernel bench not found", "path")

    try:
        with open(path, "r", encoding="utf-8", newline="") as fp:
            reader = csv.DictReader(fp)
            if reader.fieldnames is None:
                return error("tool.probe.kernel_bench.format", VN_E_FORMAT, "missing csv header", "header")
            for column in REQUIRED_COLUMNS:
                if column not in reader.fieldnames:
                    return error("tool.probe.kernel_bench.format", VN_E_FORMAT, "missing required column", column)
            rows = list(reader)
    except OSError:
        return error("tool.probe.kernel_bench.io", VN_E_IO, "failed to open kernel bench", "path")
    except csv.Error:
        return error("tool.probe.kernel_bench.format", VN_E_FORMAT, "invalid csv", "csv")

    if len(rows) == 0:
        return error("tool.probe.kernel_bench.format", VN_E_FORMAT, "missing kernel rows", "rows")

    kernels = []
    total_samples = 0
    mean_avg_sum = 0.0
    p95_max = 0.0
    max_mpix = 0.0
    first = rows[0]
    for row in rows:
        try:
            samples = int(row["samples"])
            avg_ms = float(row["avg_ms"])
            p95_ms = float(row["p95_ms"])
            mpix = float(row["mpix_per_s"])
        except ValueError:
            return error("tool.probe.kernel_bench.format", VN_E_FORMAT, "invalid numeric kernel field", "samples/avg_ms/p95_ms/mpix_per_s")
        kernels.append(row["kernel"])
        total_samples += samples
        mean_avg_sum += avg_ms
        if p95_ms > p95_max:
            p95_max = p95_ms
        if mpix > max_mpix:
            max_mpix = mpix

    print(
        " ".join(
            [
                "trace_id=tool.probe.kernel_bench.ok",
                f"kernel_count={len(rows)}",
                f"kernels={','.join(kernels)}",
                f"backend={first.get('backend', 'unknown')}",
                f"total_samples={total_samples}",
                f"mean_avg_ms={mean_avg_sum / len(rows):.3f}",
                f"max_p95_ms={p95_max:.3f}",
                f"max_mpix_per_s={max_mpix:.3f}",
                f"host_cpu={first.get('host_cpu', 'unknown')}",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
