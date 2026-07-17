#!/usr/bin/env python3
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TOOLCHAIN = [sys.executable, str(ROOT / "tools" / "toolchain.py")]
sys.path.insert(0, str(ROOT / "tools" / "packer"))

from make_demo_pack import decode_png_rgba


def run(args):
    return subprocess.run(
        TOOLCHAIN + args,
        cwd=ROOT,
        capture_output=True,
        text=True,
    )


def fnv1a32(text):
    value = 0x811C9DC5
    for byte in text.encode("ascii"):
        value ^= byte
        value = (value * 0x01000193) & 0xFFFFFFFF
    return value


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def parse_pack(path):
    data = path.read_bytes()
    magic, version, count = struct.unpack_from("<4sHH", data, 0)
    entries = []
    for index in range(count):
        entries.append(struct.unpack_from("<BBHHIII", data, 8 + index * 18))
    return data, magic, version, entries


def png_chunk(chunk_type, payload):
    checksum = zlib.crc32(chunk_type + payload) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + chunk_type + payload + struct.pack(">I", checksum)


def write_test_png(path, width, color_type, pixels, trns=None, plte=None):
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color_type]
    if len(pixels) != width * channels:
        raise AssertionError("test PNG pixel payload size mismatch")
    ihdr = struct.pack(">IIBBBBB", width, 1, 8, color_type, 0, 0, 0)
    data = bytearray(b"\x89PNG\r\n\x1a\n")
    data.extend(png_chunk(b"IHDR", ihdr))
    if plte is not None:
        data.extend(png_chunk(b"PLTE", plte))
    if trns is not None:
        data.extend(png_chunk(b"tRNS", trns))
    data.extend(png_chunk(b"IDAT", zlib.compress(b"\x00" + pixels)))
    data.extend(png_chunk(b"IEND", b""))
    path.write_bytes(bytes(data))


def expect_png_error(path, expected_error):
    try:
        decode_png_rgba(path)
    except ValueError as exc:
        if expected_error not in str(exc):
            raise AssertionError(f"unexpected PNG error for {path.name}: {exc}") from exc
    else:
        raise AssertionError(f"invalid PNG should fail: {path.name}")


def assert_png_trns_support(root):
    grayscale = root / "grayscale-trns.png"
    write_test_png(grayscale, 2, 0, bytes((17, 18)), struct.pack(">H", 17))
    width, height, rgba = decode_png_rgba(grayscale)
    if (width, height, rgba) != (
        2,
        1,
        bytes((17, 17, 17, 0, 18, 18, 18, 255)),
    ):
        raise AssertionError(f"grayscale tRNS decode mismatch: {rgba!r}")

    truecolor = root / "truecolor-trns.png"
    write_test_png(
        truecolor,
        2,
        2,
        bytes((1, 2, 3, 1, 2, 4)),
        struct.pack(">HHH", 1, 2, 3),
    )
    width, height, rgba = decode_png_rgba(truecolor)
    if (width, height, rgba) != (
        2,
        1,
        bytes((1, 2, 3, 0, 1, 2, 4, 255)),
    ):
        raise AssertionError(f"truecolor tRNS decode mismatch: {rgba!r}")

    indexed = root / "indexed-trns.png"
    write_test_png(
        indexed,
        2,
        3,
        bytes((0, 1)),
        b"\x00",
        bytes((10, 20, 30, 40, 50, 60)),
    )
    _, _, rgba = decode_png_rgba(indexed)
    if rgba != bytes((10, 20, 30, 0, 40, 50, 60, 255)):
        raise AssertionError(f"indexed tRNS decode mismatch: {rgba!r}")

    invalid_cases = (
        ("grayscale-length.png", 0, bytes((1,)), b"\x01", None, "grayscale PNG tRNS length must be 2"),
        (
            "grayscale-range.png",
            0,
            bytes((1,)),
            struct.pack(">H", 256),
            None,
            "grayscale PNG tRNS sample exceeds 8-bit range",
        ),
        ("truecolor-length.png", 2, bytes((1, 2, 3)), b"\x00\x01", None, "truecolor PNG tRNS length must be 6"),
        (
            "truecolor-range.png",
            2,
            bytes((1, 2, 3)),
            struct.pack(">HHH", 1, 2, 256),
            None,
            "truecolor PNG tRNS sample exceeds 8-bit range",
        ),
        (
            "indexed-length.png",
            3,
            bytes((0,)),
            b"\x00\xff",
            bytes((1, 2, 3)),
            "indexed PNG tRNS length exceeds PLTE entries",
        ),
        ("grayscale-alpha.png", 4, bytes((1, 255)), b"\x00\x01", None, "color_type=4 must not contain tRNS"),
        (
            "truecolor-alpha.png",
            6,
            bytes((1, 2, 3, 255)),
            b"\x00\x01\x00\x02\x00\x03",
            None,
            "color_type=6 must not contain tRNS",
        ),
    )
    for name, color_type, pixels, trns, plte, expected_error in invalid_cases:
        path = root / name
        write_test_png(path, 1, color_type, pixels, trns, plte)
        expect_png_error(path, expected_error)


def assert_project_build(project, descriptor_path):
    proc = run(["validate-project", str(descriptor_path)])
    if proc.returncode != 0 or "trace_id=tool.project.validate.ok" not in proc.stdout:
        raise AssertionError(f"validate-project failed stdout={proc.stdout} stderr={proc.stderr}")
    if (project / "build").exists():
        raise AssertionError("validate-project wrote build outputs")

    proc = run(["build-project", str(descriptor_path)])
    if proc.returncode != 0 or "trace_id=tool.project.build.ok" not in proc.stdout:
        raise AssertionError(f"build-project failed stdout={proc.stdout} stderr={proc.stderr}")

    pack, magic, version, entries = parse_pack(project / "build" / "story.vnpak")
    if magic != b"VNPK" or version != 2 or len(entries) != 5:
        raise AssertionError("vnpak header mismatch")
    if [entry[0] for entry in entries] != [2, 2, 1, 1, 3]:
        raise AssertionError(f"resource order mismatch: {[entry[0] for entry in entries]}")

    catalog_entry = entries[-1]
    catalog = pack[catalog_entry[4] : catalog_entry[4] + catalog_entry[5]]
    catalog_magic, catalog_version, scene_count, entry_scene_id = struct.unpack_from("<4sHHI", catalog, 0)
    if (catalog_magic, catalog_version, scene_count, entry_scene_id) != (
        b"VNSC",
        1,
        2,
        fnv1a32("Beta"),
    ):
        raise AssertionError("scene catalog header mismatch")

    offset = 12
    catalog_scenes = []
    for _ in range(scene_count):
        scene_id, script_id, name_len, reserved = struct.unpack_from("<IHBB", catalog, offset)
        offset += 8
        name = catalog[offset : offset + name_len].decode("ascii")
        offset += name_len
        catalog_scenes.append((scene_id, script_id, reserved, name))
    if catalog_scenes != [
        (fnv1a32("Alpha"), 0, 0, "Alpha"),
        (fnv1a32("Beta"), 1, 0, "Beta"),
    ]:
        raise AssertionError(f"scene catalog entries mismatch: {catalog_scenes}")

    alpha = (project / "build" / "scripts" / "Alpha.vns.bin").read_bytes()
    expected_alpha = (
        b"\x01\x02\x00\x07\x00"
        b"\x02\x08\x03\x00\xf4\xff\xff\x7f"
        b"\x02\x08\xff\xff\x00\x80\x00\x00"
        b"\xff"
    )
    if alpha != expected_alpha:
        raise AssertionError(f"BG/SPRITE encoding mismatch: {alpha.hex()}")

    standalone = project / "build" / "standalone.vns.bin"
    proc = subprocess.run(
        [
            sys.executable,
            str(ROOT / "tools" / "scriptc" / "compile_vns.py"),
            str(project / "assets" / "scripts" / "Alpha.vns.txt"),
            str(standalone),
            "--resource-map",
            str(project / "build" / "resource-map.json"),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0 or standalone.read_bytes() != expected_alpha:
        raise AssertionError(f"standalone resource-map compile failed: {proc.stderr}")

    resource_map = json.loads((project / "build" / "resource-map.json").read_text(encoding="utf-8"))
    if resource_map.get("symbols") != {"face": 3, "room": 2}:
        raise AssertionError(f"resource symbols mismatch: {resource_map}")
    if resource_map.get("catalog_resource_id") != 4:
        raise AssertionError("catalog resource id mismatch")

    manifest = json.loads((project / "build" / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("min_runtime") != "v1.1.0" or manifest.get("catalog_magic") != "VNSC":
        raise AssertionError("audit manifest mismatch")
    if manifest["resources"][-1].get("source") != "content-project.json":
        raise AssertionError("catalog source does not match project descriptor")


def main():
    with tempfile.TemporaryDirectory(prefix="n64gal_png_trns_") as tmp:
        assert_png_trns_support(Path(tmp))

    with tempfile.TemporaryDirectory(prefix="n64gal_project_") as tmp:
        project = Path(tmp)
        scripts = project / "assets" / "scripts"
        images = project / "assets" / "images"
        scripts.mkdir(parents=True)
        images.mkdir(parents=True)
        shutil.copy2(ROOT / "templates" / "minimal-vn" / "assets" / "images" / "demo_ui.png", images / "visual.png")

        (scripts / "Alpha.vns.txt").write_text(
            "BG room 7\n"
            "SPRITE 8 face -12 32767\n"
            "SPRITE 8 none -32768 0\n"
            "END\n",
            encoding="utf-8",
        )
        (scripts / "Beta.vns.txt").write_text("END\n", encoding="utf-8")
        write_json(
            images / "images.json",
            {
                "images": [
                    {"name": "room", "source": "visual.png", "format": "rgba16"},
                    {"name": "face", "source": "visual.png", "format": "ia8"},
                ]
            },
        )
        descriptor_path = project / "content-project.json"
        write_json(
            descriptor_path,
            {
                "template_version": 2,
                "template_name": "test-content",
                "kind": "content-project",
                "entry_scene": "Beta",
                "scenes": [
                    {"name": "Alpha", "script": "assets/scripts/Alpha.vns.txt"},
                    {"name": "Beta", "script": "assets/scripts/Beta.vns.txt"},
                ],
                "images_manifest": "assets/images/images.json",
                "pack_output": "build/story.vnpak",
            },
        )
        assert_project_build(project, descriptor_path)

        (scripts / "Beta.vns.txt").write_text("BG missing_image 0\nEND\n", encoding="utf-8")
        proc = run(["validate-project", str(descriptor_path)])
        if proc.returncode == 0 or "unknown resource: missing_image" not in proc.stderr:
            raise AssertionError(f"unknown image should fail stdout={proc.stdout} stderr={proc.stderr}")

        (scripts / "Beta.vns.txt").write_text("END\n", encoding="utf-8")
        descriptor = json.loads(descriptor_path.read_text(encoding="utf-8"))
        descriptor["scenes"][1]["name"] = "9invalid"
        write_json(descriptor_path, descriptor)
        proc = run(["validate-project", str(descriptor_path)])
        if proc.returncode == 0 or "scenes[1].name" not in proc.stderr:
            raise AssertionError(f"invalid scene name should fail stdout={proc.stdout} stderr={proc.stderr}")

        descriptor["scenes"][1]["name"] = "Beta"
        write_json(descriptor_path, descriptor)
        write_json(
            images / "images.json",
            {
                "images": [
                    {"name": f"image_{index}", "source": "visual.png", "format": "ia8"}
                    for index in range(4094)
                ]
            },
        )
        proc = run(["validate-project", str(descriptor_path)])
        if proc.returncode == 0 or "resource count exceeds runtime limit 4096" not in proc.stderr:
            raise AssertionError(f"oversized resource table should fail stdout={proc.stdout} stderr={proc.stderr}")

        oversized_script = scripts / "TooLarge.vns.txt"
        oversized_output = project / "build" / "TooLarge.vns.bin"
        oversized_script.write_text("END\n" * 65536, encoding="utf-8")
        proc = subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools" / "scriptc" / "compile_vns.py"),
                str(oversized_script),
                str(oversized_output),
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if proc.returncode == 0 or "script size exceeds VM limit 65535 bytes" not in proc.stderr:
            raise AssertionError(f"oversized script should fail stdout={proc.stdout} stderr={proc.stderr}")
        if oversized_output.exists():
            raise AssertionError("oversized script compiler wrote an output")

        invalid_control_flow = (
            ("Outside", "GOTO 60000\nEND\n", "GOTO target out of range"),
            ("EofLabel", "GOTO eof\nEND\neof:\n", "GOTO target out of range"),
            (
                "MiddleOfInstruction",
                "TEXT 1 1\nGOTO 1\nEND\n",
                "GOTO target is not an instruction boundary",
            ),
            (
                "ReachableEof",
                "TEXT 1 1\n",
                "reachable control flow falls through end of script",
            ),
            (
                "ZeroWaitLoop",
                "loop:\nWAIT 0\nGOTO loop\n",
                "control flow exceeds VM 128-step guard",
            ),
            (
                "GuardOverflow",
                ("TEXT 1 1\n" * 128) + "END\n",
                "control flow exceeds VM 128-step guard",
            ),
            (
                "ConsumedWaitAtGuard",
                ("TEXT 1 1\n" * 127) + "WAIT 1\nEND\n",
                "control flow exceeds VM 128-step guard",
            ),
            ("Empty", "", "script has no instructions"),
        )
        for name, source, expected_error in invalid_control_flow:
            source_path = scripts / f"{name}.vns.txt"
            output_path = project / "build" / f"{name}.vns.bin"
            source_path.write_text(source, encoding="utf-8")
            proc = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "tools" / "scriptc" / "compile_vns.py"),
                    str(source_path),
                    str(output_path),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
            )
            if proc.returncode == 0 or expected_error not in proc.stderr:
                raise AssertionError(
                    f"invalid control flow should fail name={name} stdout={proc.stdout} stderr={proc.stderr}"
                )
            if output_path.exists():
                raise AssertionError(f"invalid control flow wrote output: {output_path}")

        valid_control_flow = (
            ("PositiveWaitLoop", "loop:\nWAIT 1001\nGOTO loop\n"),
            ("CallReturn", "CALL sub\nEND\nsub:\nWAIT 1\nRETURN\n"),
            ("GuardBoundary", ("TEXT 1 1\n" * 127) + "END\n"),
            ("YieldingWaitBoundary", ("TEXT 1 1\n" * 127) + "WAIT 1001\nEND\n"),
        )
        for name, source in valid_control_flow:
            source_path = scripts / f"{name}.vns.txt"
            output_path = project / "build" / f"{name}.vns.bin"
            source_path.write_text(source, encoding="utf-8")
            proc = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "tools" / "scriptc" / "compile_vns.py"),
                    str(source_path),
                    str(output_path),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
            )
            if proc.returncode != 0 or not output_path.is_file() or output_path.stat().st_size == 0:
                raise AssertionError(
                    f"valid control flow should compile name={name} stdout={proc.stdout} stderr={proc.stderr}"
                )

    print("test_project_toolchain ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
