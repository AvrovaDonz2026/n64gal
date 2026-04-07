#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_release_audit.py"]


def run_case(args):
    proc = subprocess.run(
        TOOL + args,
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def write_release_artifacts(temp_root: Path, version: str):
    docs_dir = temp_root / "docs"
    release_spec_name = f"release-publish-{version}.json"
    release_spec_rel = f"docs/{release_spec_name}"
    release_note_rel = f"docs/release-{version}.md"
    release_evidence_rel = f"docs/release-evidence-{version}.md"
    release_package_rel = f"docs/release-package-{version}.md"

    shutil.copy2(ROOT / release_note_rel, temp_root / release_note_rel)
    shutil.copy2(ROOT / release_evidence_rel, temp_root / release_evidence_rel)
    shutil.copy2(ROOT / release_package_rel, temp_root / release_package_rel)
    shutil.copy2(ROOT / release_spec_rel, temp_root / release_spec_rel)

    return {
        "version": version,
        "tag": version,
        "release_url": f"https://github.com/AvrovaDonz2026/n64gal/releases/tag/{version}",
        "release_spec": docs_dir / release_spec_name,
        "release_spec_rel": release_spec_rel,
    }


def main():
    rc, out, err = run_case(["--allow-dirty"])
    if rc != 0:
        print(f"release_audit current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.release_audit.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_release_audit_") as temp_dir:
        temp_root = Path(temp_dir)
        (temp_root / "docs").mkdir(parents=True, exist_ok=True)
        (temp_root / "assets" / "demo").mkdir(parents=True, exist_ok=True)

        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        shutil.copy2(ROOT / "issue.md", temp_root / "issue.md")
        shutil.copy2(ROOT / "CHANGELOG.md", temp_root / "CHANGELOG.md")
        shutil.copy2(ROOT / "docs" / "release-checklist-v1.0.0.md", temp_root / "docs" / "release-checklist-v1.0.0.md")
        shutil.copy2(ROOT / "assets" / "demo" / "demo.vnpak", temp_root / "assets" / "demo" / "demo.vnpak")

        alpha = write_release_artifacts(temp_root, "v0.1.0-alpha")
        v1 = write_release_artifacts(temp_root, "v1.0.0")

        bundle_manifest = temp_root / "release_bundle_manifest.json"
        publish_map = temp_root / "release_publish_map.json"
        bundle_manifest.write_text(
            '{"files":[{"path":"demo.vnpak","sha256":"deadbeef","bytes":1}]}\n',
            encoding="utf-8",
        )
        publish_map.write_text(
            '{"tag":"v0.1.0-alpha","release_url":"https://github.com/AvrovaDonz2026/n64gal/releases/tag/v0.1.0-alpha","release_spec":"%s","asset":{"path":"assets/demo/demo.vnpak","sha256":"deadbeef","bytes":1}}\n'
            % str(alpha["release_spec"]),
            encoding="utf-8",
        )

        rc, out, err = run_case(["--allow-dirty", "--bundle-manifest", str(bundle_manifest), "--publish-map", str(publish_map), "--release-spec", str(alpha["release_spec"]), str(temp_root)])
        if rc != 0:
            print(f"release_audit bundle manifest failed rc={rc} stderr={err}", file=sys.stderr)
            return 1

        publish_map.write_text(
            '{"tag":"v1.0.0","release_url":"https://github.com/AvrovaDonz2026/n64gal/releases/tag/v1.0.0","release_spec":"%s","asset":{"path":"assets/demo/demo.vnpak","sha256":"deadbeef","bytes":1}}\n'
            % str(v1["release_spec"]),
            encoding="utf-8",
        )
        rc, out, err = run_case(["--allow-dirty", "--bundle-manifest", str(bundle_manifest), "--publish-map", str(publish_map), "--release-spec", str(v1["release_spec"]), str(temp_root)])
        if rc != 0:
            print(f"release_audit v1 bundle manifest failed rc={rc} stderr={err}", file=sys.stderr)
            return 1

        broken = (temp_root / "docs" / "release-checklist-v1.0.0.md").read_text(encoding="utf-8")
        broken = broken.replace("`python3 tools/toolchain.py validate-all`", "`python3 tools/toolchain.py validate-none`", 1)
        (temp_root / "docs" / "release-checklist-v1.0.0.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(["--allow-dirty", str(temp_root)])
        if rc == 0:
            print("broken release audit should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.release_audit.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

        bundle_manifest.write_text('{"files":[]}\n', encoding="utf-8")
        rc, out, err = run_case(["--allow-dirty", "--bundle-manifest", str(bundle_manifest), "--publish-map", str(publish_map), "--release-spec", str(alpha["release_spec"]), str(temp_root)])
        if rc == 0:
            print("broken bundle manifest should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.release_audit.format" not in err:
            print("missing bundle manifest format failure trace_id", file=sys.stderr)
            return 1

        publish_map.write_text('{"tag":"broken","release_url":"bad","release_spec":"broken","asset":{"path":"broken","sha256":"","bytes":0}}\n', encoding="utf-8")
        rc, out, err = run_case(["--allow-dirty", "--publish-map", str(publish_map), str(temp_root)])
        if rc == 0:
            print("broken publish map should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.release_audit.format" not in err:
            print("missing publish map format failure trace_id", file=sys.stderr)
            return 1

        alpha["release_spec"].write_text(
            '{"version":"v0.1.0-alpha","tag":"broken","release_url":"bad","release_note":"docs/release-v0.1.0-alpha.md","asset":{"path":"broken"}}\n',
            encoding="utf-8",
        )
        rc, out, err = run_case(["--allow-dirty", "--release-spec", str(alpha["release_spec"]), str(temp_root)])
        if rc == 0:
            print("broken release spec should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.release_audit.format" not in err:
            print("missing release spec format failure trace_id", file=sys.stderr)
            return 1

    print("test_release_audit_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
