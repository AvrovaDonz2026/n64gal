#!/usr/bin/env python3
from pathlib import Path
import json
import os
import sys
import tempfile
import urllib.request


VN_E_INVALID_ARG = -1
VN_E_IO = -2
VN_E_FORMAT = -3


def error(trace_id, error_code, field, message):
    error_name = {
        VN_E_INVALID_ARG: "VN_E_INVALID_ARG",
        VN_E_IO: "VN_E_IO",
        VN_E_FORMAT: "VN_E_FORMAT",
    }.get(error_code, "VN_E_UNKNOWN")
    parts = [f"trace_id={trace_id}", f"error_code={error_code}", f"error_name={error_name}"]
    if field:
        parts.append(f"field={field}")
    parts.append(f"message={message}")
    print(" ".join(parts), file=sys.stderr)
    return 1


def read_json(path: Path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def main(argv):
    release_spec = Path("docs/release-publish-v0.1.0-alpha.json")
    release_json = None
    release_json_url = ""
    github_repo = ""
    tag_name = ""
    api_root = "https://api.github.com"
    token_env = "GITHUB_TOKEN"
    i = 1
    while i < len(argv):
        arg = argv[i]
        if arg in ("-h", "--help"):
            print("usage: validate_release_remote_state.py (--release-json <path> | --release-json-url <url> | --github-repo <owner/repo>) [--tag <tag>] [--api-root <url>] [--token-env <env>] [--release-spec <path>]", file=sys.stderr)
            return 2
        if arg == "--release-spec":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_remote_state.usage", VN_E_INVALID_ARG, "release_spec", "missing value")
            release_spec = Path(argv[i])
            i += 1
            continue
        if arg == "--release-json":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_remote_state.usage", VN_E_INVALID_ARG, "release_json", "missing value")
            release_json = Path(argv[i])
            i += 1
            continue
        if arg == "--release-json-url":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_remote_state.usage", VN_E_INVALID_ARG, "release_json_url", "missing value")
            release_json_url = argv[i]
            i += 1
            continue
        if arg == "--github-repo":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_remote_state.usage", VN_E_INVALID_ARG, "github_repo", "missing value")
            github_repo = argv[i]
            i += 1
            continue
        if arg == "--tag":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_remote_state.usage", VN_E_INVALID_ARG, "tag", "missing value")
            tag_name = argv[i]
            i += 1
            continue
        if arg == "--api-root":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_remote_state.usage", VN_E_INVALID_ARG, "api_root", "missing value")
            api_root = argv[i]
            i += 1
            continue
        if arg == "--token-env":
            i += 1
            if i >= len(argv):
                return error("tool.validate.release_remote_state.usage", VN_E_INVALID_ARG, "token_env", "missing value")
            token_env = argv[i]
            i += 1
            continue
        return error("tool.validate.release_remote_state.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")

    if not release_spec.exists():
        return error("tool.validate.release_remote_state.io", VN_E_IO, str(release_spec), "release spec not found")

    try:
        spec = read_json(release_spec)
    except OSError:
        return error("tool.validate.release_remote_state.io", VN_E_IO, "release_spec", "failed reading release spec")
    except ValueError:
        return error("tool.validate.release_remote_state.format", VN_E_FORMAT, "release_spec", "invalid release spec json")

    if not tag_name:
        tag_name = spec.get("tag", "")

    mode_count = 0
    if release_json is not None:
        mode_count += 1
    if release_json_url:
        mode_count += 1
    if github_repo:
        mode_count += 1
    if mode_count != 1:
        return error("tool.validate.release_remote_state.usage", VN_E_INVALID_ARG, "release_json", "provide exactly one remote json source")

    if github_repo and release_json is None and not release_json_url:
        pass
    elif not github_repo and release_json is None and not release_json_url:
        github_repo = spec.get("repository", "")

    temp_path = None
    if release_json_url or github_repo:
        try:
            with tempfile.NamedTemporaryFile(prefix="n64gal_release_remote_", suffix=".json", delete=False) as handle:
                temp_path = Path(handle.name)
            if github_repo:
                release_json_url = f"{api_root}/repos/{github_repo}/releases/tags/{tag_name}"
            req = urllib.request.Request(release_json_url, headers={"Accept": "application/vnd.github+json"})
            token_value = os.environ.get(token_env, "")
            if token_value:
                req.add_header("Authorization", f"Bearer {token_value}")
            with urllib.request.urlopen(req) as response:
                temp_path.write_bytes(response.read())
            release_json = temp_path
        except OSError:
            return error("tool.validate.release_remote_state.io", VN_E_IO, "release_json", "failed writing fetched release json")
        except Exception:
            return error("tool.validate.release_remote_state.io", VN_E_IO, "release_json", "failed fetching remote release json")

    if release_json is None or not release_json.exists():
        return error("tool.validate.release_remote_state.io", VN_E_IO, str(release_json), "release json not found")

    try:
        remote = read_json(release_json)
    except OSError:
        return error("tool.validate.release_remote_state.io", VN_E_IO, "json", "failed reading release json")
    except ValueError:
        return error("tool.validate.release_remote_state.format", VN_E_FORMAT, "json", "invalid json payload")

    tag = spec.get("tag")
    release_url = spec.get("release_url")
    expect_draft = spec.get("draft", False)
    expect_prerelease = spec.get("prerelease", True)
    asset = spec.get("asset", {})
    asset_path = asset.get("path", "")
    asset_name = asset.get("name", Path(asset_path).name)

    if remote.get("tag_name") != tag:
        return error("tool.validate.release_remote_state.format", VN_E_FORMAT, "tag_name", "remote tag mismatch")
    if remote.get("html_url") != release_url:
        return error("tool.validate.release_remote_state.format", VN_E_FORMAT, "html_url", "remote release url mismatch")
    if bool(remote.get("draft")) != bool(expect_draft):
        return error("tool.validate.release_remote_state.format", VN_E_FORMAT, "draft", "remote release must not be draft")
    if bool(remote.get("prerelease")) != bool(expect_prerelease):
        return error("tool.validate.release_remote_state.format", VN_E_FORMAT, "prerelease", "remote prerelease flag mismatch")

    assets = remote.get("assets")
    if not isinstance(assets, list) or not assets:
        return error("tool.validate.release_remote_state.format", VN_E_FORMAT, "assets", "remote assets missing")

    matched_asset = None
    for entry in assets:
        if isinstance(entry, dict) and entry.get("name") == asset_name:
            matched_asset = entry
            break
    if matched_asset is None:
        return error("tool.validate.release_remote_state.format", VN_E_FORMAT, "assets.demo", "expected demo asset missing")
    if int(matched_asset.get("size", 0)) <= 0:
        return error("tool.validate.release_remote_state.format", VN_E_FORMAT, "assets.demo.size", "demo asset size invalid")
    if f"/download/{tag}/{asset_name}" not in matched_asset.get("browser_download_url", ""):
        return error("tool.validate.release_remote_state.format", VN_E_FORMAT, "assets.demo.url", "demo asset download url mismatch")

    print(
        " ".join(
            [
                "trace_id=tool.validate.release_remote_state.ok",
                f"release_spec={release_spec}",
                f"release_json={release_json}",
                f"release_json_url={release_json_url if release_json_url else 'n/a'}",
                f"tag={tag}",
                f"asset={asset_name}",
            ]
        )
    )
    if temp_path is not None:
        try:
            temp_path.unlink()
        except OSError:
            pass
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
