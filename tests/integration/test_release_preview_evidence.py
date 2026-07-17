#!/usr/bin/env python3
import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_preview_evidence.sh"]


def paths_match(actual, expected):
    return isinstance(actual, str) and Path(actual).resolve() == expected.resolve()


def main():
    summary_path = ROOT / "tests" / "integration" / "release_preview_tmp.md"
    summary_json_path = ROOT / "tests" / "integration" / "release_preview_tmp.json"
    out_dir = ROOT / "tests" / "integration" / "release_preview_tmp"
    out_dir_arg = out_dir.relative_to(ROOT)
    request_path = out_dir / "preview_request.txt"
    response_path = out_dir / "preview_response.json"
    screenshot_path = out_dir / "content_preview.ppm"
    try:
        if summary_path.exists():
            summary_path.unlink()
    except FileNotFoundError:
        pass
    try:
        if summary_json_path.exists():
            summary_json_path.unlink()
    except FileNotFoundError:
        pass
    if out_dir.exists():
        import shutil
        shutil.rmtree(out_dir)

    help_proc = subprocess.run(
        SCRIPT + ["--help"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if help_proc.returncode != 0:
        print(f"preview help failed rc={help_proc.returncode} stderr={help_proc.stderr}", file=sys.stderr)
        return 1
    if "--summary-json-out <path>" not in help_proc.stderr or "--preview-bin <path>" not in help_proc.stderr:
        print("preview help missing hardened options", file=sys.stderr)
        return 1

    proc = subprocess.run(
        SCRIPT
        + [
            "--out-dir",
            str(out_dir_arg),
            "--summary-out",
            str(summary_path),
            "--summary-json-out",
            str(summary_json_path),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"preview evidence failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1
    if "trace_id=release.preview.ok" not in proc.stdout:
        print("missing preview evidence success trace", file=sys.stderr)
        return 1
    if not summary_path.exists():
        print("preview evidence summary missing", file=sys.stderr)
        return 1
    if not summary_json_path.exists():
        print("preview evidence json summary missing", file=sys.stderr)
        return 1

    summary_text = summary_path.read_text(encoding="utf-8")
    if "# Preview Evidence Summary" not in summary_text:
        print("preview evidence summary header missing", file=sys.stderr)
        return 1
    if "`trace_id=preview.ok`" not in summary_text:
        print("preview evidence summary missing preview ok trace", file=sys.stderr)
        return 1
    if "`preview_protocol=v1`" not in summary_text:
        print("preview evidence summary missing preview protocol check", file=sys.stderr)
        return 1
    if str(summary_json_path) not in summary_text:
        print("preview evidence summary missing summary json path", file=sys.stderr)
        return 1

    payload = json.loads(summary_json_path.read_text(encoding="utf-8"))
    if payload.get("trace_id") != "release.preview.ok" or payload.get("status") != "ok":
        print("preview evidence json missing ok trace/status", file=sys.stderr)
        return 1
    if payload.get("summary_md") != str(summary_path) or payload.get("summary_json") != str(summary_json_path):
        print("preview evidence json summary paths inconsistent", file=sys.stderr)
        return 1
    if (
        not paths_match(payload.get("request"), request_path)
        or not paths_match(payload.get("response"), response_path)
    ):
        print("preview evidence json request/response paths inconsistent", file=sys.stderr)
        return 1
    if not paths_match(payload.get("screenshot"), screenshot_path) or not screenshot_path.is_file():
        print("preview evidence screenshot path missing or inconsistent", file=sys.stderr)
        return 1
    checks = payload.get("checks")
    if not isinstance(checks, dict) or not all(checks.values()):
        print("preview evidence json checks missing or false", file=sys.stderr)
        return 1
    facts = payload.get("response_facts", {})
    if facts.get("scene_name") != "Opening" or facts.get("backend_name") != "scalar":
        print("preview evidence json response facts mismatch", file=sys.stderr)
        return 1
    if facts.get("screenshot_width") != 64 or facts.get("screenshot_height") != 48:
        print("preview evidence json screenshot dimensions mismatch", file=sys.stderr)
        return 1
    if facts.get("screenshot_crc32") != "995ff007":
        print("preview evidence json screenshot CRC mismatch", file=sys.stderr)
        return 1
    if not screenshot_path.read_bytes().startswith(b"P6\n64 48\n255\n"):
        print("preview evidence screenshot is not the expected PPM", file=sys.stderr)
        return 1
    if not isinstance(payload.get("request_lines"), list) or not payload["request_lines"]:
        print("preview evidence json request lines missing", file=sys.stderr)
        return 1

    summary_path.unlink()
    summary_json_path.unlink()
    import shutil
    shutil.rmtree(out_dir)
    print("test_release_preview_evidence ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
