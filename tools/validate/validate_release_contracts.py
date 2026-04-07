#!/usr/bin/env python3
import argparse
from pathlib import Path
import sys


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


def read_text(root: Path, rel: str):
    path = root / rel
    if not path.exists():
        raise FileNotFoundError(rel)
    return path.read_text(encoding="utf-8")


def require_contains(text: str, needle: str, field: str):
    if needle not in text:
        raise ValueError(field)


def main(argv):
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--root", default=".")
    parser.add_argument("--help", action="store_true")
    args, extra = parser.parse_known_args(argv[1:])
    if args.help:
        print("usage: validate_release_contracts.py [--root DIR]", file=sys.stderr)
        return 2
    if extra:
        return error("tool.validate.release_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")

    root = Path(args.root).resolve()
    if not root.exists():
        return error("tool.validate.release_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        readme = read_text(root, "README.md")
        roadmap = read_text(root, "docs/release-roadmap-1.0.0.md")
        compat = read_text(root, "docs/compat-matrix.md")
        host_sdk = read_text(root, "docs/host-sdk.md")
        migration = read_text(root, "docs/migration.md")
        manifest = read_text(root, "docs/extension-manifest.md")
        governance = read_text(root, "docs/ecosystem-governance.md")
        toolchain = read_text(root, "docs/toolchain.md")
    except FileNotFoundError as exc:
        return error("tool.validate.release_contracts.io", VN_E_IO, str(exc), "required contract doc missing")
    except OSError:
        return error("tool.validate.release_contracts.io", VN_E_IO, "root", "failed reading contract docs")

    try:
        require_contains(readme, "先不包含 RVV / riscv64 native 承诺", "README.rvv_scope")
        require_contains(readme, "首个 `v1.0.0` 正式版当前只承诺前四项", "README.platform_scope")

        require_contains(roadmap, "先不包含 RVV / riscv64 native 承诺", "roadmap.rvv_scope")
        require_contains(roadmap, "`riscv64/RVV` 转入 `post-1.0` 轨道", "roadmap.post_1_0")

        require_contains(compat, "| `v1.0.0` 首版承诺 | 是 | 是 | 是 | 是 | 否 |", "compat.platform_matrix")
        require_contains(compat, "| `tool manifest` | `planned v1` |", "compat.tool_manifest")

        require_contains(host_sdk, "| `runtime api` | `public v1-draft (pre-1.0)` |", "host_sdk.runtime_api")
        require_contains(host_sdk, "| `vnsave` | `pre-1.0 unstable` |", "host_sdk.vnsave")

        require_contains(migration, "首个正式 `vnsave v1` 承诺不早于 `v1.0.0`", "migration.vnsave_scope")
        require_contains(migration, "当前已有最小 `v0 -> v1` 迁移命令", "migration.vnsave_migrate")
        require_contains(migration, "runtime session save/load", "migration.runtime_session_only")

        require_contains(manifest, "| `manifest_version` | yes |", "manifest.manifest_version")
        require_contains(manifest, "| `entry` | yes |", "manifest.entry")
        require_contains(manifest, "| `stability` | yes |", "manifest.stability")

        require_contains(governance, "先文件级扩展，后插件 ABI", "governance.file_first")
        require_contains(governance, "兼容矩阵先于第三方接入", "governance.compat_first")

        require_contains(toolchain, "validate-manifest", "toolchain.validate")
        require_contains(toolchain, "probe-perf-compare", "toolchain.probe_perf_compare")
    except ValueError as exc:
        return error("tool.validate.release_contracts.format", VN_E_FORMAT, str(exc), "release contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.release_contracts.ok",
                f"root={root}",
                "rvv_scope=post-1.0",
                "tool_manifest=v1-planned",
                "vnsave_scope=pre-1.0->v1.0.0",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
