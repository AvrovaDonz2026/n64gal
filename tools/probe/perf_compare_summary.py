#!/usr/bin/env python3
import csv
import os
import sys


VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3

REQUIRED_COLUMNS = {
    "scene",
    "baseline_label",
    "candidate_label",
    "baseline_p95_ms",
    "candidate_p95_ms",
    "p95_speedup",
    "p95_gain_pct",
    "baseline_avg_ms",
    "candidate_avg_ms",
    "avg_speedup",
    "avg_gain_pct",
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
        print("usage: perf_compare_summary.py <perf_compare.csv>", file=sys.stderr)
        return 2

    path = argv[1]
    if not os.path.exists(path):
        return error("tool.probe.perf_compare.io", VN_E_IO, "perf compare not found", "path")

    try:
        with open(path, "r", encoding="utf-8", newline="") as fp:
            reader = csv.DictReader(fp)
            if reader.fieldnames is None:
                return error("tool.probe.perf_compare.format", VN_E_FORMAT, "missing csv header", "header")
            for column in REQUIRED_COLUMNS:
                if column not in reader.fieldnames:
                    return error("tool.probe.perf_compare.format", VN_E_FORMAT, "missing required column", column)
            rows = list(reader)
    except OSError:
        return error("tool.probe.perf_compare.io", VN_E_IO, "failed to open perf compare", "path")
    except csv.Error:
        return error("tool.probe.perf_compare.format", VN_E_FORMAT, "invalid csv", "csv")

    if len(rows) == 0:
        return error("tool.probe.perf_compare.format", VN_E_FORMAT, "missing compare rows", "rows")

    scenes = []
    p95_gain_sum = 0.0
    avg_gain_sum = 0.0
    p95_speedup_max = 0.0
    first = rows[0]
    for row in rows:
        try:
            p95_speedup = float(row["p95_speedup"])
            p95_gain = float(row["p95_gain_pct"])
            avg_gain = float(row["avg_gain_pct"])
        except ValueError:
            return error("tool.probe.perf_compare.format", VN_E_FORMAT, "invalid numeric compare field", "p95/avg gain")
        scenes.append(row["scene"])
        p95_gain_sum += p95_gain
        avg_gain_sum += avg_gain
        if p95_speedup > p95_speedup_max:
            p95_speedup_max = p95_speedup

    mean_p95_gain = p95_gain_sum / len(rows)
    mean_avg_gain = avg_gain_sum / len(rows)

    print(
        " ".join(
            [
                "trace_id=tool.probe.perf_compare.ok",
                f"scene_count={len(rows)}",
                f"scenes={','.join(scenes)}",
                f"baseline={first.get('baseline_label', 'unknown')}",
                f"candidate={first.get('candidate_label', 'unknown')}",
                f"mean_p95_gain_pct={mean_p95_gain:.2f}",
                f"mean_avg_gain_pct={mean_avg_gain:.2f}",
                f"max_p95_speedup={p95_speedup_max:.3f}",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
