#!/usr/bin/env python3
from pathlib import Path
import json
import sys


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


def release_doc_paths_from_spec(spec_payload):
    release_note = spec_payload.get("release_note", "")
    version = str(spec_payload.get("version", ""))
    note_path = Path(release_note)
    note_name = note_path.name
    suffix = ""
    if note_name.startswith("release-") and note_name.endswith(".md"):
        suffix = note_name[len("release-"):-3]
    elif version:
        suffix = version
    return (
        release_note,
        str(note_path.with_name(f"release-evidence-{suffix}.md")),
        str(note_path.with_name(f"release-package-{suffix}.md")),
    )


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
    release_spec = "docs/release-publish-v0.1.0-alpha.json"
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
        checklist = read_text(root, "docs/release-checklist-v1.0.0.md")
        release_spec_text = read_text(root, release_spec)
        release_spec_payload = json.loads(release_spec_text)
        release_note_rel, release_evidence_rel, release_package_rel = release_doc_paths_from_spec(release_spec_payload)
        release_note = read_text(root, release_note_rel)
        release_evidence = read_text(root, release_evidence_rel)
        release_package = read_text(root, release_package_rel)
    except FileNotFoundError as exc:
        return error("tool.validate.release_audit.io", VN_E_IO, str(exc), "required release audit file missing")
    except OSError:
        return error("tool.validate.release_audit.io", VN_E_IO, "root", "failed reading release audit files")
    except ValueError:
        return error("tool.validate.release_audit.format", VN_E_FORMAT, "release_spec", "invalid release spec json")

    try:
        spec_tag = str(release_spec_payload.get("tag", ""))
        spec_release_url = str(release_spec_payload.get("release_url", ""))
        spec_release_note = str(release_spec_payload.get("release_note", ""))
        asset = release_spec_payload.get("asset")
        if not isinstance(asset, dict):
            raise ValueError("release_spec.asset")
        spec_asset_path = str(asset.get("path", ""))

        if not (root / "assets" / "demo" / "demo.vnpak").exists():
            raise FileNotFoundError("assets/demo/demo.vnpak")
        if spec_tag == "v0.1.0-alpha":
            require_contains(changelog, "## v0.1.0-alpha", "changelog.alpha")
            require_contains(release_note, "`v0.1.0-alpha`", "release_note.alpha")
        else:
            require_contains(release_note, "`v1.0.0`", "release_note_v1.version")
            require_contains(release_note, "`runtime-session-only`", "release_note_v1.save_scope")
            require_contains(release_evidence, "`ci-matrix`", "release_evidence_v1.ci_matrix")
            require_contains(release_package, "`docs/release-publish-v1.0.0.json`", "release_package_v1.release_spec")
        require_contains_any(
            release_evidence,
            [f"`{basename(spec_asset_path)}`", f"`{spec_asset_path}`"],
            "release_evidence.asset",
        )
        require_contains(release_package, f"`{spec_asset_path}`", "release_package.asset")
        require_contains(checklist, "`python3 tools/toolchain.py validate-all`", "checklist.validate_all")
        require_contains(checklist, "`python3 tools/toolchain.py release-gate`", "checklist.release_gate")

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
            demo_entry = None
            for entry in files:
                if isinstance(entry, dict) and entry.get("path") == "demo.vnpak":
                    demo_entry = entry
                    break
            if demo_entry is None:
                raise ValueError("bundle_manifest.demo")
            if not demo_entry.get("sha256") or int(demo_entry.get("bytes", 0)) <= 0:
                raise ValueError("bundle_manifest.demo_fields")
        if publish_map:
            publish_map_path = root / publish_map
            if not publish_map_path.exists():
                raise FileNotFoundError(publish_map)
            publish_payload = json.loads(publish_map_path.read_text(encoding="utf-8"))
            if publish_payload.get("tag") != spec_tag:
                raise ValueError("publish_map.tag")
            if publish_payload.get("release_url") != spec_release_url:
                raise ValueError("publish_map.release_url")
            publish_asset = publish_payload.get("asset")
            if not isinstance(publish_asset, dict):
                raise ValueError("publish_map.asset")
            if not publish_asset.get("path", "").endswith(spec_asset_path):
                raise ValueError("publish_map.asset_path")
            if not publish_asset.get("sha256") or int(publish_asset.get("bytes", 0)) <= 0:
                raise ValueError("publish_map.asset_fields")
            if publish_payload.get("release_spec") and publish_payload.get("release_spec") != str(release_spec):
                raise ValueError("publish_map.release_spec")
        if release_spec_payload.get("tag") != spec_tag:
            raise ValueError("release_spec.tag")
        if release_spec_payload.get("release_url") != spec_release_url:
            raise ValueError("release_spec.release_url")
        if release_spec_payload.get("release_note") != spec_release_note:
            raise ValueError("release_spec.release_note")
        if asset.get("path") != spec_asset_path:
            raise ValueError("release_spec.asset_path")
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
