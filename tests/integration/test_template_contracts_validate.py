#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_template_contracts.py"]


def run_case(root_path: Path):
    proc = subprocess.run(
        TOOL + [str(root_path)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def copy_file(src: Path, dst: Path):
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def main():
    rc, out, err = run_case(ROOT)
    if rc != 0:
        print(f"template_contracts current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.template_contracts.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_template_contracts_") as temp_dir:
        temp_root = Path(temp_dir)
        copy_file(ROOT / ".gitignore", temp_root / ".gitignore")
        copy_file(ROOT / "README.md", temp_root / "README.md")
        copy_file(ROOT / "docs" / "project-layout.md", temp_root / "docs" / "project-layout.md")
        copy_file(ROOT / "templates" / "minimal-vn" / "README.md", temp_root / "templates" / "minimal-vn" / "README.md")
        copy_file(ROOT / "templates" / "minimal-vn" / "template.json", temp_root / "templates" / "minimal-vn" / "template.json")
        copy_file(ROOT / "templates" / "minimal-vn" / "assets" / "scripts" / "S0.vns.txt", temp_root / "templates" / "minimal-vn" / "assets" / "scripts" / "S0.vns.txt")
        copy_file(ROOT / "templates" / "minimal-vn" / "assets" / "scripts" / "S1.vns.txt", temp_root / "templates" / "minimal-vn" / "assets" / "scripts" / "S1.vns.txt")
        copy_file(ROOT / "templates" / "minimal-vn" / "assets" / "scripts" / "S2.vns.txt", temp_root / "templates" / "minimal-vn" / "assets" / "scripts" / "S2.vns.txt")
        copy_file(ROOT / "templates" / "minimal-vn" / "assets" / "scripts" / "S3.vns.txt", temp_root / "templates" / "minimal-vn" / "assets" / "scripts" / "S3.vns.txt")
        copy_file(ROOT / "templates" / "minimal-vn" / "assets" / "scripts" / "S10.vns.txt", temp_root / "templates" / "minimal-vn" / "assets" / "scripts" / "S10.vns.txt")
        copy_file(ROOT / "templates" / "minimal-vn" / "assets" / "images" / "images.json", temp_root / "templates" / "minimal-vn" / "assets" / "images" / "images.json")
        copy_file(ROOT / "templates" / "minimal-vn" / "tools" / "build_assets.sh", temp_root / "templates" / "minimal-vn" / "tools" / "build_assets.sh")
        copy_file(ROOT / "templates" / "host-embed" / "README.md", temp_root / "templates" / "host-embed" / "README.md")
        copy_file(ROOT / "templates" / "host-embed" / "template.json", temp_root / "templates" / "host-embed" / "template.json")
        copy_file(ROOT / "templates" / "host-embed" / "src" / "session_loop.c", temp_root / "templates" / "host-embed" / "src" / "session_loop.c")
        copy_file(ROOT / "templates" / "host-embed" / "src" / "linux_tty_loop.c", temp_root / "templates" / "host-embed" / "src" / "linux_tty_loop.c")
        copy_file(ROOT / "templates" / "host-embed" / "src" / "windows_console_loop.c", temp_root / "templates" / "host-embed" / "src" / "windows_console_loop.c")
        copy_file(ROOT / "tests" / "integration" / "test_templates_layout.py", temp_root / "tests" / "integration" / "test_templates_layout.py")

        broken = (temp_root / "docs" / "project-layout.md").read_text(encoding="utf-8")
        broken = broken.replace("`templates/host-embed/`", "`templates/host-embed-missing/`", 1)
        (temp_root / "docs" / "project-layout.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken template contracts should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.template_contracts.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_template_contracts_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
