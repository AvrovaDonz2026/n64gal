#!/usr/bin/env python3
import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build_toolchain"
TMP_BUILD_DIR = BUILD_DIR / "tmp"
VALIDATE_ALL_COMMANDS = (
    ("validate-release-audit", [sys.executable, "tools/validate/validate_release_audit.py", "--allow-dirty"]),
    ("validate-release-docs", [sys.executable, "tools/validate/validate_release_docs.py"]),
    ("validate-manifest", [sys.executable, "tools/validate/validate_manifest.py", "tests/fixtures/tool_manifest/valid/vnsave_migrate.json"]),
    ("validate-release-contracts", [sys.executable, "tools/validate/validate_release_contracts.py"]),
    ("validate-toolchain-contracts", [sys.executable, "tools/validate/validate_toolchain_contracts.py"]),
    ("validate-backend-contracts", [sys.executable, "tools/validate/validate_backend_contracts.py"]),
    ("validate-api-index-contracts", [sys.executable, "tools/validate/validate_api_index_contracts.py"]),
    ("validate-compat-matrix", [sys.executable, "tools/validate/validate_compat_matrix.py"]),
    ("validate-ecosystem-contracts", [sys.executable, "tools/validate/validate_ecosystem_contracts.py"]),
    ("validate-error-contracts", [sys.executable, "tools/validate/validate_error_contracts.py"]),
    ("validate-host-sdk-contracts", [sys.executable, "tools/validate/validate_host_sdk_contracts.py"]),
    ("validate-migration-contracts", [sys.executable, "tools/validate/validate_migration_contracts.py"]),
    ("validate-pack-contracts", [sys.executable, "tools/validate/validate_pack_contracts.py"]),
    ("validate-platform-contracts", [sys.executable, "tools/validate/validate_platform_contracts.py"]),
    ("validate-preview-contracts", [sys.executable, "tools/validate/validate_preview_contracts.py"]),
    ("validate-perf-contracts", [sys.executable, "tools/validate/validate_perf_contracts.py"]),
    ("validate-porting-contracts", [sys.executable, "tools/validate/validate_porting_contracts.py"]),
    ("validate-runtime-contracts", [sys.executable, "tools/validate/validate_runtime_contracts.py"]),
    ("validate-save-contracts", [sys.executable, "tools/validate/validate_save_contracts.py"]),
    ("validate-template-contracts", [sys.executable, "tools/validate/validate_template_contracts.py"]),
)
CFLAGS = [
    "-std=c89",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic-errors",
    "-Iinclude",
]


def print_usage(program: str) -> int:
    print(
        "\n".join(
            [
                f"usage: {program} <command> [args]",
                "",
                "commands:",
                "  validate-all",
                "  validate-release-audit [--allow-dirty] [--require-clean] [--skip-git]",
                "  validate-release-docs",
                "  validate-manifest <manifest.json>",
                "  validate-release-contracts",
                "  validate-toolchain-contracts",
                "  validate-backend-contracts",
                "  validate-api-index-contracts",
                "  validate-compat-matrix",
                "  validate-ecosystem-contracts",
                "  validate-error-contracts",
                "  validate-host-sdk-contracts",
                "  validate-migration-contracts",
                "  validate-pack-contracts",
                "  validate-platform-contracts",
                "  validate-preview-contracts",
                "  validate-perf-contracts",
                "  validate-porting-contracts",
                "  validate-runtime-contracts",
                "  validate-save-contracts",
                "  validate-template-contracts",
                "  release-gate [--allow-dirty] [--skip-cc-suite] [--summary-out <path>] [--summary-json-out <path>] [--with-soak] [--with-bundle]",
                "  release-host-sdk-smoke [--summary-out <path>] [--summary-json-out <path>] [--skip-build]",
                "  release-platform-evidence [--out-dir <path>] [--platform-doc <path>] [--ci-suite-summary <path>] [--summary-out <path>] [--summary-json-out <path>]",
                "  release-preview-evidence [--summary-out <path>] [--summary-json-out <path>] [--skip-build]",
                "  release-soak [--frames-per-scene <n>] [--scenes <S0,...>] [--backend <name>] [--summary-out <path>] [--summary-json-out <path>]",
                "  release-bundle [--out-dir <path>] [--gate-summary <path>] [--soak-summary <path>] [--ci-summary <path>] [--host-sdk-summary <path>] [--platform-evidence-summary <path>] [--preview-evidence-summary <path>]",
                "  release-report [--out-dir <path>] [--bundle-index <path>] [--bundle-manifest <path>] [--gate-summary <path>] [--soak-summary <path>] [--ci-suite-summary <path>] [--host-sdk-summary <path>] [--platform-evidence-summary <path>] [--preview-evidence-summary <path>]",
                "  release-publish-map [--out-dir <path>] [--tag <tag>] [--release-url <url>] [--bundle-index <path>] [--bundle-manifest <path>] [--report-json <path>]",
                "  migrate-vnsave --in <legacy_v0.vnsave> --out <v1.vnsave>",
                "  probe-vnsave --in <save.vnsave>",
                "  probe-trace-summary <runtime_trace.log>",
                "  probe-preview [vn_previewd args]",
                "  probe-perf-summary <perf_summary.csv>",
                "  probe-perf-compare <perf_compare.csv>",
                "  probe-kernel-bench <kernel_bench.csv>",
                "  probe-kernel-compare <kernel_compare.csv>",
                "",
                f"trace_id=tool.toolchain.help command={program}",
            ]
        ),
        file=sys.stderr,
    )
    return 2


def repo_rel(path: Path) -> str:
    return str(path.relative_to(ROOT))


def needs_rebuild(output: Path, sources) -> bool:
    if not output.exists():
        return True
    out_mtime = output.stat().st_mtime
    for src in sources:
        if src.stat().st_mtime > out_mtime:
            return True
    return False


def ensure_c_tool(output_name: str, source_paths) -> Path:
    cc = os.environ.get("CC", "cc")
    output = BUILD_DIR / output_name
    sources = [ROOT / rel for rel in source_paths]
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    TMP_BUILD_DIR.mkdir(parents=True, exist_ok=True)
    if needs_rebuild(output, sources):
        cmd = [cc] + CFLAGS + source_paths + ["-o", repo_rel(output)]
        env = os.environ.copy()
        env["TMPDIR"] = str(TMP_BUILD_DIR)
        proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, env=env)
        if proc.stdout:
            sys.stdout.write(proc.stdout)
        if proc.stderr:
            sys.stderr.write(proc.stderr)
        if proc.returncode != 0:
            raise RuntimeError("compile failed")
    return output


def run_forward(cmd) -> int:
    proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if proc.stdout:
        sys.stdout.write(proc.stdout)
    if proc.stderr:
        sys.stderr.write(proc.stderr)
    return proc.returncode


def command_validate_manifest(argv) -> int:
    if len(argv) != 1:
        print("trace_id=tool.toolchain.validate.usage error_code=-1 error_name=VN_E_INVALID_ARG message=expected manifest path", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_manifest.py", argv[0]])


def command_validate_release_docs(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_release_docs.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_release_docs.py"])


def command_validate_release_audit(argv) -> int:
    return run_forward([sys.executable, "tools/validate/validate_release_audit.py"] + list(argv))


def command_validate_all(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_all.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2

    for name, cmd in VALIDATE_ALL_COMMANDS:
        rc = run_forward(cmd)
        if rc != 0:
            print(
                " ".join(
                    [
                        "trace_id=tool.toolchain.validate_all.failed",
                        f"validator={name}",
                        "error_code=-3",
                        "error_name=VN_E_FORMAT",
                        "message=validator failed",
                    ]
                ),
                file=sys.stderr,
            )
            return 1

    print(
        " ".join(
            [
                "trace_id=tool.toolchain.validate_all.ok",
                f"validator_count={len(VALIDATE_ALL_COMMANDS)}",
            ]
        )
    )
    return 0


def command_validate_release_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_release_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_release_contracts.py"])


def command_validate_toolchain_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_toolchain_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_toolchain_contracts.py"])


def command_validate_backend_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_backend_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_backend_contracts.py"])


def command_validate_api_index_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_api_index_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_api_index_contracts.py"])


def command_validate_compat_matrix(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_compat_matrix.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_compat_matrix.py"])


def command_validate_ecosystem_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_ecosystem_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_ecosystem_contracts.py"])


def command_validate_error_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_error_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_error_contracts.py"])


def command_validate_host_sdk_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_host_sdk_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_host_sdk_contracts.py"])


def command_validate_migration_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_migration_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_migration_contracts.py"])


def command_validate_pack_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_pack_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_pack_contracts.py"])


def command_validate_platform_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_platform_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_platform_contracts.py"])


def command_validate_preview_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_preview_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_preview_contracts.py"])


def command_validate_perf_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_perf_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_perf_contracts.py"])


def command_validate_porting_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_porting_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_porting_contracts.py"])


def command_validate_runtime_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_runtime_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_runtime_contracts.py"])


def command_validate_save_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_save_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_save_contracts.py"])


def command_validate_template_contracts(argv) -> int:
    if len(argv) != 0:
        print("trace_id=tool.toolchain.validate_template_contracts.usage error_code=-1 error_name=VN_E_INVALID_ARG message=unexpected argument", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/validate/validate_template_contracts.py"])


def command_release_gate(argv) -> int:
    return run_forward(["bash", "scripts/release/run_release_gate.sh"] + list(argv))


def command_release_host_sdk_smoke(argv) -> int:
    return run_forward(["bash", "scripts/release/run_host_sdk_smoke.sh"] + list(argv))


def command_release_platform_evidence(argv) -> int:
    return run_forward(["bash", "scripts/release/run_platform_evidence.sh"] + list(argv))


def command_release_preview_evidence(argv) -> int:
    return run_forward(["bash", "scripts/release/run_preview_evidence.sh"] + list(argv))


def command_release_soak(argv) -> int:
    return run_forward(["bash", "scripts/release/run_demo_soak.sh"] + list(argv))


def command_release_bundle(argv) -> int:
    return run_forward(["bash", "scripts/release/run_release_bundle.sh"] + list(argv))


def command_release_report(argv) -> int:
    return run_forward(["bash", "scripts/release/run_release_report.sh"] + list(argv))


def command_release_publish_map(argv) -> int:
    return run_forward(["bash", "scripts/release/run_release_publish_map.sh"] + list(argv))


def command_migrate_vnsave(argv) -> int:
    tool = ensure_c_tool(
        "vnsave_migrate",
        [
            "tools/migrate/vnsave_migrate.c",
            "src/core/error.c",
            "src/core/save.c",
            "src/core/platform.c",
        ],
    )
    return run_forward([repo_rel(tool)] + argv)


def command_probe_vnsave(argv) -> int:
    tool = ensure_c_tool(
        "vnsave_probe",
        [
            "tools/probe/vnsave_probe.c",
            "src/core/error.c",
            "src/core/save.c",
            "src/core/platform.c",
        ],
    )
    return run_forward([repo_rel(tool)] + argv)


def command_probe_trace_summary(argv) -> int:
    if len(argv) != 1:
        print("trace_id=tool.toolchain.probe_trace.usage error_code=-1 error_name=VN_E_INVALID_ARG message=expected trace log path", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/probe/trace_summary.py", argv[0]])


def command_probe_preview(argv) -> int:
    previewd = ensure_c_tool(
        "vn_previewd",
        [
            "src/tools/previewd_main.c",
            "src/tools/preview_cli.c",
            "src/core/error.c",
            "src/core/backend_registry.c",
            "src/core/renderer.c",
            "src/core/save.c",
            "src/core/vm.c",
            "src/core/pack.c",
            "src/core/platform.c",
            "src/core/runtime_cli.c",
            "src/core/dynamic_resolution.c",
            "src/frontend/render_ops.c",
            "src/frontend/dirty_tiles.c",
            "src/backend/common/pixel_pipeline.c",
            "src/backend/avx2/avx2_backend.c",
            "src/backend/avx2/avx2_fill_fade.c",
            "src/backend/avx2/avx2_textured.c",
            "src/backend/neon/neon_backend.c",
            "src/backend/rvv/rvv_backend.c",
            "src/backend/scalar/scalar_backend.c",
        ],
    )

    response_path = None
    for idx, arg in enumerate(argv):
        if arg == "--response" and (idx + 1) < len(argv):
            response_path = argv[idx + 1]
            break
        if arg.startswith("--response="):
            response_path = arg.split("=", 1)[1]
            break

    cleanup_response = False
    if response_path is None:
        TMP_BUILD_DIR.mkdir(parents=True, exist_ok=True)
        fd, temp_path = tempfile.mkstemp(prefix="preview_probe_", suffix=".json", dir=str(TMP_BUILD_DIR))
        os.close(fd)
        response_path = os.path.relpath(temp_path, ROOT)
        argv = list(argv) + ["--response", response_path]
        cleanup_response = True

    rc = run_forward([repo_rel(previewd)] + argv)
    summary_rc = 1
    if os.path.exists(ROOT / response_path):
        summary_rc = run_forward([sys.executable, "tools/probe/preview_summary.py", response_path])

    if cleanup_response and os.path.exists(ROOT / response_path):
        os.remove(ROOT / response_path)

    if rc != 0:
        return rc
    return summary_rc


def command_probe_perf_summary(argv) -> int:
    if len(argv) != 1:
        print("trace_id=tool.toolchain.probe_perf.usage error_code=-1 error_name=VN_E_INVALID_ARG message=expected perf summary path", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/probe/perf_summary.py", argv[0]])


def command_probe_perf_compare(argv) -> int:
    if len(argv) != 1:
        print("trace_id=tool.toolchain.probe_perf_compare.usage error_code=-1 error_name=VN_E_INVALID_ARG message=expected perf compare path", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/probe/perf_compare_summary.py", argv[0]])


def command_probe_kernel_bench(argv) -> int:
    if len(argv) != 1:
        print("trace_id=tool.toolchain.probe_kernel_bench.usage error_code=-1 error_name=VN_E_INVALID_ARG message=expected kernel bench path", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/probe/kernel_bench_summary.py", argv[0]])


def command_probe_kernel_compare(argv) -> int:
    if len(argv) != 1:
        print("trace_id=tool.toolchain.probe_kernel_compare.usage error_code=-1 error_name=VN_E_INVALID_ARG message=expected kernel compare path", file=sys.stderr)
        return 2
    return run_forward([sys.executable, "tools/probe/kernel_compare_summary.py", argv[0]])


def main(argv) -> int:
    if len(argv) < 2 or argv[1] in ("-h", "--help", "help"):
        return print_usage(argv[0])

    command = argv[1]
    args = argv[2:]
    try:
        if command == "validate-all":
            return command_validate_all(args)
        if command == "validate-release-audit":
            return command_validate_release_audit(args)
        if command == "validate-release-docs":
            return command_validate_release_docs(args)
        if command == "validate-manifest":
            return command_validate_manifest(args)
        if command == "validate-release-contracts":
            return command_validate_release_contracts(args)
        if command == "validate-toolchain-contracts":
            return command_validate_toolchain_contracts(args)
        if command == "validate-backend-contracts":
            return command_validate_backend_contracts(args)
        if command == "validate-api-index-contracts":
            return command_validate_api_index_contracts(args)
        if command == "validate-compat-matrix":
            return command_validate_compat_matrix(args)
        if command == "validate-ecosystem-contracts":
            return command_validate_ecosystem_contracts(args)
        if command == "validate-error-contracts":
            return command_validate_error_contracts(args)
        if command == "validate-host-sdk-contracts":
            return command_validate_host_sdk_contracts(args)
        if command == "validate-migration-contracts":
            return command_validate_migration_contracts(args)
        if command == "validate-pack-contracts":
            return command_validate_pack_contracts(args)
        if command == "validate-platform-contracts":
            return command_validate_platform_contracts(args)
        if command == "validate-preview-contracts":
            return command_validate_preview_contracts(args)
        if command == "validate-perf-contracts":
            return command_validate_perf_contracts(args)
        if command == "validate-porting-contracts":
            return command_validate_porting_contracts(args)
        if command == "validate-runtime-contracts":
            return command_validate_runtime_contracts(args)
        if command == "validate-save-contracts":
            return command_validate_save_contracts(args)
        if command == "validate-template-contracts":
            return command_validate_template_contracts(args)
        if command == "release-gate":
            return command_release_gate(args)
        if command == "release-host-sdk-smoke":
            return command_release_host_sdk_smoke(args)
        if command == "release-platform-evidence":
            return command_release_platform_evidence(args)
        if command == "release-preview-evidence":
            return command_release_preview_evidence(args)
        if command == "release-soak":
            return command_release_soak(args)
        if command == "release-bundle":
            return command_release_bundle(args)
        if command == "release-report":
            return command_release_report(args)
        if command == "release-publish-map":
            return command_release_publish_map(args)
        if command == "migrate-vnsave":
            return command_migrate_vnsave(args)
        if command == "probe-vnsave":
            return command_probe_vnsave(args)
        if command == "probe-trace-summary":
            return command_probe_trace_summary(args)
        if command == "probe-preview":
            return command_probe_preview(args)
        if command == "probe-perf-summary":
            return command_probe_perf_summary(args)
        if command == "probe-perf-compare":
            return command_probe_perf_compare(args)
        if command == "probe-kernel-bench":
            return command_probe_kernel_bench(args)
        if command == "probe-kernel-compare":
            return command_probe_kernel_compare(args)
    except RuntimeError:
        print("trace_id=tool.toolchain.compile.failed error_code=-2 error_name=VN_E_IO message=failed to build tool helper", file=sys.stderr)
        return 1

    print(
        f"trace_id=tool.toolchain.invalid error_code=-1 error_name=VN_E_INVALID_ARG command={command} message=unknown command",
        file=sys.stderr,
    )
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
