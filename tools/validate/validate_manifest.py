#!/usr/bin/env python3
import json
import os
import re
import sys


VN_OK = 0
VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3
VN_E_UNSUPPORTED = -4

REQUIRED_API_RANGE_KEYS = (
    "runtime_api",
    "script_bytecode",
    "vnpak",
    "vnsave",
    "preview_protocol",
    "tool_manifest",
)

ALLOWED_KINDS = {
    "importer",
    "exporter",
    "validator",
    "migrator",
}

ALLOWED_STABILITY = {
    "experimental",
    "preview",
    "stable",
}

ALLOWED_CAPABILITIES = {
    "read.script",
    "write.script",
    "read.vnpak",
    "write.vnpak",
    "read.vnsave",
    "write.vnsave",
    "migrate.script",
    "migrate.vnpak",
    "migrate.vnsave",
    "emit.report",
}

NAME_RE = re.compile(r"^[a-z0-9._-]+$")


def error(trace_id, error_code, field, message):
    error_name = {
        VN_E_INVALID_ARG: "VN_E_INVALID_ARG",
        VN_E_IO: "VN_E_IO",
        VN_E_FORMAT: "VN_E_FORMAT",
        VN_E_UNSUPPORTED: "VN_E_UNSUPPORTED",
    }.get(error_code, "VN_E_UNKNOWN")
    parts = [
        f"trace_id={trace_id}",
        f"error_code={error_code}",
        f"error_name={error_name}",
    ]
    if field:
        parts.append(f"field={field}")
    parts.append(f"message={message}")
    print(" ".join(parts), file=sys.stderr)
    return 1


def ensure_string(value, field):
    if not isinstance(value, str) or value == "":
        raise ValueError(field)


def ensure_string_list(value, field):
    if not isinstance(value, list):
        raise ValueError(field)
    for item in value:
        ensure_string(item, field)


def main(argv):
    if len(argv) != 2 or argv[1] in ("-h", "--help"):
        print("usage: validate_manifest.py <manifest.json>", file=sys.stderr)
        return 2

    manifest_path = argv[1]
    if not os.path.exists(manifest_path):
        return error("tool.validate.manifest.io", VN_E_IO, "path", "manifest file not found")

    try:
        with open(manifest_path, "r", encoding="utf-8") as fp:
            data = json.load(fp)
    except OSError:
        return error("tool.validate.manifest.io", VN_E_IO, "path", "failed to open manifest")
    except json.JSONDecodeError:
        return error("tool.validate.manifest.format", VN_E_FORMAT, "json", "invalid json")

    if not isinstance(data, dict):
        return error("tool.validate.manifest.format", VN_E_FORMAT, "root", "manifest root must be object")

    try:
        manifest_version = data["manifest_version"]
        if not isinstance(manifest_version, int):
            raise ValueError("manifest_version")
        if manifest_version != 1:
            return error("tool.validate.manifest.unsupported", VN_E_UNSUPPORTED, "manifest_version", "unsupported manifest version")

        name = data["name"]
        ensure_string(name, "name")
        if NAME_RE.match(name) is None:
            return error("tool.validate.manifest.format", VN_E_FORMAT, "name", "invalid extension name")

        kind = data["kind"]
        ensure_string(kind, "kind")
        if kind not in ALLOWED_KINDS:
            return error("tool.validate.manifest.format", VN_E_FORMAT, "kind", "unsupported extension kind")

        version = data["version"]
        ensure_string(version, "version")

        entry = data["entry"]
        ensure_string(entry, "entry")

        stability = data["stability"]
        ensure_string(stability, "stability")
        if stability not in ALLOWED_STABILITY:
            return error("tool.validate.manifest.format", VN_E_FORMAT, "stability", "unsupported stability")

        api_range = data["api_range"]
        if not isinstance(api_range, dict):
            raise ValueError("api_range")
        for key in REQUIRED_API_RANGE_KEYS:
            if key not in api_range:
                return error("tool.validate.manifest.format", VN_E_FORMAT, f"api_range.{key}", "missing api_range key")
            ensure_string(api_range[key], f"api_range.{key}")

        capabilities = data["capabilities"]
        ensure_string_list(capabilities, "capabilities")
        if len(capabilities) == 0:
            return error("tool.validate.manifest.format", VN_E_FORMAT, "capabilities", "capabilities must not be empty")
        for capability in capabilities:
            if capability not in ALLOWED_CAPABILITIES:
                return error("tool.validate.manifest.format", VN_E_FORMAT, "capabilities", "unsupported capability")

        if "platforms" in data:
            ensure_string_list(data["platforms"], "platforms")
        if "arches" in data:
            ensure_string_list(data["arches"], "arches")
        if "inputs" in data:
            ensure_string_list(data["inputs"], "inputs")
        if "outputs" in data:
            ensure_string_list(data["outputs"], "outputs")
    except KeyError as exc:
        return error("tool.validate.manifest.format", VN_E_FORMAT, exc.args[0], "missing required field")
    except ValueError as exc:
        field = exc.args[0] if exc.args else "manifest"
        return error("tool.validate.manifest.format", VN_E_FORMAT, field, "invalid field type or empty value")

    print(
        " ".join(
            [
                "trace_id=tool.validate.manifest.ok",
                f"manifest={manifest_path}",
                f"name={name}",
                f"kind={kind}",
                f"stability={stability}",
                f"capability_count={len(capabilities)}",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
