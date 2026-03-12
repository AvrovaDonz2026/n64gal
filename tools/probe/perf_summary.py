#!/usr/bin/env python3
import csv
import os
import sys


VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3

REQUIRED_COLUMNS = {
    "scene",
    "samples",
    "p95_frame_ms",
    "avg_frame_ms",
    "max_rss_mb",
    "backend",
    "dt_ms",
    "resolution",
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
        print("usage: perf_summary.py <perf_summary.csv>", file=sys.stderr)
        return 2

    path = argv[1]
    if not os.path.exists(path):
        return error("tool.probe.perf_summary.io", VN_E_IO, "perf summary not found", "path")

    try:
        with open(path, "r", encoding="utf-8", newline="") as fp:
            reader = csv.DictReader(fp)
            if reader.fieldnames is None:
                return error("tool.probe.perf_summary.format", VN_E_FORMAT, "missing csv header", "header")
            for column in REQUIRED_COLUMNS:
                if column not in reader.fieldnames:
                    return error("tool.probe.perf_summary.format", VN_E_FORMAT, "missing required column", column)
            rows = list(reader)
    except OSError:
        return error("tool.probe.perf_summary.io", VN_E_IO, "failed to open perf summary", "path")
    except csv.Error:
        return error("tool.probe.perf_summary.format", VN_E_FORMAT, "invalid csv", "csv")

    if len(rows) == 0:
        return error("tool.probe.perf_summary.format", VN_E_FORMAT, "missing summary rows", "rows")

    scenes = []
    total_samples = 0
    p95_max = 0.0
    avg_sum = 0.0
    max_rss = 0.0
    first = rows[0]
    for row in rows:
        try:
            samples = int(row["samples"])
            p95 = float(row["p95_frame_ms"])
            avg = float(row["avg_frame_ms"])
            rss = float(row["max_rss_mb"])
        except ValueError:
            return error("tool.probe.perf_summary.format", VN_E_FORMAT, "invalid numeric field", "samples/p95_frame_ms/avg_frame_ms/max_rss_mb")
        scenes.append(row["scene"])
        total_samples += samples
        avg_sum += avg
        if p95 > p95_max:
            p95_max = p95
        if rss > max_rss:
            max_rss = rss

    avg_frame_ms = avg_sum / len(rows)
    print(
        " ".join(
            [
                "trace_id=tool.probe.perf_summary.ok",
                f"scene_count={len(rows)}",
                f"scenes={','.join(scenes)}",
                f"total_samples={total_samples}",
                f"p95_frame_ms_max={p95_max:.3f}",
                f"avg_frame_ms_mean={avg_frame_ms:.3f}",
                f"max_rss_mb={max_rss:.3f}",
                f"backend={first.get('backend', 'unknown')}",
                f"requested_backend={first.get('requested_backend', first.get('backend', 'unknown'))}",
                f"actual_backend={first.get('actual_backend', 'unknown')}",
                f"host_cpu={first.get('host_cpu', 'unknown')}",
                f"resolution={first.get('resolution', 'unknown')}",
                f"dt_ms={first.get('dt_ms', '0')}",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
