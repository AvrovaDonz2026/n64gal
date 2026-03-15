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
        shutil.copy2(ROOT / "docs" / "release-v0.1.0-alpha.md", temp_root / "docs" / "release-v0.1.0-alpha.md")
        shutil.copy2(ROOT / "docs" / "release-evidence-v0.1.0-alpha.md", temp_root / "docs" / "release-evidence-v0.1.0-alpha.md")
        shutil.copy2(ROOT / "docs" / "release-package-v0.1.0-alpha.md", temp_root / "docs" / "release-package-v0.1.0-alpha.md")
        shutil.copy2(ROOT / "docs" / "release-checklist-v1.0.0.md", temp_root / "docs" / "release-checklist-v1.0.0.md")
        shutil.copy2(ROOT / "assets" / "demo" / "demo.vnpak", temp_root / "assets" / "demo" / "demo.vnpak")

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

    print("test_release_audit_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
