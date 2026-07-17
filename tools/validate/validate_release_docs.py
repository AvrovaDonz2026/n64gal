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


def read_text(root, rel):
    path = root / rel
    if not path.exists():
        raise FileNotFoundError(rel)
    return path.read_text(encoding="utf-8")


def read_json(root, rel):
    return json.loads(read_text(root, rel))


def require_contains(text, needle, field):
    if needle not in text:
        raise ValueError(field)


def require_equal(actual, expected, field):
    if actual != expected:
        raise ValueError(field)


def validate_spec_docs(root, spec_rel, expected_tag, require_explicit):
    raw = read_json(root, spec_rel)
    spec = normalize_release_spec(raw)
    require_equal(spec.get("version"), expected_tag, f"{spec_rel}.version")
    require_equal(spec.get("tag"), expected_tag, f"{spec_rel}.tag")
    require_equal(
        spec.get("release_url"),
        f"https://github.com/AvrovaDonz2026/n64gal/releases/tag/{expected_tag}",
        f"{spec_rel}.release_url",
    )
    if require_explicit:
        for field in (
            "release_note",
            "release_evidence",
            "release_package",
            "release_checklist",
            "assets",
        ):
            if field not in raw:
                raise ValueError(f"{spec_rel}.{field}")
        if "asset" in raw:
            raise ValueError(f"{spec_rel}.legacy_asset")

    docs = {}
    for field in (
        "release_note",
        "release_evidence",
        "release_package",
        "release_checklist",
    ):
        path = spec[field]
        if not path:
            raise ValueError(f"{spec_rel}.{field}")
        docs[field] = read_text(root, path)
        require_contains(docs[field], expected_tag, f"{path}.version")
    return raw, spec, docs


def main(argv):
    root = Path(".")
    if len(argv) > 2:
        return error("tool.validate.release_docs.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_release_docs.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])
    if not root.exists():
        return error("tool.validate.release_docs.io", VN_E_IO, "root", "root directory not found")

    try:
        readme = read_text(root, "README.md")
        issue = read_text(root, "issue.md")
        changelog = read_text(root, "CHANGELOG.md")
        roadmap_v1 = read_text(root, "docs/release-roadmap-1.0.0.md")
        roadmap_v11 = read_text(root, "docs/release-roadmap-1.1.0.md")
        triage_v1 = read_text(root, "docs/release-triage-v1.0.0.md")
        _, alpha, alpha_docs = validate_spec_docs(
            root, "docs/release-publish-v0.1.0-alpha.json", "v0.1.0-alpha", False
        )
        _, v1, v1_docs = validate_spec_docs(
            root, "docs/release-publish-v1.0.0.json", "v1.0.0", False
        )
        raw_v11, v11, v11_docs = validate_spec_docs(
            root, "docs/release-publish-v1.1.0.json", "v1.1.0", True
        )
    except FileNotFoundError as exc:
        return error("tool.validate.release_docs.io", VN_E_IO, str(exc), "required release document missing")
    except OSError:
        return error("tool.validate.release_docs.io", VN_E_IO, "root", "failed reading release docs")
    except (ValueError, json.JSONDecodeError) as exc:
        return error("tool.validate.release_docs.format", VN_E_FORMAT, str(exc), "invalid release document or spec")

    try:
        require_contains(readme, "当前对外版本状态：`v1.0.0` 已发布", "readme.stable_release")
        require_contains(readme, "当前开发版本：`v1.1.0`", "readme.current_development")
        require_contains(readme, "RVV native 因没有设备证据继续延期", "readme.rvv_deferred")
        require_contains(issue, "`v1.0.0` 已发布", "issue.v1_published")

        require_contains(changelog, "## Unreleased (`v1.1.0`)", "changelog.unreleased_v11")
        require_contains(changelog, "## v1.0.0 - 2026-04-08", "changelog.v1")
        require_contains(changelog, "## v0.1.0-alpha", "changelog.alpha")

        require_equal(bool(alpha.get("prerelease")), True, "alpha.prerelease")
        require_equal([item["path"] for item in alpha["assets"]], ["assets/demo/demo.vnpak"], "alpha.assets")
        require_contains(alpha_docs["release_note"], "`riscv64 Linux`", "alpha.note.platform")

        require_equal(bool(v1.get("prerelease")), False, "v1.prerelease")
        require_equal([item["path"] for item in v1["assets"]], ["assets/demo/demo.vnpak"], "v1.assets")
        require_contains(v1_docs["release_note"], "`runtime-session-only`", "v1.note.save_scope")
        require_contains(v1_docs["release_evidence"], "`ci-matrix`", "v1.evidence.ci")
        if "- [ ]" in v1_docs["release_checklist"]:
            raise ValueError("v1.checklist.archived_status")

        require_equal(bool(raw_v11.get("draft")), False, "v11.draft")
        require_equal(bool(raw_v11.get("prerelease")), False, "v11.prerelease")
        require_equal(
            [item["path"] for item in v11["assets"]],
            ["assets/demo/demo.vnpak", "assets/demo/content-demo.vnpak"],
            "v11.assets",
        )
        require_contains(v11_docs["release_note"], "尚未创建", "v11.note.not_published")
        require_contains(v11_docs["release_note"], "Runtime API", "v11.note.runtime_api")
        require_contains(v11_docs["release_note"], "`vnpak`", "v11.note.vnpak")
        require_contains(v11_docs["release_note"], "`vnsave v1`", "v11.note.vnsave")
        require_contains(v11_docs["release_evidence"], "四个正式平台", "v11.evidence.platforms")
        require_contains(v11_docs["release_package"], "`assets[]`", "v11.package.assets")
        require_contains(v11_docs["release_checklist"], "RVV/riscv64 native", "v11.checklist.rvv")
        require_contains(roadmap_v11, "真实内容渲染切片", "v11.roadmap.slice")
        require_contains(roadmap_v11, "`vnpak v2`", "v11.roadmap.vnpak")
        require_contains(roadmap_v11, "`vnsave v1`", "v11.roadmap.vnsave")

        require_contains(roadmap_v1, "docs/release-triage-v1.0.0.md", "v1.roadmap.triage")
        require_contains(triage_v1, "## 2. Must Have", "v1.triage.must_have")
    except ValueError as exc:
        return error("tool.validate.release_docs.format", VN_E_FORMAT, str(exc), "release document drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.release_docs.ok",
                f"root={root}",
                "historical_v1=validated",
                "current_v1_1=validated",
                f"current_assets={len(v11['assets'])}",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
