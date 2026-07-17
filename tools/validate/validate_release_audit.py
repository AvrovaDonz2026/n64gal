#!/usr/bin/env python3
from pathlib import Path
import json
import sys

from release_spec import normalize_release_spec


VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3


def error(trace_id, error_code, field, message):
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


def read_text(root: Path, rel: str):
    path = root / rel
    if not path.exists():
        raise FileNotFoundError(rel)
    return path.read_text(encoding="utf-8")


def require_contains(text: str, needle: str, field: str):
    if needle not in text:
        raise ValueError(field)


def require_contains_any(text: str, needles, field: str):
    for needle in needles:
        if needle in text:
            return
    raise ValueError(field)


def worktree_dirty(root: Path) -> int:
    status = (root / ".git")
    if not status.exists():
        return 0
    import subprocess
    proc = subprocess.run(
        ["git", "status", "--porcelain"],
        cwd=root,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError("git status failed")
    return 1 if proc.stdout.strip() else 0


def basename(path_text: str) -> str:
    return Path(path_text).name


def main(argv):
    root = Path(".")
    allow_dirty = 0
    skip_git = 0
    release_gate_summary = ""
    soak_summary = ""
    bundle_manifest = ""
    publish_map = ""
    release_spec = "docs/release-publish-v1.1.0.json"
    i = 1

    while i < len(argv):
        arg = argv[i]
        if arg in ("-h", "--help"):
            print("usage: validate_release_audit.py [--allow-dirty|--require-clean] [--skip-git] [--release-gate-summary <path>] [--soak-summary <path>] [--bundle-manifest <path>] [--publish-map <path>] [--release-spec <path>] [root]", file=sys.stderr)
            return 2
        if arg == "--allow-dirty":
            allow_dirty = 1
            i += 1
            continue
        if arg == "--require-clean":
            allow_dirty = 0
            i += 1
            continue
        if arg == "--skip-git":
            skip_git = 1
            i += 1
            continue
        if arg == "--release-gate-summary":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_audit.usage", VN_E_INVALID_ARG, "release_gate_summary", "missing value")
            release_gate_summary = argv[i]
            i += 1
            continue
        if arg == "--soak-summary":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_audit.usage", VN_E_INVALID_ARG, "soak_summary", "missing value")
            soak_summary = argv[i]
            i += 1
            continue
        if arg == "--bundle-manifest":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_audit.usage", VN_E_INVALID_ARG, "bundle_manifest", "missing value")
            bundle_manifest = argv[i]
            i += 1
            continue
        if arg == "--publish-map":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_audit.usage", VN_E_INVALID_ARG, "publish_map", "missing value")
            publish_map = argv[i]
            i += 1
            continue
        if arg == "--release-spec":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_audit.usage", VN_E_INVALID_ARG, "release_spec", "missing value")
            release_spec = argv[i]
            i += 1
            continue
        if arg.startswith("-"):
            return error("tool.validate.release_audit.usage", VN_E_INVALID_ARG, "argv", "unexpected flag")
        root = Path(arg)
        i += 1

    if not root.exists():
        return error("tool.validate.release_audit.io", VN_E_IO, "root", "root directory not found")

    try:
        readme = read_text(root, "README.md")
        issue = read_text(root, "issue.md")
        changelog = read_text(root, "CHANGELOG.md")
        release_spec_text = read_text(root, release_spec)
        release_spec_payload = normalize_release_spec(json.loads(release_spec_text))
        release_note = read_text(root, release_spec_payload["release_note"])
        release_evidence = read_text(root, release_spec_payload["release_evidence"])
        release_package = read_text(root, release_spec_payload["release_package"])
        checklist = read_text(root, release_spec_payload["release_checklist"])
    except FileNotFoundError as exc:
        return error("tool.validate.release_audit.io", VN_E_IO, str(exc), "required release audit file missing")
    except OSError:
        return error("tool.validate.release_audit.io", VN_E_IO, "root", "failed reading release audit files")
    except ValueError:
        return error("tool.validate.release_audit.format", VN_E_FORMAT, "release_spec", "invalid release spec json")

    try:
        spec_tag = str(release_spec_payload.get("tag", ""))
        spec_release_url = str(release_spec_payload.get("release_url", ""))
        spec_version = str(release_spec_payload.get("version", ""))
        spec_release_note = str(release_spec_payload.get("release_note", ""))
        if not spec_version or spec_tag != spec_version:
            raise ValueError("release_spec.version_tag")
        if not spec_release_url.endswith(f"/tag/{spec_tag}"):
            raise ValueError("release_spec.release_url")
        require_contains_any(changelog, [f"## {spec_version}", f"`{spec_version}`"], "changelog.version")
        require_contains(release_note, f"`{spec_version}`", "release_note.version")
        require_contains(release_evidence, spec_version, "release_evidence.version")
        require_contains(release_package, spec_version, "release_package.version")
        require_contains(checklist, spec_version, "release_checklist.version")
        for asset in release_spec_payload["assets"]:
            spec_asset_path = asset["path"]
            if not (root / spec_asset_path).exists():
                raise FileNotFoundError(spec_asset_path)
            require_contains_any(
                release_evidence,
                [f"`{asset['name']}`", f"`{spec_asset_path}`"],
                f"release_evidence.asset.{asset['name']}",
            )
            require_contains(release_package, f"`{spec_asset_path}`", f"release_package.asset.{asset['name']}")
        version_core = spec_version.lstrip("v").split("-", 1)[0]
        try:
            major_version = int(version_core.split(".", 1)[0])
        except ValueError:
            raise ValueError("release_spec.version")
        if major_version >= 1:
            require_contains(checklist, "python3 tools/toolchain.py validate-all", "checklist.validate_all")
            require_contains(checklist, "python3 tools/toolchain.py release-gate", "checklist.release_gate")

        if skip_git == 0 and allow_dirty == 0 and worktree_dirty(root) != 0:
            return error("tool.validate.release_audit.format", VN_E_FORMAT, "worktree", "worktree must be clean")

        if release_gate_summary:
            if not (root / release_gate_summary).exists():
                raise FileNotFoundError(release_gate_summary)
        if soak_summary:
            if not (root / soak_summary).exists():
                raise FileNotFoundError(soak_summary)
        if bundle_manifest:
            bundle_manifest_path = root / bundle_manifest
            if not bundle_manifest_path.exists():
                raise FileNotFoundError(bundle_manifest)
            bundle_payload = json.loads(bundle_manifest_path.read_text(encoding="utf-8"))
            files = bundle_payload.get("files")
            if not isinstance(files, list) or not files:
                raise ValueError("bundle_manifest.files")
            manifest_entries = {
                entry.get("path"): entry for entry in files if isinstance(entry, dict)
            }
            for asset in release_spec_payload["assets"]:
                asset_entry = manifest_entries.get(asset["name"])
                if asset_entry is None:
                    raise ValueError(f"bundle_manifest.assets.{asset['name']}")
                if not asset_entry.get("sha256") or int(asset_entry.get("bytes", 0)) <= 0:
                    raise ValueError(f"bundle_manifest.assets.{asset['name']}.fields")
        if publish_map:
            publish_map_path = root / publish_map
            if not publish_map_path.exists():
                raise FileNotFoundError(publish_map)
            publish_payload = json.loads(publish_map_path.read_text(encoding="utf-8"))
            if publish_payload.get("tag") != spec_tag:
                raise ValueError("publish_map.tag")
            if publish_payload.get("release_url") != spec_release_url:
                raise ValueError("publish_map.release_url")
            publish_assets = publish_payload.get("assets")
            if publish_assets is None:
                publish_asset = publish_payload.get("asset")
                publish_assets = [publish_asset] if isinstance(publish_asset, dict) else []
            if not isinstance(publish_assets, list):
                raise ValueError("publish_map.assets")
            published_by_name = {
                str(entry.get("name") or basename(str(entry.get("path", "")))): entry
                for entry in publish_assets
                if isinstance(entry, dict)
            }
            for asset in release_spec_payload["assets"]:
                publish_asset = published_by_name.get(asset["name"])
                if publish_asset is None:
                    raise ValueError(f"publish_map.assets.{asset['name']}")
                if not str(publish_asset.get("path", "")).endswith(asset["path"]):
                    raise ValueError(f"publish_map.assets.{asset['name']}.path")
                if not publish_asset.get("sha256") or int(publish_asset.get("bytes", 0)) <= 0:
                    raise ValueError(f"publish_map.assets.{asset['name']}.fields")
            if publish_payload.get("release_spec") and publish_payload.get("release_spec") != str(release_spec):
                raise ValueError("publish_map.release_spec")
        if release_spec_payload.get("tag") != spec_tag:
            raise ValueError("release_spec.tag")
        if release_spec_payload.get("release_url") != spec_release_url:
            raise ValueError("release_spec.release_url")
        if release_spec_payload.get("release_note") != spec_release_note:
            raise ValueError("release_spec.release_note")
    except FileNotFoundError as exc:
        return error("tool.validate.release_audit.io", VN_E_IO, str(exc), "required release artifact missing")
    except ValueError as exc:
        return error("tool.validate.release_audit.format", VN_E_FORMAT, str(exc), "release audit drift detected")
    except RuntimeError:
        return error("tool.validate.release_audit.io", VN_E_IO, "git", "failed to inspect git status")

    print(
        " ".join(
            [
                "trace_id=tool.validate.release_audit.ok",
                f"root={root}",
                f"allow_dirty={allow_dirty}",
                f"skip_git={skip_git}",
                f"release_gate_summary={release_gate_summary if release_gate_summary else 'n/a'}",
                f"soak_summary={soak_summary if soak_summary else 'n/a'}",
                f"bundle_manifest={bundle_manifest if bundle_manifest else 'n/a'}",
                f"publish_map={publish_map if publish_map else 'n/a'}",
                f"release_spec={release_spec if release_spec else 'n/a'}",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
