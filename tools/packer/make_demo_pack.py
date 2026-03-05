#!/usr/bin/env python3
import argparse
import json
import pathlib
import struct
import zlib

MAGIC = b"VNPK"
VERSION = 2
ENTRY_SIZE = 18


def crc32_u32(data):
    return zlib.crc32(data) & 0xFFFFFFFF


def build_pack(script_paths):
    blobs = []
    for idx, p in enumerate(script_paths):
        data = p.read_bytes()
        blobs.append(
            {
                "id": idx,
                "name": p.name,
                "type": 2,
                "flags": 0,
                "width": 0,
                "height": 0,
                "data_size": len(data),
                "crc32": crc32_u32(data),
                "data": data,
            }
        )

    count = len(blobs)
    header = struct.pack("<4sHH", MAGIC, VERSION, count)
    table_size = ENTRY_SIZE * count
    data_off = len(header) + table_size

    entries = bytearray()
    payload = bytearray()

    for item in blobs:
        item["data_off"] = data_off
        entries.extend(
            struct.pack(
                "<BBHHIII",
                item["type"],
                item["flags"],
                item["width"],
                item["height"],
                item["data_off"],
                item["data_size"],
                item["crc32"],
            )
        )
        payload.extend(item["data"])
        data_off += item["data_size"]

    pack_bytes = header + bytes(entries) + bytes(payload)
    manifest = {
        "vnpak_magic": "VNPK",
        "vnpak_version": VERSION,
        "resource_count": count,
        "entry_size": ENTRY_SIZE,
        "pack_size": len(pack_bytes),
        "pack_crc32": f"{crc32_u32(pack_bytes):08x}",
        "resources": [
            {
                "id": item["id"],
                "name": item["name"],
                "type": item["type"],
                "flags": item["flags"],
                "width": item["width"],
                "height": item["height"],
                "data_off": item["data_off"],
                "data_size": item["data_size"],
                "crc32": f"{item['crc32']:08x}",
            }
            for item in blobs
        ],
    }
    return pack_bytes, manifest


def main():
    parser = argparse.ArgumentParser(description="Create demo.vnpak from compiled scripts")
    parser.add_argument("--scripts-dir", required=True, type=pathlib.Path)
    parser.add_argument("--out", required=True, type=pathlib.Path)
    parser.add_argument("--manifest-out", default=None, type=pathlib.Path)
    args = parser.parse_args()

    scripts = [
        args.scripts_dir / "S0.vns.bin",
        args.scripts_dir / "S1.vns.bin",
        args.scripts_dir / "S2.vns.bin",
        args.scripts_dir / "S3.vns.bin",
    ]

    payload, manifest = build_pack(scripts)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(payload)
    print(f"[vnpak] wrote {args.out} ({len(payload)} bytes)")

    if args.manifest_out is not None:
        args.manifest_out.parent.mkdir(parents=True, exist_ok=True)
        args.manifest_out.write_text(
            json.dumps(manifest, ensure_ascii=True, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(f"[vnpak] wrote {args.manifest_out}")


if __name__ == "__main__":
    main()
