#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(".").resolve()
TOOL = ["python3", "tools/validate/validate_release_remote_state.py"]

def run_case(args):
    proc = subprocess.run(TOOL + args, cwd=ROOT, capture_output=True, text=True)
    return proc.returncode, proc.stdout, proc.stderr


def main():
    fixture = "tests/fixtures/release_api/github_release_v0.1.0-alpha.json"
    alpha_spec = "docs/release-publish-v0.1.0-alpha.json"
    rc, out, err = run_case(["--release-spec", alpha_spec, "--release-json", fixture])
    if rc != 0:
        print(f"release remote validate failed rc={rc} stderr={err}", file=sys.stderr)
        return 1
    if "trace_id=tool.validate.release_remote_state.ok" not in out:
        print("missing remote validate success trace", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_release_remote_") as temp_dir:
        broken = Path(temp_dir) / "broken_release.json"
        broken.write_text(
            '{"tag_name":"v0.1.0-alpha","html_url":"https://github.com/AvrovaDonz2026/n64gal/releases/tag/v0.1.0-alpha","draft":false,"prerelease":true,"assets":[]}\n',
            encoding="utf-8",
        )
        rc, out, err = run_case(["--release-spec", alpha_spec, "--release-json", str(broken)])
        if rc == 0:
            print("broken remote release should fail", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.release_remote_state.format" not in err:
            print("missing remote format failure trace", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_release_remote_http_") as temp_dir:
        temp_root = Path(temp_dir)
        served = temp_root / "github_release_v0.1.0-alpha.json"
        served.write_text((ROOT / fixture).read_text(encoding="utf-8"), encoding="utf-8")
        url = served.resolve().as_uri()
        rc, out, err = run_case(["--release-spec", alpha_spec, "--release-json-url", url])

        if rc != 0:
            print(f"release remote validate url failed rc={rc} stderr={err}", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.release_remote_state.ok" not in out:
            print("missing remote validate url success trace", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_release_remote_api_") as temp_dir:
        temp_root = Path(temp_dir)
        api_served = temp_root / "repos" / "AvrovaDonz2026" / "n64gal" / "releases" / "tags" / "v0.1.0-alpha"
        api_served.parent.mkdir(parents=True, exist_ok=True)
        api_served.write_text((ROOT / fixture).read_text(encoding="utf-8"), encoding="utf-8")
        api_root = temp_root.resolve().as_uri()
        rc, out, err = run_case(["--release-spec", alpha_spec, "--tag", "v0.1.0-alpha", "--api-root", api_root, "--github-repo", "AvrovaDonz2026/n64gal"])

        if rc != 0:
            print(f"release remote validate api failed rc={rc} stderr={err}", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.release_remote_state.ok" not in out:
            print("missing remote validate api success trace", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_release_remote_api_spec_") as temp_dir:
        temp_root = Path(temp_dir)
        api_served = temp_root / "repos" / "AvrovaDonz2026" / "n64gal" / "releases" / "tags" / "v0.1.0-alpha"
        api_served.parent.mkdir(parents=True, exist_ok=True)
        api_served.write_text((ROOT / fixture).read_text(encoding="utf-8"), encoding="utf-8")
        api_root = temp_root.resolve().as_uri()
        rc, out, err = run_case(["--release-spec", alpha_spec, "--github-repo", "AvrovaDonz2026/n64gal", "--api-root", api_root])

        if rc != 0:
            print(f"release remote validate api spec-default failed rc={rc} stderr={err}", file=sys.stderr)
            return 1
        if "trace_id=tool.validate.release_remote_state.ok" not in out:
            print("missing remote validate api spec-default success trace", file=sys.stderr)
            return 1

    print("test_release_remote_state_validate ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
