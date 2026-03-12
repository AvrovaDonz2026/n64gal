#!/usr/bin/env python3
import csv
import os
import sys


VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3

REQUIRED_COLUMNS = {
    "kernel",
    "baseline_label",
    "candidate_label",
    "avg_speedup",
    "avg_gain_pct",
    "p95_speedup",
    "p95_gain_pct",
    "throughput_gain_pct",
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
        print("usage: kernel_compare_summary.py <kernel_compare.csv>", file=sys.stderr)
        return 2

    path = argv[1]
    if not os.path.exists(path):
        return error("tool.probe.kernel_compare.io", VN_E_IO, "kernel compare not found", "path")

    try:
        with open(path, "r", encoding="utf-8", newline="") as fp:
            reader = csv.DictReader(fp)
            if reader.fieldnames is None:
                return error("tool.probe.kernel_compare.format", VN_E_FORMAT, "missing csv header", "header")
            for column in REQUIRED_COLUMNS:
                if column not in reader.fieldnames:
                    return error("tool.probe.kernel_compare.format", VN_E_FORMAT, "missing required column", column)
            rows = list(reader)
    except OSError:
        return error("tool.probe.kernel_compare.io", VN_E_IO, "failed to open kernel compare", "path")
    except csv.Error:
        return error("tool.probe.kernel_compare.format", VN_E_FORMAT, "invalid csv", "csv")

    if len(rows) == 0:
        return error("tool.probe.kernel_compare.format", VN_E_FORMAT, "missing compare rows", "rows")

    kernels = []
    avg_gain_sum = 0.0
    p95_gain_sum = 0.0
    throughput_gain_sum = 0.0
    max_p95_speedup = 0.0
    first = rows[0]
    for row in rows:
        try:
            avg_gain = float(row["avg_gain_pct"])
            p95_gain = float(row["p95_gain_pct"])
            throughput_gain = float(row["throughput_gain_pct"])
            p95_speedup = float(row["p95_speedup"])
        except ValueError:
            return error("tool.probe.kernel_compare.format", VN_E_FORMAT, "invalid numeric compare field", "avg/p95/throughput gain")
        kernels.append(row["kernel"])
        avg_gain_sum += avg_gain
        p95_gain_sum += p95_gain
        throughput_gain_sum += throughput_gain
        if p95_speedup > max_p95_speedup:
            max_p95_speedup = p95_speedup

    print(
        " ".join(
            [
                "trace_id=tool.probe.kernel_compare.ok",
                f"kernel_count={len(rows)}",
                f"kernels={','.join(kernels)}",
                f"baseline={first.get('baseline_label', 'unknown')}",
                f"candidate={first.get('candidate_label', 'unknown')}",
                f"mean_avg_gain_pct={avg_gain_sum / len(rows):.2f}",
                f"mean_p95_gain_pct={p95_gain_sum / len(rows):.2f}",
                f"mean_throughput_gain_pct={throughput_gain_sum / len(rows):.2f}",
                f"max_p95_speedup={max_p95_speedup:.3f}",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
