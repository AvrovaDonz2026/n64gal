#!/usr/bin/env python3
import argparse
import pathlib
import struct
import sys

OP_TEXT = 0x03
OP_WAIT = 0x04
OP_CHOICE = 0x05
OP_GOTO = 0x06
OP_CALL = 0x07
OP_RETURN = 0x08
OP_FADE = 0x09
OP_BGM = 0x0A
OP_SE = 0x0B
OP_END = 0xFF


class CompileError(Exception):
    pass


def tokenize_line(raw):
    line = raw.split("#", 1)[0].strip()
    if not line:
        return None
    if line.endswith(":"):
        return ("LABEL", line[:-1].strip())
    parts = line.split()
    return ("INSN", parts)


def parse_number(text):
    base = 10
    if text.startswith("0x") or text.startswith("0X"):
        base = 16
    return int(text, base)


def resolve_target(token, labels):
    if token in labels:
        return labels[token]
    return parse_number(token)


def insn_size(parts):
    op = parts[0].upper()
    argc = len(parts) - 1

    if op == "TEXT":
        if argc != 2:
            raise CompileError("TEXT needs 2 args")
        return 5
    if op == "WAIT":
        if argc != 1:
            raise CompileError("WAIT needs 1 arg")
        return 3
    if op == "CHOICE":
        if argc < 2 or (argc % 2) != 0:
            raise CompileError("CHOICE needs str/target pairs")
        count = argc // 2
        if count > 255:
            raise CompileError("CHOICE count > 255")
        return 2 + (count * 4)
    if op == "GOTO":
        if argc != 1:
            raise CompileError("GOTO needs 1 arg")
        return 3
    if op == "CALL":
        if argc != 1:
            raise CompileError("CALL needs 1 arg")
        return 3
    if op == "RETURN":
        if argc != 0:
            raise CompileError("RETURN has no args")
        return 1
    if op == "FADE":
        if argc != 3:
            raise CompileError("FADE needs 3 args")
        return 5
    if op == "BGM":
        if argc != 2:
            raise CompileError("BGM needs 2 args")
        return 4
    if op == "SE":
        if argc != 1:
            raise CompileError("SE needs 1 arg")
        return 3
    if op == "END":
        if argc != 0:
            raise CompileError("END has no args")
        return 1
    raise CompileError(f"unknown opcode: {parts[0]}")


def parse_source(text):
    labels = {}
    insns = []
    pc = 0

    for lineno, raw in enumerate(text.splitlines(), start=1):
        tok = tokenize_line(raw)
        if tok is None:
            continue
        kind, payload = tok
        if kind == "LABEL":
            if not payload:
                raise CompileError(f"line {lineno}: empty label")
            if payload in labels:
                raise CompileError(f"line {lineno}: duplicate label {payload}")
            labels[payload] = pc
            continue

        parts = payload
        if not parts:
            continue
        try:
            size = insn_size(parts)
        except CompileError as exc:
            raise CompileError(f"line {lineno}: {exc}")
        insns.append((lineno, parts, pc))
        pc += size

    return labels, insns


def encode(labels, insns):
    out = bytearray()

    for lineno, parts, _pc in insns:
        op = parts[0].upper()

        if op == "TEXT":
            text_id = parse_number(parts[1])
            speed = parse_number(parts[2])
            if not (0 <= text_id <= 0xFFFF and 0 <= speed <= 0xFFFF):
                raise CompileError(f"line {lineno}: TEXT args out of range")
            out.append(OP_TEXT)
            out.extend(struct.pack("<HH", text_id, speed))
            continue

        if op == "WAIT":
            ms = parse_number(parts[1])
            if not (0 <= ms <= 0xFFFF):
                raise CompileError(f"line {lineno}: WAIT out of range")
            out.append(OP_WAIT)
            out.extend(struct.pack("<H", ms))
            continue

        if op == "CHOICE":
            count = (len(parts) - 1) // 2
            i = 0
            out.append(OP_CHOICE)
            out.append(count)
            while i < count:
                text_id = parse_number(parts[1 + i * 2])
                target = resolve_target(parts[2 + i * 2], labels)
                if not (0 <= text_id <= 0xFFFF and 0 <= target <= 0xFFFF):
                    raise CompileError(f"line {lineno}: CHOICE args out of range")
                out.extend(struct.pack("<HH", text_id, target))
                i += 1
            continue

        if op == "GOTO":
            target = resolve_target(parts[1], labels)
            if not (0 <= target <= 0xFFFF):
                raise CompileError(f"line {lineno}: GOTO target out of range")
            out.append(OP_GOTO)
            out.extend(struct.pack("<H", target))
            continue

        if op == "CALL":
            target = resolve_target(parts[1], labels)
            if not (0 <= target <= 0xFFFF):
                raise CompileError(f"line {lineno}: CALL target out of range")
            out.append(OP_CALL)
            out.extend(struct.pack("<H", target))
            continue

        if op == "RETURN":
            out.append(OP_RETURN)
            continue

        if op == "FADE":
            layer_mask = parse_number(parts[1])
            alpha = parse_number(parts[2])
            duration = parse_number(parts[3])
            if not (0 <= layer_mask <= 0xFF and 0 <= alpha <= 0xFF and 0 <= duration <= 0xFFFF):
                raise CompileError(f"line {lineno}: FADE args out of range")
            out.append(OP_FADE)
            out.extend(struct.pack("<BBH", layer_mask, alpha, duration))
            continue

        if op == "BGM":
            audio_id = parse_number(parts[1])
            loop = parse_number(parts[2])
            if not (0 <= audio_id <= 0xFFFF and 0 <= loop <= 0xFF):
                raise CompileError(f"line {lineno}: BGM args out of range")
            out.append(OP_BGM)
            out.extend(struct.pack("<HB", audio_id, loop))
            continue

        if op == "SE":
            audio_id = parse_number(parts[1])
            if not (0 <= audio_id <= 0xFFFF):
                raise CompileError(f"line {lineno}: SE arg out of range")
            out.append(OP_SE)
            out.extend(struct.pack("<H", audio_id))
            continue

        if op == "END":
            out.append(OP_END)
            continue

        raise CompileError(f"line {lineno}: unknown opcode {parts[0]}")

    return bytes(out)


def main():
    parser = argparse.ArgumentParser(description="Compile .vns.txt to .vns.bin")
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()

    src = args.input.read_text(encoding="utf-8")
    labels, insns = parse_source(src)
    payload = encode(labels, insns)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    print(f"[scriptc] wrote {args.output} ({len(payload)} bytes)")


if __name__ == "__main__":
    try:
        main()
    except CompileError as exc:
        print(f"compile error: {exc}", file=sys.stderr)
        sys.exit(2)
