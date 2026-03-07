#!/usr/bin/env python3
import argparse
import json
import pathlib
import struct
import zlib

MAGIC = b"VNPK"
VERSION = 2
ENTRY_SIZE = 18

RESOURCE_TYPE_IMAGE = 1
RESOURCE_TYPE_SCRIPT = 2

FMT_RGBA16 = 1
FMT_CI8 = 2
FMT_IA8 = 3
FORMAT_TO_FLAG = {
    "rgba16": FMT_RGBA16,
    "ci8": FMT_CI8,
    "ia8": FMT_IA8,
}

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def crc32_u32(data):
    return zlib.crc32(data) & 0xFFFFFFFF


def paeth_predictor(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def decode_png_rgba(path):
    data = path.read_bytes()
    if len(data) < 8 or data[:8] != PNG_SIGNATURE:
        raise ValueError(f"not a PNG file: {path}")

    pos = 8
    width = None
    height = None
    bit_depth = None
    color_type = None
    compression = None
    filter_method = None
    interlace = None
    plte = None
    trns = None
    idat = bytearray()

    while pos + 8 <= len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        ctype = data[pos + 4 : pos + 8]
        pos += 8
        if pos + length + 4 > len(data):
            raise ValueError(f"corrupt PNG chunk bounds: {path}")
        chunk = data[pos : pos + length]
        pos += length
        _crc = data[pos : pos + 4]
        pos += 4

        if ctype == b"IHDR":
            if length != 13:
                raise ValueError(f"invalid IHDR size: {path}")
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(
                ">IIBBBBB", chunk
            )
        elif ctype == b"PLTE":
            plte = bytes(chunk)
        elif ctype == b"tRNS":
            trns = bytes(chunk)
        elif ctype == b"IDAT":
            idat.extend(chunk)
        elif ctype == b"IEND":
            break

    if width is None or height is None:
        raise ValueError(f"missing IHDR: {path}")
    if bit_depth != 8:
        raise ValueError(f"unsupported PNG bit depth={bit_depth}: {path}")
    if compression != 0 or filter_method != 0:
        raise ValueError(f"unsupported PNG compression/filter: {path}")
    if interlace != 0:
        raise ValueError(f"unsupported interlaced PNG: {path}")
    if width <= 0 or height <= 0 or width > 65535 or height > 65535:
        raise ValueError(f"invalid PNG dimensions {width}x{height}: {path}")

    if color_type == 6:
        bpp = 4
    elif color_type == 2:
        bpp = 3
    elif color_type == 0:
        bpp = 1
    elif color_type == 4:
        bpp = 2
    elif color_type == 3:
        bpp = 1
        if plte is None or (len(plte) % 3) != 0:
            raise ValueError(f"indexed PNG missing/invalid PLTE: {path}")
    else:
        raise ValueError(f"unsupported PNG color_type={color_type}: {path}")

    raw = zlib.decompress(bytes(idat))
    stride = width * bpp
    expect = height * (1 + stride)
    if len(raw) != expect:
        raise ValueError(f"decoded PNG size mismatch {len(raw)} != {expect}: {path}")

    out_scan = bytearray(height * stride)
    in_pos = 0
    out_pos = 0
    for _row in range(height):
        ftype = raw[in_pos]
        in_pos += 1
        row = bytearray(raw[in_pos : in_pos + stride])
        in_pos += stride

        if ftype == 0:
            pass
        elif ftype == 1:
            for i in range(stride):
                left = row[i - bpp] if i >= bpp else 0
                row[i] = (row[i] + left) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                up = out_scan[out_pos - stride + i] if out_pos >= stride else 0
                row[i] = (row[i] + up) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                left = row[i - bpp] if i >= bpp else 0
                up = out_scan[out_pos - stride + i] if out_pos >= stride else 0
                row[i] = (row[i] + ((left + up) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                left = row[i - bpp] if i >= bpp else 0
                up = out_scan[out_pos - stride + i] if out_pos >= stride else 0
                up_left = out_scan[out_pos - stride + i - bpp] if (out_pos >= stride and i >= bpp) else 0
                row[i] = (row[i] + paeth_predictor(left, up, up_left)) & 0xFF
        else:
            raise ValueError(f"unsupported PNG filter={ftype}: {path}")

        out_scan[out_pos : out_pos + stride] = row
        out_pos += stride

    rgba = bytearray(width * height * 4)
    src_pos = 0
    dst_pos = 0
    palette_count = 0
    palette = []
    alpha_table = []
    if color_type == 3:
        palette_count = len(plte) // 3
        palette = plte
        alpha_table = trns if trns is not None else b""

    for _ in range(width * height):
        if color_type == 6:
            r = out_scan[src_pos]
            g = out_scan[src_pos + 1]
            b = out_scan[src_pos + 2]
            a = out_scan[src_pos + 3]
            src_pos += 4
        elif color_type == 2:
            r = out_scan[src_pos]
            g = out_scan[src_pos + 1]
            b = out_scan[src_pos + 2]
            a = 255
            src_pos += 3
        elif color_type == 0:
            v = out_scan[src_pos]
            r = v
            g = v
            b = v
            a = 255
            src_pos += 1
        elif color_type == 4:
            v = out_scan[src_pos]
            r = v
            g = v
            b = v
            a = out_scan[src_pos + 1]
            src_pos += 2
        else:
            idx = out_scan[src_pos]
            if idx >= palette_count:
                raise ValueError(f"palette index out of range idx={idx} count={palette_count}: {path}")
            r = palette[idx * 3]
            g = palette[idx * 3 + 1]
            b = palette[idx * 3 + 2]
            if idx < len(alpha_table):
                a = alpha_table[idx]
            else:
                a = 255
            src_pos += 1

        rgba[dst_pos] = r
        rgba[dst_pos + 1] = g
        rgba[dst_pos + 2] = b
        rgba[dst_pos + 3] = a
        dst_pos += 4

    return width, height, bytes(rgba)


def rgba_to_rgba16_be(rgba):
    out = bytearray((len(rgba) // 4) * 2)
    src = 0
    dst = 0
    while src < len(rgba):
        r = rgba[src]
        g = rgba[src + 1]
        b = rgba[src + 2]
        a = rgba[src + 3]
        v = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (1 if a >= 128 else 0)
        out[dst] = (v >> 8) & 0xFF
        out[dst + 1] = v & 0xFF
        src += 4
        dst += 2
    return bytes(out)


def rgba_to_ia8(rgba):
    out = bytearray(len(rgba) // 4)
    src = 0
    dst = 0
    while src < len(rgba):
        r = rgba[src]
        g = rgba[src + 1]
        b = rgba[src + 2]
        a = rgba[src + 3]
        intensity = (77 * r + 150 * g + 29 * b + 128) >> 8
        out[dst] = ((intensity >> 4) << 4) | (a >> 4)
        src += 4
        dst += 1
    return bytes(out)


def rgba_to_ci8_payload(width, height, rgba):
    pixels = len(rgba) // 4
    if pixels != width * height:
        raise ValueError("rgba length does not match image dimensions")

    seen = {}
    palette = []
    indices = bytearray(pixels)
    pi = 0
    for i in range(pixels):
        c = (
            rgba[i * 4],
            rgba[i * 4 + 1],
            rgba[i * 4 + 2],
            rgba[i * 4 + 3],
        )
        if c not in seen:
            if len(palette) < 256:
                seen[c] = len(palette)
                palette.append(c)
            else:
                break
        pi += 1

    if len(palette) <= 256 and pi == pixels:
        for i in range(pixels):
            c = (
                rgba[i * 4],
                rgba[i * 4 + 1],
                rgba[i * 4 + 2],
                rgba[i * 4 + 3],
            )
            indices[i] = seen[c]
        while len(palette) < 256:
            palette.append((0, 0, 0, 0))
    else:
        acc = [[0, 0, 0, 0, 0] for _ in range(256)]
        for i in range(pixels):
            r = rgba[i * 4]
            g = rgba[i * 4 + 1]
            b = rgba[i * 4 + 2]
            a = rgba[i * 4 + 3]
            idx = ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6)
            indices[i] = idx
            slot = acc[idx]
            slot[0] += r
            slot[1] += g
            slot[2] += b
            slot[3] += a
            slot[4] += 1
        palette = []
        for slot in acc:
            if slot[4] > 0:
                cnt = slot[4]
                palette.append(
                    (
                        slot[0] // cnt,
                        slot[1] // cnt,
                        slot[2] // cnt,
                        slot[3] // cnt,
                    )
                )
            else:
                palette.append((0, 0, 0, 0))

    pal_rgba = bytearray(256 * 4)
    for i, (r, g, b, a) in enumerate(palette):
        pal_rgba[i * 4] = r
        pal_rgba[i * 4 + 1] = g
        pal_rgba[i * 4 + 2] = b
        pal_rgba[i * 4 + 3] = a
    pal_rgba16 = rgba_to_rgba16_be(bytes(pal_rgba))
    return pal_rgba16 + bytes(indices)


def convert_png_to_format(path, fmt):
    width, height, rgba = decode_png_rgba(path)
    if fmt == "rgba16":
        return width, height, rgba_to_rgba16_be(rgba)
    if fmt == "ia8":
        return width, height, rgba_to_ia8(rgba)
    if fmt == "ci8":
        return width, height, rgba_to_ci8_payload(width, height, rgba)
    raise ValueError(f"unsupported format: {fmt}")


def load_image_jobs(manifest_path):
    if manifest_path is None:
        return []
    if not manifest_path.exists():
        return []

    root = manifest_path.parent
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    if isinstance(data, dict):
        items = data.get("images", [])
    elif isinstance(data, list):
        items = data
    else:
        raise ValueError(f"invalid images manifest shape: {manifest_path}")

    jobs = []
    for i, item in enumerate(items):
        if not isinstance(item, dict):
            raise ValueError(f"images manifest item[{i}] must be object")
        source = item.get("source")
        fmt = item.get("format", "").lower()
        name = item.get("name")
        if not source or not fmt:
            raise ValueError(f"images manifest item[{i}] missing source/format")
        if fmt not in FORMAT_TO_FLAG:
            raise ValueError(f"images manifest item[{i}] unsupported format={fmt}")
        src_path = root / source
        if not src_path.exists():
            raise ValueError(f"images manifest item[{i}] source not found: {src_path}")
        if not name:
            name = f"{src_path.stem}_{fmt}"
        jobs.append(
            {
                "name": str(name),
                "source": src_path,
                "format": fmt,
            }
        )
    return jobs


def build_pack(script_paths, image_jobs):
    blobs = []
    next_id = 0

    for p in script_paths:
        data = p.read_bytes()
        blobs.append(
            {
                "id": next_id,
                "name": p.name,
                "source": str(p),
                "kind": "script",
                "type": RESOURCE_TYPE_SCRIPT,
                "flags": 0,
                "width": 0,
                "height": 0,
                "data_size": len(data),
                "crc32": crc32_u32(data),
                "data": data,
            }
        )
        next_id += 1

    for job in image_jobs:
        w, h, payload = convert_png_to_format(job["source"], job["format"])
        blobs.append(
            {
                "id": next_id,
                "name": job["name"],
                "source": str(job["source"]),
                "kind": "image",
                "format": job["format"],
                "type": RESOURCE_TYPE_IMAGE,
                "flags": FORMAT_TO_FLAG[job["format"]],
                "width": w,
                "height": h,
                "data_size": len(payload),
                "crc32": crc32_u32(payload),
                "data": payload,
            }
        )
        next_id += 1

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

    resources_manifest = []
    for item in blobs:
        out = {
            "id": item["id"],
            "name": item["name"],
            "source": item["source"],
            "kind": item["kind"],
            "type": item["type"],
            "flags": item["flags"],
            "width": item["width"],
            "height": item["height"],
            "data_off": item["data_off"],
            "data_size": item["data_size"],
            "crc32": f"{item['crc32']:08x}",
        }
        if "format" in item:
            out["format"] = item["format"]
        resources_manifest.append(out)

    manifest = {
        "vnpak_magic": "VNPK",
        "vnpak_version": VERSION,
        "resource_count": count,
        "entry_size": ENTRY_SIZE,
        "pack_size": len(pack_bytes),
        "pack_crc32": f"{crc32_u32(pack_bytes):08x}",
        "resources": resources_manifest,
    }
    return pack_bytes, manifest


def main():
    parser = argparse.ArgumentParser(description="Create demo.vnpak from compiled scripts/images")
    parser.add_argument("--scripts-dir", required=True, type=pathlib.Path)
    parser.add_argument("--images-manifest", default=None, type=pathlib.Path)
    parser.add_argument("--out", required=True, type=pathlib.Path)
    parser.add_argument("--manifest-out", default=None, type=pathlib.Path)
    args = parser.parse_args()

    scripts = [
        args.scripts_dir / "S0.vns.bin",
        args.scripts_dir / "S1.vns.bin",
        args.scripts_dir / "S2.vns.bin",
        args.scripts_dir / "S3.vns.bin",
        args.scripts_dir / "S10.vns.bin",
    ]
    image_jobs = load_image_jobs(args.images_manifest)

    payload, manifest = build_pack(scripts, image_jobs)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(payload)
    print(f"[vnpak] wrote {args.out} ({len(payload)} bytes)")
    if image_jobs:
        print(f"[vnpak] packed images={len(image_jobs)}")
    else:
        print("[vnpak] packed images=0")

    if args.manifest_out is not None:
        args.manifest_out.parent.mkdir(parents=True, exist_ok=True)
        args.manifest_out.write_text(
            json.dumps(manifest, ensure_ascii=True, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(f"[vnpak] wrote {args.manifest_out}")


if __name__ == "__main__":
    main()
