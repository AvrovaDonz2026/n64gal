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
        return error("tool.validate.runtime_contracts.usage", VN_E_INVALID_ARG, "argv", "unexpected argument")
    if len(argv) == 2:
        if argv[1] in ("-h", "--help"):
            print("usage: validate_runtime_contracts.py [root]", file=sys.stderr)
            return 2
        root = Path(argv[1])

    if not root.exists():
        return error("tool.validate.runtime_contracts.io", VN_E_IO, "root", "root directory not found")

    try:
        runtime_doc = read_text(root, "docs/api/runtime.md")
        runtime_header = read_text(root, "include/vn_runtime.h")
        runtime_cli = read_text(root, "src/core/runtime_cli.c")
        runtime_api_test = read_text(root, "tests/unit/test_runtime_api.c")
        runtime_session_test = read_text(root, "tests/unit/test_runtime_session.c")
        runtime_cli_errors_test = read_text(root, "tests/unit/test_runtime_cli_errors.c")
        readme = read_text(root, "README.md")
        toolchain = read_text(root, "docs/toolchain.md")
    except FileNotFoundError as exc:
        return error("tool.validate.runtime_contracts.io", VN_E_IO, str(exc), "required runtime contract file missing")
    except OSError:
        return error("tool.validate.runtime_contracts.io", VN_E_IO, "root", "failed reading runtime contract files")

    try:
        require_contains(runtime_doc, "`public v1-draft (pre-1.0)`", "runtime_doc.stability")
        require_contains(runtime_doc, "### `VNRuntimeBuildInfo`", "runtime_doc.build_info_struct")
        require_contains(runtime_doc, "### `VNRuntimeSessionSnapshot`", "runtime_doc.snapshot_struct")
        require_contains(runtime_doc, "### `void vn_runtime_query_build_info(VNRuntimeBuildInfo* out_info)`", "runtime_doc.build_info_api")
        require_contains(runtime_doc, "### `int vn_runtime_session_capture_snapshot(const VNRuntimeSession* session, VNRuntimeSessionSnapshot* out_snapshot)`", "runtime_doc.snapshot_capture_api")
        require_contains(runtime_doc, "### `int vn_runtime_session_create_from_snapshot(const VNRuntimeSessionSnapshot* snapshot, VNRuntimeSession** out_session)`", "runtime_doc.snapshot_create_api")
        require_contains(runtime_doc, "### `int vn_runtime_session_save_to_file(const VNRuntimeSession* session, const char* path, vn_u32 slot_id, vn_u32 timestamp_s)`", "runtime_doc.snapshot_save_file_api")
        require_contains(runtime_doc, "### `int vn_runtime_session_load_from_file(const char* path, VNRuntimeSession** out_session)`", "runtime_doc.snapshot_load_file_api")
        require_contains(runtime_doc, "### `int vn_runtime_run(const VNRunConfig* cfg, VNRunResult* out_result)`", "runtime_doc.run_api")
        require_contains(runtime_doc, "### `int vn_runtime_session_create(const VNRunConfig* cfg, VNRuntimeSession** out_session)`", "runtime_doc.session_create")
        require_contains(runtime_doc, "### `int vn_runtime_session_inject_input(VNRuntimeSession* session, const VNInputEvent* event)`", "runtime_doc.inject_input")
        require_contains(runtime_doc, "`VN_RUNTIME_PERF_DYNAMIC_RESOLUTION`", "runtime_doc.dynamic_resolution_flag")
        require_contains(runtime_doc, "`--load-save=<path>`", "runtime_doc.load_save_cli")
        require_contains(runtime_doc, "`--perf-dynamic-resolution=<on|off>`", "runtime_doc.dynamic_resolution_cli")
        require_contains(runtime_doc, "trace_id=runtime.run.ok", "runtime_doc.run_ok")
        require_contains(runtime_doc, "trace_id=runtime.run.failed", "runtime_doc.run_failed")

        require_contains(runtime_header, "#define VN_RUNTIME_PERF_FRAME_REUSE", "runtime_header.frame_reuse")
        require_contains(runtime_header, "#define VN_RUNTIME_PERF_DIRTY_TILE", "runtime_header.dirty_tile")
        require_contains(runtime_header, "#define VN_RUNTIME_PERF_DYNAMIC_RESOLUTION", "runtime_header.dynamic_resolution")
        require_contains(runtime_header, "#define VN_RUNTIME_API_VERSION", "runtime_header.api_version")
        require_contains(runtime_header, "typedef struct {", "runtime_header.struct_present")
        require_contains(runtime_header, "} VNRuntimeBuildInfo;", "runtime_header.build_info_struct")
        require_contains(runtime_header, "} VNRuntimeSessionSnapshot;", "runtime_header.snapshot_struct")
        require_contains(runtime_header, "void vn_runtime_query_build_info(VNRuntimeBuildInfo* out_info);", "runtime_header.build_info_api")
        require_contains(runtime_header, "int vn_runtime_session_capture_snapshot(const VNRuntimeSession* session,", "runtime_header.snapshot_capture_api")
        require_contains(runtime_header, "int vn_runtime_session_create_from_snapshot(const VNRuntimeSessionSnapshot* snapshot,", "runtime_header.snapshot_create_api")
        require_contains(runtime_header, "int vn_runtime_session_save_to_file(const VNRuntimeSession* session,", "runtime_header.snapshot_save_file_api")
        require_contains(runtime_header, "int vn_runtime_session_load_from_file(const char* path,", "runtime_header.snapshot_load_file_api")
        require_contains(runtime_header, "int vn_runtime_run(const VNRunConfig* cfg, VNRunResult* out_result);", "runtime_header.run_api")
        require_contains(runtime_header, "int vn_runtime_session_create(const VNRunConfig* cfg, VNRuntimeSession** out_session);", "runtime_header.session_create")
        require_contains(runtime_header, "int vn_runtime_session_inject_input(VNRuntimeSession* session, const VNInputEvent* event);", "runtime_header.inject_input")

        require_contains(runtime_cli, "runtime.cli.arg.missing", "runtime_cli.arg_missing")
        require_contains(runtime_cli, "runtime.cli.arg.invalid", "runtime_cli.arg_invalid")
        require_contains(runtime_cli, "runtime.cli.scene.invalid", "runtime_cli.scene_invalid")
        require_contains(runtime_cli, "void vn_runtime_query_build_info(VNRuntimeBuildInfo* out_info)", "runtime_cli.build_info_impl")
        require_contains(runtime_cli, "int vn_runtime_session_capture_snapshot(const VNRuntimeSession* session,", "runtime_cli.snapshot_capture_impl")
        require_contains(runtime_cli, "int vn_runtime_session_create_from_snapshot(const VNRuntimeSessionSnapshot* snapshot,", "runtime_cli.snapshot_create_impl")
        require_contains(runtime_cli, "int vn_runtime_session_save_to_file(const VNRuntimeSession* session,", "runtime_cli.snapshot_save_file_impl")
        require_contains(runtime_cli, "int vn_runtime_session_load_from_file(const char* path,", "runtime_cli.snapshot_load_file_impl")
        require_contains(runtime_cli, "--load-save", "runtime_cli.load_save_cli")
        require_contains(runtime_cli, "trace_id=runtime.run.ok", "runtime_cli.run_ok")
        require_contains(runtime_cli, "--perf-dynamic-resolution", "runtime_cli.dynamic_resolution_cli")

        require_contains(runtime_api_test, 'cfg.scene_name = "S10";', "runtime_api_test.s10")
        require_contains(runtime_api_test, "vn_runtime_query_build_info(&build_info);", "runtime_api_test.build_info")
        require_contains(runtime_api_test, "VN_RUNTIME_PERF_DYNAMIC_RESOLUTION", "runtime_api_test.dynamic_resolution")
        require_contains(runtime_api_test, "VN_RUNTIME_PERF_DIRTY_TILE", "runtime_api_test.dirty_tile")
        require_contains(runtime_api_test, "VN_RUNTIME_PERF_FRAME_REUSE", "runtime_api_test.frame_reuse")

        require_contains(runtime_cli_errors_test, "trace_id=runtime.cli.arg.missing", "runtime_cli_errors_test.arg_missing")
        require_contains(runtime_cli_errors_test, "trace_id=runtime.cli.arg.invalid", "runtime_cli_errors_test.arg_invalid")
        require_contains(runtime_cli_errors_test, "trace_id=runtime.cli.scene.invalid", "runtime_cli_errors_test.scene_invalid")
        require_contains(runtime_cli_errors_test, "--load-save", "runtime_cli_errors_test.load_save")
        require_contains(runtime_session_test, "vn_runtime_session_capture_snapshot(session, &snapshot);", "runtime_session_test.snapshot_capture")
        require_contains(runtime_session_test, "vn_runtime_session_create_from_snapshot(&snapshot, &restored_session);", "runtime_session_test.snapshot_restore")
        require_contains(runtime_session_test, "vn_runtime_session_save_to_file(session, save_path, 7u, 123u);", "runtime_session_test.snapshot_save_file")
        require_contains(runtime_session_test, "vn_runtime_session_load_from_file(save_path, &file_session);", "runtime_session_test.snapshot_load_file")

        require_contains(readme, "Session API：`create/step/is_done/set_choice/inject_input/destroy`。", "readme.session_api")
        require_contains(readme, "`--load-save=<save.vnsave>`", "readme.load_save_cli")
        require_contains(readme, "`VN_RUNTIME_PERF_DYNAMIC_RESOLUTION`", "readme.dynamic_resolution")
        require_contains(toolchain, "python3 tools/toolchain.py validate-runtime-contracts", "toolchain.validate_runtime")
    except ValueError as exc:
        return error("tool.validate.runtime_contracts.format", VN_E_FORMAT, str(exc), "runtime contract drift detected")

    print(
        " ".join(
            [
                "trace_id=tool.validate.runtime_contracts.ok",
                f"root={root}",
                "runtime_api=present",
                "session_api=present",
                "cli_errors=structured",
                "perf_flags=present",
            ]
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
