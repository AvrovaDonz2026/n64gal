#!/usr/bin/env python3
import math
import os
import sys


VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3


def parse_pairs(line):
    result = {}
    for field in line.strip().split():
        if "=" not in field:
            continue
        key, value = field.split("=", 1)
        result[key] = value
    return result


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


def percentile95(values):
    ordered = sorted(values)
    index = int(math.ceil((95.0 * len(ordered)) / 100.0)) - 1
    if index < 0:
        index = 0
    if index >= len(ordered):
        index = len(ordered) - 1
    return ordered[index]


def main(argv):
    if len(argv) != 2 or argv[1] in ("-h", "--help"):
        print("usage: trace_summary.py <runtime_trace.log>", file=sys.stderr)
        return 2

    path = argv[1]
    if not os.path.exists(path):
        return error("tool.probe.trace_summary.io", VN_E_IO, "trace log not found", "path")

    frames = []
    summary = None
    try:
        with open(path, "r", encoding="utf-8") as fp:
            for raw_line in fp:
                line = raw_line.strip()
                if line.startswith("frame="):
                    frames.append(parse_pairs(line))
                elif line.startswith("vn_runtime ok "):
                    summary = parse_pairs(line)
    except UnicodeDecodeError:
        return error("tool.probe.trace_summary.format", VN_E_FORMAT, "trace log is not valid text", "encoding")
    except OSError:
        return error("tool.probe.trace_summary.io", VN_E_IO, "failed to open trace log", "path")

    if summary is None:
        return error("tool.probe.trace_summary.format", VN_E_FORMAT, "missing runtime summary line", "summary")
    if len(frames) == 0:
        return error("tool.probe.trace_summary.format", VN_E_FORMAT, "missing frame samples", "frames")

    try:
        frame_ms_values = [float(item["frame_ms"]) for item in frames]
        vm_ms_values = [float(item.get("vm_ms", "0")) for item in frames]
        build_ms_values = [float(item.get("build_ms", "0")) for item in frames]
        raster_ms_values = [float(item.get("raster_ms", "0")) for item in frames]
    except KeyError as exc:
        return error("tool.probe.trace_summary.format", VN_E_FORMAT, "missing frame field", str(exc))
    except ValueError:
        return error("tool.probe.trace_summary.format", VN_E_FORMAT, "invalid numeric frame field", "frame_ms")

    avg_frame_ms = sum(frame_ms_values) / len(frame_ms_values)
    avg_vm_ms = sum(vm_ms_values) / len(vm_ms_values)
    avg_build_ms = sum(build_ms_values) / len(build_ms_values)
    avg_raster_ms = sum(raster_ms_values) / len(raster_ms_values)
    p95_frame_ms = percentile95(frame_ms_values)
    max_frame_ms = max(frame_ms_values)

    print(
        " ".join(
            [
                "trace_id=tool.probe.trace_summary.ok",
                f"samples={len(frame_ms_values)}",
                f"avg_frame_ms={avg_frame_ms:.3f}",
                f"p95_frame_ms={p95_frame_ms:.3f}",
                f"max_frame_ms={max_frame_ms:.3f}",
                f"avg_vm_ms={avg_vm_ms:.3f}",
                f"avg_build_ms={avg_build_ms:.3f}",
                f"avg_raster_ms={avg_raster_ms:.3f}",
                f"backend={summary.get('backend', 'unknown')}",
                f"scene={summary.get('scene', 'unknown')}",
                f"frames={summary.get('frames', '0')}",
                f"dynres_tier={summary.get('dynres_tier', 'unknown')}",
                f"dynres_switches={summary.get('dynres_switches', '0')}",
                f"frame_reuse_hits={summary.get('frame_reuse_hits', '0')}",
                f"op_cache_hits={summary.get('op_cache_hits', '0')}",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
