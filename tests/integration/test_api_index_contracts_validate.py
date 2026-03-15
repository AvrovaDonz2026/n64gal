#!/usr/bin/env python3
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_api_index_contracts.py"]


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
        print(f"api_index_contracts current repo failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.api_index_contracts.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_api_index_contracts_") as temp_dir:
        temp_root = Path(temp_dir)
        (temp_root / "docs" / "api").mkdir(parents=True, exist_ok=True)

        shutil.copy2(ROOT / "README.md", temp_root / "README.md")
        shutil.copy2(ROOT / "issue.md", temp_root / "issue.md")
        shutil.copy2(ROOT / "docs" / "toolchain.md", temp_root / "docs" / "toolchain.md")
        shutil.copy2(ROOT / "docs" / "errors.md", temp_root / "docs" / "errors.md")
        shutil.copy2(ROOT / "docs" / "preview-protocol.md", temp_root / "docs" / "preview-protocol.md")
        shutil.copy2(ROOT / "docs" / "host-sdk.md", temp_root / "docs" / "host-sdk.md")
        shutil.copy2(ROOT / "docs" / "platform-matrix.md", temp_root / "docs" / "platform-matrix.md")
        shutil.copy2(ROOT / "docs" / "api" / "README.md", temp_root / "docs" / "api" / "README.md")
        shutil.copy2(ROOT / "docs" / "api" / "runtime.md", temp_root / "docs" / "api" / "runtime.md")
        shutil.copy2(ROOT / "docs" / "api" / "backend.md", temp_root / "docs" / "api" / "backend.md")
        shutil.copy2(ROOT / "docs" / "api" / "pack.md", temp_root / "docs" / "api" / "pack.md")
        shutil.copy2(ROOT / "docs" / "api" / "save.md", temp_root / "docs" / "api" / "save.md")
        shutil.copy2(ROOT / "docs" / "api" / "dirty-tile-draft.md", temp_root / "docs" / "api" / "dirty-tile-draft.md")
        shutil.copy2(ROOT / "docs" / "api" / "compat-log.md", temp_root / "docs" / "api" / "compat-log.md")

        broken = (temp_root / "docs" / "api" / "README.md").read_text(encoding="utf-8")
        broken = broken.replace("`../platform-matrix.md`", "`../platform-matrix-missing.md`", 1)
        (temp_root / "docs" / "api" / "README.md").write_text(broken, encoding="utf-8")

        rc, out, err = run_case(temp_root)
        if rc == 0:
            print("broken api index contracts should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.api_index_contracts.format" not in err:
            print("missing format failure trace_id", file=sys.stderr)
            return 1

    print("test_api_index_contracts_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
