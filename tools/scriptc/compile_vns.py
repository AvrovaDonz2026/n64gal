#!/usr/bin/env python3
import argparse
import json
import pathlib
import struct
import sys

OP_BG = 0x01
OP_SPRITE = 0x02
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
MAX_SCRIPT_SIZE = 0xFFFF
VM_STEP_GUARD = 128
VM_CALL_STACK_MAX = 16
VM_MAX_DT_MS = 1000


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


def validate_target(target, script_size, instruction_offsets, lineno, op):
    if not (0 <= target < script_size):
        raise CompileError(f"line {lineno}: {op} target out of range")
    if target not in instruction_offsets:
        raise CompileError(f"line {lineno}: {op} target is not an instruction boundary")


def validate_control_flow(labels, insns, script_size, instruction_offsets):
    if not insns:
        raise CompileError("script has no instructions")

    insn_by_pc = {pc: (lineno, parts) for lineno, parts, pc in insns}
    pending = [(0, (), 0, VM_MAX_DT_MS)]
    visited = set()

    def enqueue(next_pc, call_stack, step_count, remaining_ms, lineno):
        if next_pc not in instruction_offsets:
            raise CompileError(
                f"line {lineno}: reachable control flow falls through end of script"
            )
        pending.append((next_pc, call_stack, step_count, remaining_ms))

    while pending:
        pc, call_stack, step_count, remaining_ms = pending.pop()
        state = (pc, call_stack, step_count, remaining_ms)
        if state in visited:
            continue
        visited.add(state)

        if pc not in insn_by_pc:
            raise CompileError("reachable control flow falls through end of script")
        lineno, parts = insn_by_pc[pc]
        op = parts[0].upper()
        next_pc = pc + insn_size(parts)
        next_step_count = step_count + 1

        if op == "END":
            continue
        if op == "WAIT":
            wait_ms = parse_number(parts[1])
            if wait_ms > remaining_ms:
                enqueue(next_pc, call_stack, 0, VM_MAX_DT_MS, lineno)
                continue
            remaining_ms -= wait_ms
        if op == "RETURN":
            if not call_stack:
                raise CompileError(
                    f"line {lineno}: RETURN is reachable with an empty call stack"
                )
            next_pc = call_stack[-1]
            next_call_stack = call_stack[:-1]
            successors = [(next_pc, next_call_stack)]
        elif op == "CALL":
            if len(call_stack) >= VM_CALL_STACK_MAX:
                raise CompileError(
                    f"line {lineno}: CALL stack exceeds VM limit {VM_CALL_STACK_MAX}"
                )
            target = resolve_target(parts[1], labels)
            validate_target(target, script_size, instruction_offsets, lineno, op)
            successors = [(target, call_stack + (next_pc,))]
        elif op == "GOTO":
            target = resolve_target(parts[1], labels)
            validate_target(target, script_size, instruction_offsets, lineno, op)
            successors = [(target, call_stack)]
        elif op == "CHOICE":
            count = (len(parts) - 1) // 2
            successors = []
            for i in range(count):
                target = resolve_target(parts[2 + i * 2], labels)
                validate_target(target, script_size, instruction_offsets, lineno, op)
                successors.append((target, call_stack))
        else:
            successors = [(next_pc, call_stack)]

        if next_step_count >= VM_STEP_GUARD:
            raise CompileError(
                f"line {lineno}: control flow exceeds VM {VM_STEP_GUARD}-step guard "
                "without a guaranteed-yield WAIT or END"
            )
        for successor_pc, successor_stack in successors:
            enqueue(successor_pc,
                    successor_stack,
                    next_step_count,
                    remaining_ms,
                    lineno)


def resolve_resource(token, resources, allow_none=False, strict_resources=False):
    if allow_none and token.lower() == "none":
        return 0xFFFF
    if token in resources:
        value = resources[token]
    else:
        try:
            value = parse_number(token)
        except ValueError:
            raise CompileError(f"unknown resource: {token}")
    if not isinstance(value, int) or isinstance(value, bool):
        raise CompileError(f"resource id is not an integer: {token}")
    if not (0 <= value <= 0xFFFF):
        raise CompileError(f"resource id out of range: {token}")
    if strict_resources and value not in resources.values():
        raise CompileError(f"unknown resource id: {value}")
    return value


def insn_size(parts):
    op = parts[0].upper()
    argc = len(parts) - 1

    if op == "BG":
        if argc != 2:
            raise CompileError("BG needs 2 args")
        return 5
    if op == "SPRITE":
        if argc != 4:
            raise CompileError("SPRITE needs 4 args")
        return 8
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
        if pc > MAX_SCRIPT_SIZE:
            raise CompileError(
                f"line {lineno}: script size exceeds VM limit {MAX_SCRIPT_SIZE} bytes"
            )

    return labels, insns


def encode(labels, insns, resources=None, strict_resources=False):
    if resources is None:
        resources = {}
    out = bytearray()
    script_size = 0
    instruction_offsets = {pc for _lineno, _parts, pc in insns}

    for _lineno, parts, pc in insns:
        script_size = max(script_size, pc + insn_size(parts))

    for lineno, parts, _pc in insns:
        op = parts[0].upper()

        if op == "BG":
            try:
                image_id = resolve_resource(parts[1], resources, strict_resources=strict_resources)
                duration = parse_number(parts[2])
            except (CompileError, ValueError) as exc:
                raise CompileError(f"line {lineno}: {exc}")
            if not (0 <= duration <= 0xFFFF):
                raise CompileError(f"line {lineno}: BG duration out of range")
            out.append(OP_BG)
            out.extend(struct.pack("<HH", image_id, duration))
            continue

        if op == "SPRITE":
            try:
                layer = parse_number(parts[1])
                image_id = resolve_resource(
                    parts[2],
                    resources,
                    allow_none=True,
                    strict_resources=strict_resources,
                )
                x = parse_number(parts[3])
                y = parse_number(parts[4])
            except (CompileError, ValueError) as exc:
                raise CompileError(f"line {lineno}: {exc}")
            if not (1 <= layer <= 8):
                raise CompileError(f"line {lineno}: SPRITE layer out of range")
            if not (-0x8000 <= x <= 0x7FFF and -0x8000 <= y <= 0x7FFF):
                raise CompileError(f"line {lineno}: SPRITE coordinates out of range")
            out.append(OP_SPRITE)
            out.extend(struct.pack("<BHhh", layer, image_id, x, y))
            continue

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
                if not (0 <= text_id <= 0xFFFF):
                    raise CompileError(f"line {lineno}: CHOICE text id out of range")
                validate_target(target, script_size, instruction_offsets, lineno, "CHOICE")
                out.extend(struct.pack("<HH", text_id, target))
                i += 1
            continue

        if op == "GOTO":
            target = resolve_target(parts[1], labels)
            validate_target(target, script_size, instruction_offsets, lineno, "GOTO")
            out.append(OP_GOTO)
            out.extend(struct.pack("<H", target))
            continue

        if op == "CALL":
            target = resolve_target(parts[1], labels)
            validate_target(target, script_size, instruction_offsets, lineno, "CALL")
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

    if len(out) > MAX_SCRIPT_SIZE:
        raise CompileError(f"script size exceeds VM limit {MAX_SCRIPT_SIZE} bytes")
    validate_control_flow(labels, insns, script_size, instruction_offsets)
    return bytes(out)


def load_resource_symbols(path):
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise CompileError(f"resource map not found: {path}")
    except (OSError, UnicodeError) as exc:
        raise CompileError(f"failed reading resource map: {exc}")
    except json.JSONDecodeError as exc:
        raise CompileError(f"invalid resource map JSON at line {exc.lineno} column {exc.colno}")
    if not isinstance(data, dict):
        raise CompileError("resource map must be a JSON object")
    symbols = data.get("symbols", data)
    if not isinstance(symbols, dict):
        raise CompileError("resource map symbols must be a JSON object")
    out = {}
    for name, value in symbols.items():
        if not isinstance(name, str) or not name:
            raise CompileError("resource symbol names must be non-empty strings")
        if not isinstance(value, int) or isinstance(value, bool) or not (0 <= value <= 0xFFFF):
            raise CompileError(f"resource symbol id out of range: {name}")
        out[name] = value
    return out


def main():
    parser = argparse.ArgumentParser(description="Compile .vns.txt to .vns.bin")
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--resource-map", default=None, type=pathlib.Path)
    args = parser.parse_args()

    src = args.input.read_text(encoding="utf-8")
    labels, insns = parse_source(src)
    resources = {}
    if args.resource_map is not None:
        resources = load_resource_symbols(args.resource_map)
    try:
        payload = encode(labels, insns, resources, strict_resources=args.resource_map is not None)
    except ValueError as exc:
        raise CompileError(str(exc))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    print(f"[scriptc] wrote {args.output} ({len(payload)} bytes)")


if __name__ == "__main__":
    try:
        main()
    except CompileError as exc:
        print(f"compile error: {exc}", file=sys.stderr)
        sys.exit(2)
