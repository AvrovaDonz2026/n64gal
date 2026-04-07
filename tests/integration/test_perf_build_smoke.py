#!/usr/bin/env python3
import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()


def main():
    with tempfile.TemporaryDirectory(prefix="n64gal_perf_build_") as temp_dir:
        runner_bin = Path(temp_dir) / "vn_perf_runner"
        env = os.environ.copy()
        env["VN_PERF_CFLAGS"] = "-O2 -DNDEBUG"
        proc = subprocess.run(
            [
                "bash",
                "tests/perf/run_perf.sh",
                "--backend",
                "scalar",
                "--build-only",
                "--runner-bin",
                str(runner_bin),
            ],
            cwd=ROOT,
            env=env,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            print(
                f"perf build-only smoke failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}",
                file=sys.stderr,
            )
            return 1
        if not runner_bin.exists():
            print("perf build-only smoke did not produce runner binary", file=sys.stderr)
            return 1

    print("test_perf_build_smoke ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
