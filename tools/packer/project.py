#!/usr/bin/env python3
import argparse
import json
import pathlib
import re
import struct
import sys
import zlib


ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "scriptc"))

from compile_vns import CompileError, encode, parse_source
from make_demo_pack import (
    FORMAT_TO_FLAG,
    MAX_RESOURCE_COUNT,
    RESOURCE_TYPE_IMAGE,
    RESOURCE_TYPE_SCENE_CATALOG,
    RESOURCE_TYPE_SCRIPT,
    convert_png_to_format,
    crc32_u32,
    encode_pack,
    load_image_jobs,
)


PROJECT_VERSION = 2
MIN_RUNTIME = "v1.1.0"
MAX_SCENES = 256
CATALOG_MAGIC = b"VNSC"
CATALOG_VERSION = 1
SCENE_NAME_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_-]{0,62}$")
LEGACY_SCENE_IDS = {
    "S0": 0,
    "S1": 1,
    "S2": 2,
    "S3": 3,
    "S10": 10,
}
RESERVED_SCENE_IDS = frozenset(LEGACY_SCENE_IDS.values())


class ProjectError(Exception):
    def __init__(self, field, message):
        super().__init__(message)
        self.field = field


def fnv1a32(text):
    value = 0x811C9DC5
    for byte in text.encode("ascii"):
        value ^= byte
        value = (value * 0x01000193) & 0xFFFFFFFF
    return value


def scene_id_for_name(name):
    if name in LEGACY_SCENE_IDS:
        return LEGACY_SCENE_IDS[name]
    return fnv1a32(name)


def resolve_inside(project_root, value, field):
    if not isinstance(value, str) or not value:
        raise ProjectError(field, "expected non-empty relative path")
    path = pathlib.Path(value)
    if path.is_absolute():
        raise ProjectError(field, "absolute paths are not allowed")
    resolved = (project_root / path).resolve()
    try:
        resolved.relative_to(project_root)
    except ValueError:
        raise ProjectError(field, "path escapes project root")
    return resolved


def project_relative(project_root, path):
    return path.resolve().relative_to(project_root).as_posix()


def load_json(path, field):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise ProjectError(field, f"file not found: {path}")
    except (OSError, UnicodeError) as exc:
        raise ProjectError(field, f"failed reading file: {exc}")
    except json.JSONDecodeError as exc:
        raise ProjectError(field, f"invalid JSON at line {exc.lineno} column {exc.colno}")


def load_project(project_arg):
    candidate = pathlib.Path(project_arg).resolve()
    if candidate.is_dir():
        project_root = candidate
        project_path = candidate / "template.json"
    else:
        project_path = candidate
        project_root = candidate.parent

    data = load_json(project_path, "template.json")
    if not isinstance(data, dict):
        raise ProjectError("template.json", "project descriptor must be a JSON object")
    if data.get("template_version") != PROJECT_VERSION:
        raise ProjectError("template_version", f"expected {PROJECT_VERSION}")
    if data.get("kind") != "content-project":
        raise ProjectError("kind", "expected content-project")

    entry_scene = data.get("entry_scene")
    if not isinstance(entry_scene, str):
        raise ProjectError("entry_scene", "expected scene name")

    raw_scenes = data.get("scenes")
    if not isinstance(raw_scenes, list) or not raw_scenes:
        raise ProjectError("scenes", "expected non-empty array")
    if len(raw_scenes) > MAX_SCENES:
        raise ProjectError("scenes", f"scene count exceeds runtime limit {MAX_SCENES}")

    scenes = []
    seen_names = set()
    seen_ids = {}
    for index, item in enumerate(raw_scenes):
        field = f"scenes[{index}]"
        if not isinstance(item, dict):
            raise ProjectError(field, "expected object")
        name = item.get("name")
        if not isinstance(name, str) or SCENE_NAME_RE.fullmatch(name) is None:
            raise ProjectError(f"{field}.name", "expected ASCII [A-Za-z][A-Za-z0-9_-]{0,62}")
        if name in seen_names:
            raise ProjectError(f"{field}.name", f"duplicate scene name: {name}")
        scene_id = scene_id_for_name(name)
        if name not in LEGACY_SCENE_IDS and scene_id in RESERVED_SCENE_IDS:
            raise ProjectError(f"{field}.name", f"scene id collides with reserved id: {scene_id}")
        if scene_id in seen_ids:
            raise ProjectError(
                f"{field}.name",
                f"scene id collision with {seen_ids[scene_id]}: 0x{scene_id:08x}",
            )
        script_path = resolve_inside(project_root, item.get("script"), f"{field}.script")
        if not script_path.is_file():
            raise ProjectError(f"{field}.script", f"script not found: {script_path}")
        seen_names.add(name)
        seen_ids[scene_id] = name
        scenes.append(
            {
                "name": name,
                "scene_id": scene_id,
                "script_path": script_path,
                "script_source": project_relative(project_root, script_path),
                "script_resource_id": index,
            }
        )

    if entry_scene not in seen_names:
        raise ProjectError("entry_scene", f"unknown scene: {entry_scene}")

    images_manifest = resolve_inside(project_root, data.get("images_manifest"), "images_manifest")
    if not images_manifest.is_file():
        raise ProjectError("images_manifest", f"manifest not found: {images_manifest}")
    pack_output = resolve_inside(project_root, data.get("pack_output"), "pack_output")
    if pack_output.suffix.lower() != ".vnpak":
        raise ProjectError("pack_output", "expected .vnpak output path")

    try:
        image_jobs = load_image_jobs(images_manifest)
    except (ValueError, OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ProjectError("images_manifest", str(exc))

    image_symbols = {}
    for index, job in enumerate(image_jobs):
        name = job["name"]
        if not name or any(ch.isspace() for ch in name) or not name.isascii():
            raise ProjectError(f"images[{index}].name", "expected non-empty ASCII token")
        if name.lower() == "none":
            raise ProjectError(f"images[{index}].name", "none is reserved")
        if name in image_symbols:
            raise ProjectError(f"images[{index}].name", f"duplicate image name: {name}")
        try:
            job["source"].resolve().relative_to(project_root)
        except ValueError:
            raise ProjectError(f"images[{index}].source", "path escapes project root")
        resource_id = len(scenes) + index
        job["resource_id"] = resource_id
        job["source_rel"] = project_relative(project_root, job["source"])
        image_symbols[name] = resource_id

    catalog_resource_id = len(scenes) + len(image_jobs)
    if catalog_resource_id >= MAX_RESOURCE_COUNT:
        raise ProjectError("resources", f"resource count exceeds runtime limit {MAX_RESOURCE_COUNT}")

    return {
        "root": project_root,
        "path": project_path,
        "config": data,
        "entry_scene": entry_scene,
        "scenes": scenes,
        "images_manifest": images_manifest,
        "image_jobs": image_jobs,
        "image_symbols": image_symbols,
        "pack_output": pack_output,
        "catalog_resource_id": catalog_resource_id,
    }


def build_catalog(project):
    entry_scene_id = None
    payload = bytearray()
    for scene in project["scenes"]:
        if scene["name"] == project["entry_scene"]:
            entry_scene_id = scene["scene_id"]
        name_bytes = scene["name"].encode("ascii")
        payload.extend(
            struct.pack(
                "<IHBB",
                scene["scene_id"],
                scene["script_resource_id"],
                len(name_bytes),
                0,
            )
        )
        payload.extend(name_bytes)
    return struct.pack(
        "<4sHHI",
        CATALOG_MAGIC,
        CATALOG_VERSION,
        len(project["scenes"]),
        entry_scene_id,
    ) + bytes(payload)


def prepare_build(project):
    blobs = []
    compiled_scripts = []
    for scene in project["scenes"]:
        try:
            source = scene["script_path"].read_text(encoding="utf-8")
            labels, insns = parse_source(source)
            script_bytes = encode(
                labels,
                insns,
                project["image_symbols"],
                strict_resources=True,
            )
        except (OSError, UnicodeError) as exc:
            raise ProjectError(f"scene.{scene['name']}.script", f"failed reading script: {exc}")
        except (CompileError, ValueError) as exc:
            raise ProjectError(f"scene.{scene['name']}.script", str(exc))
        compiled_scripts.append((scene, script_bytes))
        blobs.append(
            {
                "id": scene["script_resource_id"],
                "name": scene["name"],
                "source": scene["script_source"],
                "kind": "script",
                "type": RESOURCE_TYPE_SCRIPT,
                "flags": 0,
                "width": 0,
                "height": 0,
                "data_size": len(script_bytes),
                "crc32": crc32_u32(script_bytes),
                "data": script_bytes,
            }
        )

    for index, job in enumerate(project["image_jobs"]):
        try:
            width, height, image_bytes = convert_png_to_format(job["source"], job["format"])
        except (OSError, ValueError, zlib.error) as exc:
            raise ProjectError(f"images[{index}]", str(exc))
        blobs.append(
            {
                "id": job["resource_id"],
                "name": job["name"],
                "source": job["source_rel"],
                "kind": "image",
                "format": job["format"],
                "type": RESOURCE_TYPE_IMAGE,
                "flags": FORMAT_TO_FLAG[job["format"]],
                "width": width,
                "height": height,
                "data_size": len(image_bytes),
                "crc32": crc32_u32(image_bytes),
                "data": image_bytes,
            }
        )

    catalog = build_catalog(project)
    blobs.append(
        {
            "id": project["catalog_resource_id"],
            "name": "scene_catalog",
            "source": project_relative(project["root"], project["path"]),
            "kind": "scene_catalog",
            "type": RESOURCE_TYPE_SCENE_CATALOG,
            "flags": 0,
            "width": 0,
            "height": 0,
            "data_size": len(catalog),
            "crc32": crc32_u32(catalog),
            "data": catalog,
        }
    )
    pack_bytes, pack_manifest = encode_pack(blobs)
    return compiled_scripts, catalog, pack_bytes, pack_manifest


def make_resource_map(project):
    resources = []
    for scene in project["scenes"]:
        resources.append(
            {
                "id": scene["script_resource_id"],
                "kind": "script",
                "name": scene["name"],
                "scene_id": f"{scene['scene_id']:08x}",
            }
        )
    for job in project["image_jobs"]:
        resources.append(
            {
                "id": job["resource_id"],
                "kind": "image",
                "name": job["name"],
            }
        )
    resources.append(
        {
            "id": project["catalog_resource_id"],
            "kind": "scene_catalog",
            "name": "scene_catalog",
        }
    )
    return {
        "schema_version": 1,
        "entry_scene": project["entry_scene"],
        "catalog_resource_id": project["catalog_resource_id"],
        "symbols": dict(project["image_symbols"]),
        "resources": resources,
    }


def write_json(path, data):
    path.write_text(
        json.dumps(data, ensure_ascii=True, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def validate_project(project_arg):
    project = load_project(project_arg)
    _compiled, _catalog, _pack, pack_manifest = prepare_build(project)
    print(
        " ".join(
            [
                "trace_id=tool.project.validate.ok",
                f"project={project['path']}",
                f"scenes={len(project['scenes'])}",
                f"images={len(project['image_jobs'])}",
                f"resources={pack_manifest['resource_count']}",
            ]
        )
    )
    return 0


def build_project(project_arg):
    project = load_project(project_arg)
    compiled_scripts, catalog, pack_bytes, pack_manifest = prepare_build(project)
    output_dir = project["pack_output"].parent
    scripts_dir = output_dir / "scripts"
    output_dir.mkdir(parents=True, exist_ok=True)
    scripts_dir.mkdir(parents=True, exist_ok=True)
    for scene, script_bytes in compiled_scripts:
        (scripts_dir / f"{scene['name']}.vns.bin").write_bytes(script_bytes)

    project["pack_output"].write_bytes(pack_bytes)
    resource_map = make_resource_map(project)
    resource_map_path = output_dir / "resource-map.json"
    write_json(resource_map_path, resource_map)

    audit_manifest = dict(pack_manifest)
    audit_manifest.update(
        {
            "schema_version": 1,
            "kind": "n64gal-content-build",
            "min_runtime": MIN_RUNTIME,
            "project_template_version": PROJECT_VERSION,
            "entry_scene": project["entry_scene"],
            "entry_scene_id": f"{scene_id_for_name(project['entry_scene']):08x}",
            "catalog_resource_id": project["catalog_resource_id"],
            "catalog_magic": CATALOG_MAGIC.decode("ascii"),
            "catalog_version": CATALOG_VERSION,
            "catalog_size": len(catalog),
            "resource_map": project_relative(project["root"], resource_map_path),
            "pack_output": project_relative(project["root"], project["pack_output"]),
        }
    )
    manifest_path = output_dir / "manifest.json"
    write_json(manifest_path, audit_manifest)
    print(
        " ".join(
            [
                "trace_id=tool.project.build.ok",
                f"project={project['path']}",
                f"pack={project['pack_output']}",
                f"manifest={manifest_path}",
                f"resource_map={resource_map_path}",
                f"resources={pack_manifest['resource_count']}",
            ]
        )
    )
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="Validate or build a N64GAL content project")
    subparsers = parser.add_subparsers(dest="command", required=True)
    for command in ("validate", "build"):
        subparser = subparsers.add_parser(command)
        subparser.add_argument("project", help="project directory or template.json")
    args = parser.parse_args(argv)

    try:
        if args.command == "validate":
            return validate_project(args.project)
        return build_project(args.project)
    except ProjectError as exc:
        print(
            " ".join(
                [
                    "trace_id=tool.project.invalid",
                    "error_code=-3",
                    "error_name=VN_E_FORMAT",
                    f"field={exc.field}",
                    f"message={exc}",
                ]
            ),
            file=sys.stderr,
        )
        return 1
    except OSError as exc:
        print(
            " ".join(
                [
                    "trace_id=tool.project.io",
                    "error_code=-2",
                    "error_name=VN_E_IO",
                    f"message={exc}",
                ]
            ),
            file=sys.stderr,
        )
        return 1


if __name__ == "__main__":
    sys.exit(main())
