#!/usr/bin/env python3
"""Validate the v1.1 real-content soak evidence contract."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


EXPECTED_SCENES = ["Opening", "Gallery"]
MIN_SCENE_MS = 450000


def fail(field, message):
    print(
        "trace_id=tool.validate.content_soak.failed "
        f"error_code=-3 error_name=VN_E_FORMAT field={field} message={message}",
        file=sys.stderr,
    )
    return 1


def is_int(value):
    return isinstance(value, int) and not isinstance(value, bool)


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("summary_json")
    parser.add_argument("--summary-md", required=True)
    parser.add_argument("--pack", required=True)
    args = parser.parse_args(argv[1:])

    try:
        payload = json.loads(Path(args.summary_json).read_text(encoding="utf-8"))
        markdown = Path(args.summary_md).read_text(encoding="utf-8")
        pack_bytes = Path(args.pack).read_bytes()
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        return fail("input", str(exc))

    if not isinstance(payload, dict):
        return fail("root", "summary JSON must be an object")
    if payload.get("schema_version") != 1:
        return fail("schema_version", "expected schema version 1")
    if payload.get("status") != "success":
        return fail("status", "content soak did not succeed")
    if payload.get("scenes") != EXPECTED_SCENES:
        return fail("scenes", "expected Opening,Gallery in canonical order")
    if payload.get("source_state") not in ("clean", "dirty"):
        return fail("source_state", "expected clean or dirty")
    if Path(str(payload.get("pack", ""))).name != "content-demo.vnpak":
        return fail("pack", "expected content-demo.vnpak")
    if re.fullmatch(r"[0-9a-f]{64}", str(payload.get("pack_sha256", ""))) is None:
        return fail("pack_sha256", "expected lowercase SHA256")
    actual_pack_sha256 = hashlib.sha256(pack_bytes).hexdigest()
    if payload.get("pack_sha256") != actual_pack_sha256:
        return fail("pack_sha256", "summary SHA256 does not match the release asset")

    frames = payload.get("frames_per_scene")
    dt_ms = payload.get("dt_ms")
    total_ms = payload.get("total_simulated_ms")
    if not is_int(frames) or frames <= 0:
        return fail("frames_per_scene", "expected a positive integer")
    if not is_int(dt_ms) or dt_ms <= 0:
        return fail("dt_ms", "expected a positive integer")
    if not is_int(total_ms):
        return fail("total_simulated_ms", "expected an integer")
    if frames * dt_ms < MIN_SCENE_MS:
        return fail("frames_per_scene", "each scene must simulate at least 450 seconds")
    if total_ms != frames * dt_ms * len(EXPECTED_SCENES) or total_ms < MIN_SCENE_MS * 2:
        return fail("total_simulated_ms", "expected the exact duration of both scenes")

    results = payload.get("scene_results")
    if not isinstance(results, list) or len(results) != len(EXPECTED_SCENES):
        return fail("scene_results", "expected one result per content scene")
    for index, expected_scene in enumerate(EXPECTED_SCENES):
        result = results[index]
        if not isinstance(result, dict) or result.get("scene") != expected_scene:
            return fail(f"scene_results[{index}].scene", "scene order mismatch")
        if result.get("status") != "success":
            return fail(f"scene_results[{index}].status", "scene did not succeed")
        if result.get("frames_executed") != frames:
            return fail(f"scene_results[{index}].frames_executed", "frame count mismatch")
        if result.get("vm_ended") != 1 or result.get("vm_error") != 0:
            return fail(f"scene_results[{index}].vm", "expected ended VM without error")

    if "# Demo Soak Summary" not in markdown or "- Status: `success`" not in markdown:
        return fail("summary_md", "markdown summary is not a successful demo soak")
    if "- Scenes: `Opening,Gallery`" not in markdown:
        return fail("summary_md.scenes", "markdown scene list mismatch")

    print(
        "trace_id=tool.validate.content_soak.ok "
        f"summary_json={args.summary_json} summary_md={args.summary_md} "
        f"pack={args.pack} pack_sha256={actual_pack_sha256} "
        f"frames_per_scene={frames} dt_ms={dt_ms} total_simulated_ms={total_ms}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
