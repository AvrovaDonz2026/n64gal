#!/usr/bin/env python3
import json
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(".").resolve()


def require_file(path: Path):
    if not path.exists():
        raise AssertionError(f"missing file: {path}")


def main():
    minimal = ROOT / "templates" / "minimal-vn"
    host = ROOT / "templates" / "host-embed"

    require_file(minimal / "README.md")
    require_file(minimal / "template.json")
    require_file(minimal / "assets" / "scripts" / "S0.vns.txt")
    require_file(minimal / "assets" / "images" / "images.json")
    require_file(minimal / "tools" / "build_assets.sh")
    require_file(host / "README.md")
    require_file(host / "template.json")
    require_file(host / "src" / "session_loop.c")
    require_file(host / "src" / "linux_tty_loop.c")
    require_file(host / "src" / "windows_console_loop.c")
    require_file(ROOT / "docs" / "project-layout.md")

    with open(minimal / "template.json", "r", encoding="utf-8") as fp:
        minimal_meta = json.load(fp)
    with open(host / "template.json", "r", encoding="utf-8") as fp:
        host_meta = json.load(fp)

    if minimal_meta.get("template_version") != 1 or host_meta.get("template_version") != 1:
        print("template_version mismatch", file=sys.stderr)
        return 1

    proc = subprocess.run(
        ["bash", str(minimal / "tools" / "build_assets.sh")],
        cwd=str(ROOT),
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f"template asset build failed stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
        return 1

    compiled_scripts = minimal / "build" / "scripts"
    require_file(compiled_scripts / "S0.vns.bin")
    require_file(compiled_scripts / "S1.vns.bin")
    require_file(compiled_scripts / "S2.vns.bin")
    require_file(compiled_scripts / "S3.vns.bin")
    require_file(compiled_scripts / "S10.vns.bin")
    require_file(minimal / "build" / "minimal.vnpak")
    require_file(minimal / "build" / "manifest.json")
    print("test_templates_layout ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
