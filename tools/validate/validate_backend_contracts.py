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


def require_define(text: str, name: str, value: str, field: str):
    expected = ["#define", name, value]
    if not any(line.split() == expected for line in text.splitlines()):
        raise ValueError(field)


def main(argv):
    root = Path(".")
    if len(argv) > 2:
        return error("tool.validate.backend_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_backend_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.backend_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        backend_doc = read_text(root, "docs/api/backend.md")
        backend_header = read_text(root, "include/vn_backend.h")
        pixel_pipeline_header = read_text(root, "src/backend/common/pixel_pipeline.h")
        pixel_pipeline = read_text(root, "src/backend/common/pixel_pipeline.c")
        builtin_backends = {
            "scalar": read_text(root, "src/backend/scalar/scalar_backend.c"),
            "avx2": read_text(root, "src/backend/avx2/avx2_backend.c"),
            "neon": read_text(root, "src/backend/neon/neon_backend.c"),
            "rvv": read_text(root, "src/backend/rvv/rvv_backend.c"),
        }
        registry = read_text(root, "src/core/backend_registry.c")
        priority_test = read_text(root, "tests/unit/test_backend_priority.c")
        fallback_test = read_text(root, "tests/unit/test_renderer_fallback.c")
        resource_texture_test = read_text(root, "tests/unit/test_resource_texture_backend.c")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
    except FileNotFoundError as exc:
        return error("tool.validate.backend_contracts.io", VN_E_IO, str(exc), "required backend contract file missing")
    except OSError:
        return error("tool.validate.backend_contracts.io", VN_E_IO, "root", "failed reading backend contract files")

    try:
        require_contains(backend_doc, "默认优先级：", "backend_doc.priority_header")
        require_contains(backend_doc, "1. `avx2`", "backend_doc.priority_avx2")
        require_contains(backend_doc, "2. `neon`", "backend_doc.priority_neon")
        require_contains(backend_doc, "3. `rvv`", "backend_doc.priority_rvv")
        require_contains(backend_doc, "4. `scalar`", "backend_doc.priority_scalar")
        require_contains(backend_doc, "`avx2_asm` 当前不属于默认优先级链", "backend_doc.avx2_asm_force_only")
        require_contains(backend_doc, "自动模式按 `avx2 -> neon -> rvv -> scalar` 顺序逐个尝试初始化。", "backend_doc.auto_fallback")
        require_contains(backend_doc, "强制模式下若所请求的 SIMD 后端初始化失败，也必须回退到 `scalar`。", "backend_doc.forced_fallback")
        require_contains(backend_doc, "`test_backend_priority`", "backend_doc.priority_test")
        require_contains(backend_doc, "`test_renderer_fallback`", "backend_doc.fallback_test")
        require_contains(backend_doc, "`test_resource_texture_backend`", "backend_doc.resource_texture_test")
        require_contains(backend_doc, "`VN_OP_FLAG_RESOURCE_CROSSFADE_FROM` 固定为 `0x40u`", "backend_doc.crossfade_from")
        require_contains(backend_doc, "`VN_OP_FLAG_RESOURCE_CROSSFADE_TO` 固定为 `0x20u`", "backend_doc.crossfade_to")
        require_contains(backend_doc, "premultiplied alpha 空间", "backend_doc.crossfade_premultiplied")
        require_contains(backend_doc, "malformed pair", "backend_doc.crossfade_malformed")
        require_contains(backend_doc, "`vn_pp_draw_resource_crossfade(...)`", "backend_doc.crossfade_helper")

        require_contains(backend_header, "#define VN_ARCH_AVX2   2", "backend_header.arch_avx2")
        require_contains(backend_header, "#define VN_ARCH_NEON   3", "backend_header.arch_neon")
        require_contains(backend_header, "#define VN_ARCH_RVV    4", "backend_header.arch_rvv")
        require_contains(backend_header, "#define VN_ARCH_AVX2_ASM 5", "backend_header.arch_avx2_asm")
        require_contains(backend_header, "VN_ARCH_MASK_ALL      (VN_ARCH_MASK_SCALAR | VN_ARCH_MASK_AVX2 | VN_ARCH_MASK_NEON | VN_ARCH_MASK_RVV)", "backend_header.mask_all")
        require_contains(backend_header, "int vn_backend_register(const VNRenderBackend* be);", "backend_header.register_api")
        require_contains(backend_header, "const VNRenderBackend* vn_backend_select(vn_u32 prefer_arch_mask);", "backend_header.select_api")
        require_contains(backend_header, "#define VN_OP_FLAG_RESOURCE_TEXTURE 0x80u", "backend_header.resource_texture_flag")
        require_define(backend_header, "VN_OP_FLAG_RESOURCE_CROSSFADE_FROM", "0x40u", "backend_header.crossfade_from_flag")
        require_define(backend_header, "VN_OP_FLAG_RESOURCE_CROSSFADE_TO", "0x20u", "backend_header.crossfade_to_flag")
        require_contains(backend_header, "} VNTextureView;", "backend_header.texture_view")
        require_contains(backend_header, "int (*get_framebuffer)(const vn_u32** out_pixels,", "backend_header.framebuffer_callback")

        require_contains(pixel_pipeline_header, "int vn_pp_draw_resource_crossfade(", "pixel_pipeline_header.crossfade")
        require_contains(pixel_pipeline, "int vn_pp_draw_resource_crossfade(", "pixel_pipeline.crossfade")
        require_contains(pixel_pipeline, "VN_OP_FLAG_RESOURCE_CROSSFADE_FROM", "pixel_pipeline.crossfade_from")
        require_contains(pixel_pipeline, "VN_OP_FLAG_RESOURCE_CROSSFADE_TO", "pixel_pipeline.crossfade_to")
        require_contains(pixel_pipeline, "VN_E_FORMAT", "pixel_pipeline.crossfade_format_error")
        for backend_name, backend_source in builtin_backends.items():
            require_contains(backend_source, "vn_pp_draw_resource_crossfade", f"backend.{backend_name}.crossfade_helper")
            require_contains(backend_source, "VN_OP_FLAG_RESOURCE_CROSSFADE_FROM", f"backend.{backend_name}.crossfade_from")
            require_contains(backend_source, "VN_OP_FLAG_RESOURCE_CROSSFADE_TO", f"backend.{backend_name}.crossfade_to")

        require_contains(registry, "if ((prefer_arch_mask & VN_ARCH_MASK_AVX2_ASM) != 0u)", "registry.force_avx2_asm")
        require_contains(registry, "if (picked == (const VNRenderBackend*)0 && (prefer_arch_mask & VN_ARCH_MASK_AVX2) != 0u)", "registry.avx2")
        require_contains(registry, "if (picked == (const VNRenderBackend*)0 && (prefer_arch_mask & VN_ARCH_MASK_NEON) != 0u)", "registry.neon")
        require_contains(registry, "if (picked == (const VNRenderBackend*)0 && (prefer_arch_mask & VN_ARCH_MASK_RVV) != 0u)", "registry.rvv")
        require_contains(registry, "if (picked == (const VNRenderBackend*)0 && (prefer_arch_mask & VN_ARCH_MASK_SCALAR) != 0u)", "registry.scalar")

        require_contains(priority_test, "VN_RENDERER_FLAG_FORCE_AVX2_ASM", "priority_test.force_avx2_asm")
        require_contains(priority_test, "simd auto must not select force-only avx2_asm", "priority_test.force_only_assert")
        require_contains(fallback_test, 'expect_backend(VN_RENDERER_FLAG_FORCE_AVX2_ASM, "avx2_asm")', "fallback_test.force_avx2_asm")
        require_contains(fallback_test, 'expect_backend(VN_RENDERER_FLAG_FORCE_RVV, "rvv")', "fallback_test.force_rvv")
        require_contains(resource_texture_test, "VN_TEXTURE_FORMAT_RGBA16", "resource_texture_test.rgba16")
        require_contains(resource_texture_test, "VN_TEXTURE_FORMAT_CI8", "resource_texture_test.ci8")
        require_contains(resource_texture_test, "VN_TEXTURE_FORMAT_IA8", "resource_texture_test.ia8")
        require_contains(resource_texture_test, "VN_OP_FLAG_RESOURCE_CROSSFADE_FROM", "resource_texture_test.crossfade_from")
        require_contains(resource_texture_test, "VN_OP_FLAG_RESOURCE_CROSSFADE_TO", "resource_texture_test.crossfade_to")
        require_contains(resource_texture_test, "VN_E_FORMAT", "resource_texture_test.crossfade_malformed")

        require_contains(readme, "docs/api/backend.md", "readme.backend_doc")
        require_contains(toolchain, "python3 tools/toolchain.py validate-backend-contracts", "toolchain.validate_backend")
    except ValueError as exc:
        return error("tool.validate.backend_contracts.format", VN_E_FORMAT, str(exc), "backend contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.backend_contracts.ok",
                f"root={root}",
                "backend_api=present",
                "selection_order=present",
                "force_only_avx2_asm=present",
                "resource_crossfade=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
