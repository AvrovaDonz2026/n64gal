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
        return error("tool.validate.porting_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_porting_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.porting_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        porting = read_text(root, "docs/backend-porting.md")
        backend_doc = read_text(root, "docs/api/backend.md")
        backend_header = read_text(root, "include/vn_backend.h")
        renderer_header = read_text(root, "include/vn_renderer.h")
        priority_test = read_text(root, "tests/unit/test_backend_priority.c")
        fallback_test = read_text(root, "tests/unit/test_renderer_fallback.c")
        dirty_test = read_text(root, "tests/unit/test_renderer_dirty_submit.c")
        consistency_test = read_text(root, "tests/unit/test_backend_consistency.c")
        golden_test = read_text(root, "tests/unit/test_runtime_golden.c")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
        issue = read_text(root, "issue.md")
    except FileNotFoundError as exc:
        return error("tool.validate.porting_contracts.io", VN_E_IO, str(exc), "required porting contract file missing")
    except OSError:
        return error("tool.validate.porting_contracts.io", VN_E_IO, "root", "failed reading porting contract files")

    try:
        require_contains(porting, "`include/vn_backend.h`", "porting.backend_h")
        require_contains(porting, "`include/vn_renderer.h`", "porting.renderer_h")
        require_contains(porting, "`src/core/backend_registry.c`", "porting.registry")
        require_contains(porting, "`src/core/renderer.c`", "porting.renderer")
        require_contains(porting, "`src/core/runtime_cli.c`", "porting.runtime_cli")
        require_contains(porting, "`src/core/runtime_input.c`", "porting.runtime_input")
        require_contains(porting, "`src/core/runtime_parse.c`", "porting.runtime_parse")
        require_contains(porting, "`src/tools/preview_cli.c`", "porting.preview_cli")
        require_contains(porting, "`src/tools/preview_parse.c`", "porting.preview_parse")
        require_contains(porting, "`VNRenderBackend`", "porting.backend_table")
        require_contains(porting, "`test_renderer_fallback`", "porting.fallback_test")
        require_contains(porting, "`test_renderer_dirty_submit`", "porting.dirty_test")
        require_contains(porting, "`test_backend_consistency`", "porting.consistency_test")
        require_contains(porting, "`test_runtime_golden`", "porting.golden_test")
        require_contains(porting, "`avx2_asm`", "porting.avx2_asm")
        require_contains(porting, "./scripts/check_c89.sh", "porting.check_c89")
        require_contains(porting, "./scripts/ci/run_cc_suite.sh", "porting.cc_suite")
        require_contains(porting, "./tests/perf/run_kernel_bench.sh --backend <backend>", "porting.kernel_bench")
        require_contains(porting, "./tests/perf/run_perf_compare.sh --baseline scalar --candidate <backend>", "porting.perf_compare")

        require_contains(backend_doc, "`avx2_asm` 当前不属于默认优先级链", "backend_doc.avx2_asm")
        require_contains(backend_header, "#define VN_ARCH_AVX2_ASM 5", "backend_header.avx2_asm")
        require_contains(renderer_header, "VN_RENDERER_FLAG_FORCE_AVX2_ASM", "renderer_header.force_avx2_asm")

        require_contains(priority_test, "simd auto must not select force-only avx2_asm", "priority_test.avx2_asm")
        require_contains(fallback_test, 'expect_backend(VN_RENDERER_FLAG_FORCE_AVX2_ASM, "avx2_asm")', "fallback_test.avx2_asm")
        require_contains(dirty_test, 'strcmp(backend_name, "avx2") == 0 || strcmp(backend_name, "avx2_asm") == 0', "dirty_test.avx2_asm")
        require_contains(consistency_test, '"avx2_asm"', "consistency_test.avx2_asm")
        require_contains(golden_test, '"avx2_asm"', "golden_test.avx2_asm")

        require_contains(readme, "docs/backend-porting.md", "readme.porting_doc")
        require_contains(toolchain, "python3 tools/toolchain.py validate-porting-contracts", "toolchain.validate_porting")
        require_contains(issue, "`docs/backend-porting.md`", "issue.porting_doc")
    except ValueError as exc:
        return error("tool.validate.porting_contracts.format", VN_E_FORMAT, str(exc), "porting contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.porting_contracts.ok",
                f"root={root}",
                "porting_doc=present",
                "backend_contract=present",
                "tests=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
