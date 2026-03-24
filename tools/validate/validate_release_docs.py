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


def main(argv):
    root = Path(".")
    if len(argv) > 2:
        return error("tool.validate.release_docs.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_release_docs.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.release_docs.io", VN_E_IO, "root", "root directory not found")

    try:
        readme = read_text(root, "README.md")
        issue = read_text(root, "issue.md")
        changelog = read_text(root, "CHANGELOG.md")
        release_note = read_text(root, "docs/release-v0.1.0-alpha.md")
        release_evidence = read_text(root, "docs/release-evidence-v0.1.0-alpha.md")
        release_package = read_text(root, "docs/release-package-v0.1.0-alpha.md")
        alpha_checklist = read_text(root, "docs/release-checklist-v0.1.0-alpha.md")
        mvp_gap = read_text(root, "docs/release-gap-v0.1.0-mvp.md")
        roadmap = read_text(root, "docs/release-roadmap-1.0.0.md")
        checklist_v1 = read_text(root, "docs/release-checklist-v1.0.0.md")
        release_publish_spec = read_text(root, "docs/release-publish-v0.1.0-alpha.json")
    except FileNotFoundError as exc:
        return error("tool.validate.release_docs.io", VN_E_IO, str(exc), "required release document missing")
    except OSError:
        return error("tool.validate.release_docs.io", VN_E_IO, "root", "failed reading release docs")

    try:
        require_contains(readme, "当前对外版本状态：`v0.1.0-alpha` 已发布", "readme.alpha_published")
        require_contains(readme, "当前后续目标：`v0.1.0-mvp`", "readme.mvp_target")
        require_contains(readme, "docs/release-v0.1.0-alpha.md", "readme.release_note_link")
        require_contains(readme, "docs/release-evidence-v0.1.0-alpha.md", "readme.release_evidence_link")
        require_contains(readme, "docs/release-gap-v0.1.0-mvp.md", "readme.mvp_gap_link")
        require_contains(readme, "docs/release-checklist-v1.0.0.md", "readme.v1_checklist_link")

        require_contains(issue, "`v0.1.0-alpha` 已发布", "issue.alpha_published")
        require_contains(issue, "`v0.1.0-mvp`", "issue.mvp_target")
        require_contains(issue, "`v1.0.0` 当前明确先不包含 `RVV/riscv64 native` 承诺。", "issue.v1_boundary")

        require_contains(changelog, "## v0.1.0-alpha", "changelog.alpha_section")
        require_contains(changelog, "`rvv` 最小可运行后端与 `qemu-first` 验证链", "changelog.rvv")
        require_contains(changelog, "`avx2_asm`", "changelog.avx2_asm")
        require_contains(changelog, "`JIT`", "changelog.jit")
        require_contains(changelog, "`vnsave` 迁移不在当前版本范围", "changelog.vnsave_boundary")

        require_contains(release_note, "`v0.1.0-alpha`", "release_note.alpha")
        require_contains(release_note, "`avx2_asm` 自动选择；它仍是 force-only 实验后端。", "release_note.avx2_asm")
        require_contains(release_note, "`JIT`；当前仍是文档化实验方向，不是 release blocker。", "release_note.jit")
        require_contains(release_note, "`riscv64 Linux`：`rvv -> scalar`，当前以 `cross-build + qemu` 为主", "release_note.rvv")

        require_contains(release_evidence, "https://github.com/AvrovaDonz2026/n64gal/releases/tag/v0.1.0-alpha", "release_evidence.link")
        require_contains(release_evidence, "`demo.vnpak`", "release_evidence.asset")
        require_contains(release_evidence, "`./scripts/check_c89.sh`", "release_evidence.check_c89")
        require_contains(release_evidence, "`./scripts/ci/run_cc_suite.sh`", "release_evidence.cc_suite")
        require_contains(release_evidence, "`S0`", "release_evidence.s0")
        require_contains(release_evidence, "`S10`", "release_evidence.s10")

        require_contains(release_package, "`assets/demo/demo.vnpak`", "release_package.demo_pack")
        require_contains(release_package, "`docs/release-v0.1.0-alpha.md`", "release_package.release_note")
        require_contains(release_package, "`docs/release-evidence-v0.1.0-alpha.md`", "release_package.release_evidence")
        require_contains(release_package, "`docs/backend-porting.md`", "release_package.backend_porting")
        require_contains(release_package, "`docs/migration.md`", "release_package.migration")
        require_contains(release_package, "`release_publish_map(.md/.json)`", "release_package.publish_map")

        require_contains(alpha_checklist, "`v0.1.0-alpha` 不是 ABI/格式冻结版本", "alpha_checklist.not_frozen")
        require_contains(alpha_checklist, "`riscv64 native` 不在当前发布级承诺范围", "alpha_checklist.rvv_boundary")
        require_contains(alpha_checklist, "`JIT` 不在当前发布范围", "alpha_checklist.jit")

        require_contains(mvp_gap, "`v0.1.0-alpha` GitHub prerelease 已发布", "mvp_gap.alpha_released")
        require_contains(mvp_gap, "`v1.0.0` 范围已明确排除 `RVV/riscv64 native`", "mvp_gap.rvv_boundary")

        require_contains(roadmap, "`v1.0.0` **先不包含 RVV / riscv64 native 承诺**", "roadmap.rvv_boundary")
        require_contains(checklist_v1, "`RVV/riscv64 native` 转入 `post-1.0`", "checklist_v1.rvv_boundary")
        require_contains(release_publish_spec, "\"tag\": \"v0.1.0-alpha\"", "release_publish_spec.tag")
        require_contains(release_publish_spec, "\"release_note\": \"docs/release-v0.1.0-alpha.md\"", "release_publish_spec.release_note")
        require_contains(release_publish_spec, "\"path\": \"assets/demo/demo.vnpak\"", "release_publish_spec.asset")
    except ValueError as exc:
        return error("tool.validate.release_docs.format", VN_E_FORMAT, str(exc), "release document drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.release_docs.ok",
                f"root={root}",
                "alpha_release_docs=present",
                "mvp_gap=present",
                "v1_boundary=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
