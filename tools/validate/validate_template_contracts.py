#!/usr/bin/env python3
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


def require_file(root: Path, rel: str):
    if not (root / rel).exists():
        raise FileNotFoundError(rel)


def main(argv):
    root = Path(".")
    if len(argv) > 2:
        return error("tool.validate.template_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_template_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.template_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        gitignore = read_text(root, ".gitignore")
        readme = read_text(root, "README.md")
        project_layout = read_text(root, "docs/project-layout.md")
        minimal_readme = read_text(root, "templates/minimal-vn/README.md")
        host_readme = read_text(root, "templates/host-embed/README.md")
        minimal_meta = read_text(root, "templates/minimal-vn/template.json")
        host_meta = read_text(root, "templates/host-embed/template.json")
        templates_test = read_text(root, "tests/integration/test_templates_layout.py")

        require_file(root, "templates/minimal-vn/assets/scripts/S0.vns.txt")
        require_file(root, "templates/minimal-vn/assets/scripts/S1.vns.txt")
        require_file(root, "templates/minimal-vn/assets/scripts/S2.vns.txt")
        require_file(root, "templates/minimal-vn/assets/scripts/S3.vns.txt")
        require_file(root, "templates/minimal-vn/assets/scripts/S10.vns.txt")
        require_file(root, "templates/minimal-vn/assets/images/images.json")
        require_file(root, "templates/minimal-vn/tools/build_assets.sh")
        require_file(root, "templates/host-embed/src/session_loop.c")
        require_file(root, "templates/host-embed/src/linux_tty_loop.c")
        require_file(root, "templates/host-embed/src/windows_console_loop.c")
    except FileNotFoundError as exc:
        return error("tool.validate.template_contracts.io", VN_E_IO, str(exc), "required template file missing")
    except OSError:
        return error("tool.validate.template_contracts.io", VN_E_IO, "root", "failed reading template files")

    try:
        require_contains(readme, "`templates/minimal-vn/`", "readme.templates.minimal")
        require_contains(readme, "`templates/host-embed/`", "readme.templates.host")
        require_contains(readme, "docs/project-layout.md", "readme.project_layout")
        require_contains(gitignore, "templates/minimal-vn/assets/scripts/*.vns.bin", "gitignore.template_legacy_bins")
        require_contains(gitignore, "templates/minimal-vn/build/", "gitignore.template_build")

        require_contains(project_layout, "`templates/minimal-vn/`", "layout.minimal")
        require_contains(project_layout, "`templates/host-embed/`", "layout.host")
        require_contains(project_layout, "`assets/scripts/*.vns.txt`", "layout.scripts")
        require_contains(project_layout, "`build/`", "layout.build")
        require_contains(project_layout, "`build/scripts/`", "layout.build_scripts")

        require_contains(minimal_readme, "./templates/minimal-vn/tools/build_assets.sh", "minimal.readme.build_assets")
        require_contains(minimal_readme, "templates/minimal-vn/build/minimal.vnpak", "minimal.readme.pack_output")
        require_contains(minimal_readme, "templates/minimal-vn/build/scripts/*.vns.bin", "minimal.readme.script_output")
        require_contains(minimal_meta, "\"template_version\": 1", "minimal.meta.version")
        require_contains(minimal_meta, "\"template_name\": \"minimal-vn\"", "minimal.meta.name")

        require_contains(host_readme, "./templates/minimal-vn/tools/build_assets.sh", "host.readme.depends_minimal")
        require_contains(host_readme, "templates/host-embed/src/session_loop.c", "host.readme.session_loop")
        require_contains(host_readme, "src/core/platform.c", "host.readme.platform")
        require_contains(host_readme, "src/core/dynamic_resolution.c", "host.readme.dynamic_resolution")
        require_contains(host_readme, "src/frontend/dirty_tiles.c", "host.readme.dirty_tiles")
        require_contains(host_readme, "src/backend/avx2/avx2_fill_fade.c", "host.readme.avx2_fill_fade")
        require_contains(host_readme, "src/backend/avx2/avx2_textured.c", "host.readme.avx2_textured")
        require_contains(host_meta, "\"template_version\": 1", "host.meta.version")
        require_contains(host_meta, "\"template_name\": \"host-embed\"", "host.meta.name")

        require_contains(templates_test, '"minimal-vn"', "templates_test.minimal")
        require_contains(templates_test, '"host-embed"', "templates_test.host")
        require_contains(templates_test, "build_assets.sh", "templates_test.build_assets")
        require_contains(templates_test, "minimal.vnpak", "templates_test.pack_output")
        require_contains(templates_test, 'compiled_scripts = minimal / "build" / "scripts"', "templates_test.script_output")
    except ValueError as exc:
        return error("tool.validate.template_contracts.format", VN_E_FORMAT, str(exc), "template contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.template_contracts.ok",
                f"root={root}",
                "minimal_template=present",
                "host_template=present",
                "layout_doc=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
