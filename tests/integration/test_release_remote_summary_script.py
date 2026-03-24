#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path
from socketserver import TCPServer
from threading import Thread
from http.server import SimpleHTTPRequestHandler


ROOT = Path(".").resolve()
SCRIPT = ["bash", "scripts/release/run_release_remote_summary.sh"]


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        return


def main():
    with tempfile.TemporaryDirectory(prefix="n64gal_release_remote_summary_") as temp_dir:
        out_dir = Path(temp_dir) / "remote"
        fixture = ROOT / "tests" / "fixtures" / "release_api" / "github_release_v0.1.0-alpha.json"
        proc = subprocess.run(
            SCRIPT + ["--release-json", str(fixture), "--out-dir", str(out_dir)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            print(f"release remote summary failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
            return 1
        if "trace_id=release.remote_summary.ok" not in proc.stdout:
            print("missing release remote summary success trace", file=sys.stderr)
            return 1
        if not (out_dir / "release_remote_summary.md").exists() or not (out_dir / "release_remote_summary.json").exists():
            print("release remote summary outputs missing", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_release_remote_http_") as temp_dir:
        temp_root = Path(temp_dir)
        served = temp_root / "github_release_v0.1.0-alpha.json"
        served.write_text(fixture.read_text(encoding="utf-8"), encoding="utf-8")
        api_served = temp_root / "repos" / "AvrovaDonz2026" / "n64gal" / "releases" / "tags" / "v0.1.0-alpha"
        api_served.parent.mkdir(parents=True, exist_ok=True)
        api_served.write_text(fixture.read_text(encoding="utf-8"), encoding="utf-8")

        old_cwd = Path.cwd()
        try:
            import os
            os.chdir(temp_root)
            server = TCPServer(("127.0.0.1", 0), QuietHandler)
            thread = Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                url = f"http://127.0.0.1:{server.server_address[1]}/github_release_v0.1.0-alpha.json"
                out_dir = temp_root / "remote_url"
                proc = subprocess.run(
                    SCRIPT + ["--release-json-url", url, "--out-dir", str(out_dir)],
                    cwd=ROOT,
                    capture_output=True,
                    text=True,
                )
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)
        finally:
            os.chdir(old_cwd)

        if proc.returncode != 0:
            print(f"release remote summary url failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
            return 1
        if "trace_id=release.remote_summary.ok" not in proc.stdout:
            print("missing release remote summary url success trace", file=sys.stderr)
            return 1
        if not (out_dir / "release_remote_state.json").exists():
            print("release remote summary url missing fetched json", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="n64gal_release_remote_api_") as temp_dir:
        temp_root = Path(temp_dir)
        api_served = temp_root / "repos" / "AvrovaDonz2026" / "n64gal" / "releases" / "tags" / "v0.1.0-alpha"
        api_served.parent.mkdir(parents=True, exist_ok=True)
        api_served.write_text(fixture.read_text(encoding="utf-8"), encoding="utf-8")

        old_cwd = Path.cwd()
        try:
            import os
            os.chdir(temp_root)
            server = TCPServer(("127.0.0.1", 0), QuietHandler)
            thread = Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                api_root = f"http://127.0.0.1:{server.server_address[1]}"
                out_dir = temp_root / "remote_api"
                proc = subprocess.run(
                    SCRIPT + ["--github-repo", "AvrovaDonz2026/n64gal", "--tag", "v0.1.0-alpha", "--api-root", api_root, "--out-dir", str(out_dir)],
                    cwd=ROOT,
                    capture_output=True,
                    text=True,
                )
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)
        finally:
            os.chdir(old_cwd)

        if proc.returncode != 0:
            print(f"release remote summary api failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}", file=sys.stderr)
            return 1
        if "trace_id=release.remote_summary.ok" not in proc.stdout:
            print("missing release remote summary api success trace", file=sys.stderr)
            return 1
        if not (out_dir / "release_remote_state.json").exists():
            print("release remote summary api missing fetched json", file=sys.stderr)
            return 1

    print("test_release_remote_summary_script ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
