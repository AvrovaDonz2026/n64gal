#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_release_docs.py"]


def run_case(root_path: Path):
    proc = subprocess.run(
        TOOL + [str(root_path)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def main():
    rc, out, err = run_case(ROOT)
    if rc != 0:
        print(f"release_docs current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.release_docs.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_release_docs_") as temp_dir:
        temp_root = Path(temp_dir)
        (temp_root / "docs").mkdir(parents=True, exist_ok=True)

        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        shutil.copy2(ROOT / "issue.md", temp_root / "issue.md")
        shutil.copy2(ROOT / "CHANGELOG.md", temp_root / "CHANGELOG.md")
        shutil.copy2(ROOT / "docs" / "release-v0.1.0-alpha.md", temp_root / "docs" / "release-v0.1.0-alpha.md")
        shutil.copy2(ROOT / "docs" / "release-evidence-v0.1.0-alpha.md", temp_root / "docs" / "release-evidence-v0.1.0-alpha.md")
        shutil.copy2(ROOT / "docs" / "release-package-v0.1.0-alpha.md", temp_root / "docs" / "release-package-v0.1.0-alpha.md")
        shutil.copy2(ROOT / "docs" / "release-v1.0.0.md", temp_root / "docs" / "release-v1.0.0.md")
        shutil.copy2(ROOT / "docs" / "release-evidence-v1.0.0.md", temp_root / "docs" / "release-evidence-v1.0.0.md")
        shutil.copy2(ROOT / "docs" / "release-package-v1.0.0.md", temp_root / "docs" / "release-package-v1.0.0.md")
        shutil.copy2(ROOT / "docs" / "release-checklist-v0.1.0-alpha.md", temp_root / "docs" / "release-checklist-v0.1.0-alpha.md")
        shutil.copy2(ROOT / "docs" / "release-gap-v0.1.0-mvp.md", temp_root / "docs" / "release-gap-v0.1.0-mvp.md")
        shutil.copy2(ROOT / "docs" / "release-roadmap-1.0.0.md", temp_root / "docs" / "release-roadmap-1.0.0.md")
        shutil.copy2(ROOT / "docs" / "release-triage-v1.0.0.md", temp_root / "docs" / "release-triage-v1.0.0.md")
        shutil.copy2(ROOT / "docs" / "release-checklist-v1.0.0.md", temp_root / "docs" / "release-checklist-v1.0.0.md")
        shutil.copy2(ROOT / "docs" / "release-publish-v0.1.0-alpha.json", temp_root / "docs" / "release-publish-v0.1.0-alpha.json")
        shutil.copy2(ROOT / "docs" / "release-publish-v1.0.0.json", temp_root / "docs" / "release-publish-v1.0.0.json")

        broken = (temp_root / "docs" / "release-v0.1.0-alpha.md").read_text(encoding="utf-8")
        broken = broken.replace("`JIT`；当前仍是文档化实验方向，不是 release blocker。",
                                "`JIT` 已进入当前发布范围。",
                                1)
        (temp_root / "docs" / "release-v0.1.0-alpha.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken release docs should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.release_docs.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_release_docs_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
