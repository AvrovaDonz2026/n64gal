#!/usr/bin/env python3
import subprocess
import sys


ROOT = "."
TOOL = ["python3", "tools/validate/validate_manifest.py"]


def run_case(path):
    proc = subprocess.run(
        TOOL + [path],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def main():
    rc, out, err = run_case("tests/fixtures/tool_manifest/valid/vnsave_migrate.json")
    if rc != 0:
        print(f"valid manifest failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.manifest.ok" not in out:
        print("missing success trace_id", file=sys.stderr)
        return 1

    rc, out, err = run_case("tests/fixtures/tool_manifest/invalid/missing_api_range_key.json")
    if rc == 0:
        print("missing_api_range_key should fail", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.manifest.format" not in err:
        print("missing format trace_id for missing_api_range_key", file=sys.stderr)
        return 1

    rc, out, err = run_case("tests/fixtures/tool_manifest/invalid/bad_kind.json")
    if rc == 0:
        print("bad_kind should fail", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.manifest.format" not in err:
        print("missing format trace_id for bad_kind", file=sys.stderr)
        return 1

    print("test_manifest_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
