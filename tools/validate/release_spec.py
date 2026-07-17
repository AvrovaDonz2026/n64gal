#!/usr/bin/env python3
"""Normalize release publish specs while preserving the legacy schema."""

import argparse
import json
from pathlib import Path
import shlex
import sys


def _derived_doc_path(release_note, version, kind):
    note_path = Path(str(release_note))
    note_name = note_path.name
    suffix = ""
    if note_name.startswith("release-") and note_name.endswith(".md"):
        suffix = note_name[len("release-"):-3]
    elif version:
        suffix = str(version)
    if not suffix:
        return ""
    return str(note_path.with_name(f"release-{kind}-{suffix}.md"))


def normalize_release_spec(payload):
    if not isinstance(payload, dict):
        raise ValueError("release_spec")

    result = dict(payload)
    version = str(payload.get("version", ""))
    release_note = str(payload.get("release_note", ""))
    result["version"] = version
    result["release_note"] = release_note
    result["release_evidence"] = str(
        payload.get("release_evidence")
        or _derived_doc_path(release_note, version, "evidence")
    )
    result["release_package"] = str(
        payload.get("release_package")
        or _derived_doc_path(release_note, version, "package")
    )
    result["release_checklist"] = str(
        payload.get("release_checklist")
        or _derived_doc_path(release_note, version, "checklist")
    )

    raw_assets = payload.get("assets")
    if raw_assets is None:
        legacy_asset = payload.get("asset")
        raw_assets = [legacy_asset] if isinstance(legacy_asset, dict) else []
    if not isinstance(raw_assets, list):
        raise ValueError("release_spec.assets")
    if not raw_assets:
        raise ValueError("release_spec.assets")

    assets = []
    names = set()
    for index, raw_asset in enumerate(raw_assets):
        if not isinstance(raw_asset, dict):
            raise ValueError(f"release_spec.assets[{index}]")
        asset_path = str(raw_asset.get("path", ""))
        asset_name = str(raw_asset.get("name") or Path(asset_path).name)
        if not asset_path:
            raise ValueError(f"release_spec.assets[{index}].path")
        if "\n" in asset_path or "\r" in asset_path:
            raise ValueError(f"release_spec.assets[{index}].path")
        if not asset_name:
            raise ValueError(f"release_spec.assets[{index}].name")
        if asset_name != Path(asset_name).name or asset_name in (".", ".."):
            raise ValueError(f"release_spec.assets[{index}].name")
        if asset_name in names:
            raise ValueError(f"release_spec.assets[{index}].name")
        names.add(asset_name)
        assets.append({"name": asset_name, "path": asset_path})
    result["assets"] = assets
    return result


def load_release_spec(path):
    with open(path, "r", encoding="utf-8") as handle:
        return normalize_release_spec(json.load(handle))


def _emit_shell(spec):
    fields = {
        "SPEC_VERSION": spec.get("version", ""),
        "SPEC_REPOSITORY": spec.get("repository", ""),
        "SPEC_TAG": spec.get("tag", ""),
        "SPEC_RELEASE_URL": spec.get("release_url", ""),
        "SPEC_RELEASE_NOTE": spec.get("release_note", ""),
        "SPEC_RELEASE_EVIDENCE": spec.get("release_evidence", ""),
        "SPEC_RELEASE_PACKAGE": spec.get("release_package", ""),
        "SPEC_RELEASE_CHECKLIST": spec.get("release_checklist", ""),
    }
    for key, value in fields.items():
        print(f"{key}={shlex.quote(str(value))}")


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("release_spec")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--shell", action="store_true")
    group.add_argument("--asset-paths", action="store_true")
    group.add_argument("--asset-names", action="store_true")
    group.add_argument("--asset-path-for-name")
    group.add_argument("--json", action="store_true")
    args = parser.parse_args(argv[1:])

    try:
        spec = load_release_spec(args.release_spec)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"release spec error: {exc}", file=sys.stderr)
        return 1

    if args.asset_paths:
        for asset in spec["assets"]:
            print(asset["path"])
    elif args.asset_names:
        for asset in spec["assets"]:
            print(asset["name"])
    elif args.asset_path_for_name is not None:
        for asset in spec["assets"]:
            if asset["name"] == args.asset_path_for_name:
                print(asset["path"])
                return 0
        print(f"release spec asset not found: {args.asset_path_for_name}", file=sys.stderr)
        return 1
    elif args.json:
        print(json.dumps(spec, indent=2, sort_keys=True))
    else:
        _emit_shell(spec)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
