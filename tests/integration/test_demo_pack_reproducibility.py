#!/usr/bin/env python3
import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def fail(message):
    print(f"test_demo_pack_reproducibility: {message}", file=sys.stderr)
    return 1


def main():
    tracked_paths = [
        ROOT / "assets/demo/demo.vnpak",
        ROOT / "assets/demo/content-demo.vnpak",
        ROOT / "assets/demo/manifest.json",
        *(
            ROOT / "assets/demo/scripts" / name
            for name in (
                "S0.vns.bin",
                "S1.vns.bin",
                "S2.vns.bin",
                "S3.vns.bin",
                "S10.vns.bin",
            )
        ),
    ]
    before = {path: path.read_bytes() for path in tracked_paths}
    proc = subprocess.run(
        ["./tools/packer/make_demo_pack.sh"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        return fail(f"packer wrapper failed rc={proc.returncode} stderr={proc.stderr}")
    changed = [
        path.relative_to(ROOT).as_posix()
        for path in tracked_paths
        if path.read_bytes() != before[path]
    ]
    if changed:
        return fail(f"packer wrapper changed tracked outputs: {','.join(changed)}")

    manifest = json.loads(
        (ROOT / "assets/demo/manifest.json").read_text(encoding="utf-8")
    )
    sources = [item.get("source", "") for item in manifest.get("resources", [])]
    if not sources or any(not source or Path(source).is_absolute() for source in sources):
        return fail("manifest resource sources must be non-empty relative paths")
    if any(source == ".." or source.startswith("../") for source in sources):
        return fail("manifest resource sources must stay inside the demo root")
    demo_root = ROOT / "assets/demo"
    if any(not (demo_root / source).is_file() for source in sources):
        return fail("manifest resource sources must resolve inside the demo root")

    print("test_demo_pack_reproducibility ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
