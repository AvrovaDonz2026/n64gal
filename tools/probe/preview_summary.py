#!/usr/bin/env python3
import json
import os
import sys


VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3


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
        print("usage: preview_summary.py <preview_response.json>", file=sys.stderr)
        return 2

    path = argv[1]
    if not os.path.exists(path):
        return error("tool.probe.preview.io", VN_E_IO, "preview response not found", "path")

    try:
        with open(path, "r", encoding="utf-8") as fp:
            data = json.load(fp)
    except OSError:
        return error("tool.probe.preview.io", VN_E_IO, "failed to open preview response", "path")
    except json.JSONDecodeError:
        return error("tool.probe.preview.format", VN_E_FORMAT, "invalid preview json", "json")

    if not isinstance(data, dict):
        return error("tool.probe.preview.format", VN_E_FORMAT, "preview response root must be object", "root")

    status = data.get("status", "error")
    error_code = data.get("error_code", 0)
    error_name = data.get("error_name", "VN_E_UNKNOWN")
    preview_trace_id = data.get("trace_id", "preview.unknown")
    request = data.get("request", {}) or {}
    summary = data.get("summary", {}) or {}
    perf_summary = data.get("perf_summary", {}) or {}
    final_state = data.get("final_state", {}) or {}

    if status != "ok":
        print(
            " ".join(
                [
                    "trace_id=tool.probe.preview.failed",
                    f"error_code={error_code}",
                    f"error_name={error_name}",
                    f"preview_trace_id={preview_trace_id}",
                    f"scene={request.get('scene_name', 'unknown')}",
                    f"backend={request.get('backend_name', 'unknown')}",
                    "message=preview response indicates failure",
                ]
            ),
            file=sys.stderr,
        )
        return 1

    print(
        " ".join(
            [
                "trace_id=tool.probe.preview.ok",
                f"preview_trace_id={preview_trace_id}",
                f"scene={request.get('scene_name', 'unknown')}",
                f"requested_backend={request.get('backend_name', 'unknown')}",
                f"actual_backend={final_state.get('backend_name', 'unknown')}",
                f"width={request.get('width', 0)}",
                f"height={request.get('height', 0)}",
                f"frame_samples={summary.get('frame_samples', 0)}",
                f"reload_count={summary.get('reload_count', 0)}",
                f"session_done={summary.get('session_done', 0)}",
                f"op_count={final_state.get('op_count', 0)}",
                f"text_id={final_state.get('text_id', 0)}",
                f"perf_samples={perf_summary.get('samples', 0)}",
                f"avg_step_ms={perf_summary.get('avg_step_ms', 0)}",
                f"max_step_ms={perf_summary.get('max_step_ms', 0)}",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
