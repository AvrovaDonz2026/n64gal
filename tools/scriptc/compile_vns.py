#!/usr/bin/env python3
import argparse
import pathlib
import struct
import sys

OP_TEXT = 0x03
OP_WAIT = 0x04
OP_GOTO = 0x06
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


def insn_size(parts):
    op = parts[0].upper()
    if op == "TEXT":
        return 5
    if op == "WAIT":
        return 3
    if op == "GOTO":
        return 3
    if op == "END":
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
        size = insn_size(parts)
        insns.append((lineno, parts, pc))
        pc += size

    return labels, insns


def encode(labels, insns):
    out = bytearray()

    for lineno, parts, _pc in insns:
        op = parts[0].upper()
        if op == "TEXT":
            if len(parts) != 3:
                raise CompileError(f"line {lineno}: TEXT needs 2 args")
            text_id = parse_number(parts[1])
            speed = parse_number(parts[2])
            if not (0 <= text_id <= 0xFFFF and 0 <= speed <= 0xFFFF):
                raise CompileError(f"line {lineno}: TEXT args out of range")
            out.append(OP_TEXT)
            out.extend(struct.pack("<HH", text_id, speed))
            continue

        if op == "WAIT":
            if len(parts) != 2:
                raise CompileError(f"line {lineno}: WAIT needs 1 arg")
            ms = parse_number(parts[1])
            if not (0 <= ms <= 0xFFFF):
                raise CompileError(f"line {lineno}: WAIT out of range")
            out.append(OP_WAIT)
            out.extend(struct.pack("<H", ms))
            continue

        if op == "GOTO":
            if len(parts) != 2:
                raise CompileError(f"line {lineno}: GOTO needs 1 arg")
            target = parts[1]
            if target in labels:
                off = labels[target]
            else:
                off = parse_number(target)
            if not (0 <= off <= 0xFFFF):
                raise CompileError(f"line {lineno}: GOTO target out of range")
            out.append(OP_GOTO)
            out.extend(struct.pack("<H", off))
            continue

        if op == "END":
            if len(parts) != 1:
                raise CompileError(f"line {lineno}: END has no args")
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
