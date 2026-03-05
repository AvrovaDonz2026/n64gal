#!/usr/bin/env python3
import argparse
import pathlib
import struct

MAGIC = b"VNPK"
VERSION = 1
ENTRY_SIZE = 14


def build_pack(script_paths):
    blobs = []
    for p in script_paths:
        data = p.read_bytes()
        blobs.append((p.name, data))

    count = len(blobs)
    header = struct.pack("<4sHH", MAGIC, VERSION, count)
    table_size = ENTRY_SIZE * count
    data_off = len(header) + table_size

    entries = bytearray()
    payload = bytearray()

    for _name, data in blobs:
        size = len(data)
        entries.extend(struct.pack("<BBHHII", 2, 0, 0, 0, data_off, size))
        payload.extend(data)
        data_off += size

    return header + bytes(entries) + bytes(payload)


def main():
    parser = argparse.ArgumentParser(description="Create demo.vnpak from compiled scripts")
    parser.add_argument("--scripts-dir", required=True, type=pathlib.Path)
    parser.add_argument("--out", required=True, type=pathlib.Path)
    args = parser.parse_args()

    scripts = [
        args.scripts_dir / "S0.vns.bin",
        args.scripts_dir / "S1.vns.bin",
        args.scripts_dir / "S2.vns.bin",
        args.scripts_dir / "S3.vns.bin",
    ]

    payload = build_pack(scripts)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(payload)
    print(f"[vnpak] wrote {args.out} ({len(payload)} bytes)")


if __name__ == "__main__":
    main()
