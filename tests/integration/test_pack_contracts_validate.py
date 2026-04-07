#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_pack_contracts.py"]


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
        print(f"pack_contracts current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.pack_contracts.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_pack_contracts_") as temp_dir:
        temp_root = Path(temp_dir)
        (temp_root / "docs" / "api").mkdir(parents=True, exist_ok=True)
        (temp_root / "include").mkdir(parents=True, exist_ok=True)
        (temp_root / "src" / "core").mkdir(parents=True, exist_ok=True)
        (temp_root / "tests" / "unit").mkdir(parents=True, exist_ok=True)

        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        shutil.copy2(ROOT / "docs" / "toolchain.md", temp_root / "docs" / "toolchain.md")
        shutil.copy2(ROOT / "docs" / "api" / "pack.md", temp_root / "docs" / "api" / "pack.md")
        shutil.copy2(ROOT / "include" / "vn_pack.h", temp_root / "include" / "vn_pack.h")
        shutil.copy2(ROOT / "src" / "core" / "runtime_cli.c", temp_root / "src" / "core" / "runtime_cli.c")
        shutil.copy2(ROOT / "src" / "core" / "runtime_session_support.c", temp_root / "src" / "core" / "runtime_session_support.c")
        shutil.copy2(ROOT / "tests" / "unit" / "test_vnpak.c", temp_root / "tests" / "unit" / "test_vnpak.c")

        broken = (temp_root / "docs" / "api" / "pack.md").read_text(encoding="utf-8")
        broken = broken.replace("当前运行时兼容读取 `v1/v2`。", "当前运行时兼容读取 `v2 only`。", 1)
        (temp_root / "docs" / "api" / "pack.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken pack contracts should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.pack_contracts.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_pack_contracts_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
