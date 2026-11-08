#!/usr/bin/env python3
"""Mechanical Goose->CUDA device adapter for worker-10.

The frozen Goose emitter produces self-contained C functions.  The entry
functions and their reachable helpers use only scalar Goose arithmetic and the
fg153_am_* raw primitives, so the adapter can mechanically add CUDA device
qualifiers and emit the exact generated bodies.  It refuses to invent ids:
the generated entry id for every operation is read from the witness marker in
the real generated translation unit.
"""
from __future__ import annotations
import argparse, hashlib, json, re, sys
from pathlib import Path

FUNC_RE = re.compile(r'static\s+([A-Za-z_][A-Za-z0-9_]*)\s+gg_function_(\d+)\s*\(([^)]*)\)\s*\{')
CALL_RE = re.compile(r'\bgg_function_(\d+)\s*\(')
THREAD_RE = re.compile(r'\bfg153_am_(?:gid_x|bid_x|bid_y|bid_z|tid_x|tid_y|bdim_x|gdim_y|gdim_z)\s*\(')
FORBIDDEN = ["gg_get_stack", "gg_allocate", "gg_append", "gg_slice", "gg_header",
             "gg_reference", "gg_text", "gg_integer", "gg_float", "gg_write",
             "printf", "malloc", "fopen"]
MARKERS = {
    "fg153_am_enter_copy_scalar": "copy_scalar",
    "fg153_am_enter_copy_contig": "copy_contig",
    "fg153_am_enter_copy_blocks": "copy_blocks",
    "fg153_am_enter_transpose": "transpose",
    "fg153_am_enter_getrows": "getrows",
    "fg153_am_enter_setrows": "setrows",
    "fg153_am_enter_q4_dequant": "q4_dequant",
}
REQUIRED = {
    "copy_scalar": ("device", "serial"),
    "copy_contig": ("device", "serial"),
    "copy_blocks": ("device", "serial"),
    "transpose": ("device", "serial"),
    "getrows": ("device", "serial"),
    "setrows": ("device", "serial"),
    "q4_dequant": ("device",),
}
ID_MACRO = {
    "copy_scalar": "FG153_AM_FN_COPY_SCALAR",
    "copy_contig": "FG153_AM_FN_COPY_CONTIG",
    "copy_blocks": "FG153_AM_FN_COPY_BLOCKS",
    "transpose": "FG153_AM_FN_TRANSPOSE",
    "getrows": "FG153_AM_FN_GETROWS",
    "setrows": "FG153_AM_FN_SETROWS",
    "q4_dequant": "FG153_AM_FN_Q4_DEQUANT",
}


def skip_literal(text: str, i: int) -> int:
    q = text[i]
    i += 1
    while i < len(text):
        if text[i] == "\\":
            i += 2
            continue
        if text[i] == q:
            return i + 1
        i += 1
    raise ValueError("unterminated literal")


def parse_defs(text: str):
    defs = {}
    for m in FUNC_RE.finditer(text):
        ret, fid, args = m.group(1), int(m.group(2)), m.group(3).strip()
        i = m.end() - 1
        depth = 0
        start = i
        while i < len(text):
            c = text[i]
            if c in "\"'":
                i = skip_literal(text, i)
                continue
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    i += 1
                    break
            i += 1
        body = text[start:i]
        if fid in defs:
            raise SystemExit(f"duplicate generated function id {fid}")
        defs[fid] = {"ret": ret, "args": args, "body": body, "start": m.start(), "end": i}
    return defs


def calls(body: str):
    return sorted({int(x) for x in CALL_RE.findall(body) if int(x) in DEFS})


def closure(roots):
    seen = set()
    stack = list(roots)
    while stack:
        f = stack.pop()
        if f in seen:
            continue
        if f not in DEFS:
            raise SystemExit(f"generated helper {f} missing from translation unit")
        seen.add(f)
        stack.extend(calls(DEFS[f]["body"]))
    return seen


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--generated", required=True)
    ap.add_argument("--cuh", required=True)
    ap.add_argument("--ids", required=True)
    ap.add_argument("--report", required=True)
    ap.add_argument("--consts", required=True)
    args = ap.parse_args(argv)

    text = Path(args.generated).read_text(encoding="utf-8", errors="strict")
    global DEFS
    DEFS = parse_defs(text)
    if not DEFS:
        raise SystemExit("no gg_function definitions found")

    found = {}
    evidence = {}
    for fid, d in DEFS.items():
        for marker, kind in MARKERS.items():
            if re.search(r'\b' + re.escape(marker) + r'\s*\(\s*\)\s*;', d["body"]):
                cl = closure([fid])
                is_device = any(THREAD_RE.search(DEFS[x]["body"]) for x in cl)
                flavour = "device" if is_device else "serial"
                key = (kind, flavour)
                if key in found:
                    raise SystemExit(f"ambiguous entry for {key}: {found[key]} and {fid}")
                found[key] = fid
                evidence[f"{kind}:{flavour}"] = {
                    "id": fid, "ret": d["ret"], "args": d["args"],
                    "closure": sorted(cl), "bytes": len(d["body"]),
                }

    for kind, flavours in REQUIRED.items():
        for flavour in flavours:
            if (kind, flavour) not in found:
                raise SystemExit(f"generated entry missing: {kind}:{flavour}")
    for kind, flavours in REQUIRED.items():
        for flavour in flavours:
            cl = closure([found[(kind, flavour)]])
            for f in cl:
                body = DEFS[f]["body"]
                for bad in FORBIDDEN:
                    if bad in body:
                        raise SystemExit(f"generated device closure reaches runtime symbol {bad} in gg_function_{f}")

    device_roots = [found[(kind, "device")] for kind in REQUIRED]
    serial_roots = [found[(kind, "serial")] for kind in REQUIRED if (kind, "serial") in found]
    device_set = closure(device_roots)
    needed = device_set | set(serial_roots)

    out = []
    out.append("/* Generated by apex-move-adapt.py from the real pinned Goose emit. */\n")
    out.append("#pragma once\n")
    out.append("#include <stdint.h>\n\n")
    if any("gg_globals" in DEFS[f]["body"] for f in device_set):
        vals = {int(m.group(1)): int(m.group(2)) for m in
                re.finditer(r'gg_globals\.v(\d+)\s*=\s*\(\(int64_t\)\(\(\(int64_t\)UINT64_C\((\d+)\)\)\)\);', text)}
        if not vals:
            raise SystemExit("device closure uses gg_globals but no constant initializer was found")
        out.append("static __device__ const struct {\n")
        out.append("    unsigned char dummy;\n")
        for i in range(max(vals) + 1):
            if i not in vals:
                raise SystemExit(f"missing generated constant gg_globals.v{i}")
            out.append(f"    int64_t v{i};\n")
        out.append("} gg_globals = { 0")
        for i in range(max(vals) + 1):
            out.append(f", {vals[i]}")
        out.append(" };\n\n")
    out.append("/* Exact reachable generated function ids are in apex-move-entry-ids.h. */\n")
    for fid in sorted(device_set):
        d = DEFS[fid]
        out.append(f"static __device__ __forceinline__ {d['ret']} gg_function_{fid}({d['args']});\n")
    out.append("\n")
    for fid in sorted(device_set):
        d = DEFS[fid]
        out.append(f"static __device__ __forceinline__ {d['ret']} gg_function_{fid}({d['args']}) {d['body']}\n")
    Path(args.cuh).write_text("".join(out), encoding="utf-8")

    ids = ["/* Generated entry ids; never guessed. */", "#pragma once"]
    for kind in REQUIRED:
        for flavour in REQUIRED[kind]:
            ids.append(f"#define {ID_MACRO[kind]}_{flavour.upper()} {found[(kind, flavour)]}")
    ids.append(f"#define FG153_AM_FN_DEVICE_COUNT {len(device_roots)}")
    ids.append(f"#define FG153_AM_FN_REACHABLE_COUNT {len(needed)}")
    Path(args.ids).write_text("\n".join(ids) + "\n", encoding="utf-8")

    const_lines = ["/* Generated constant initialization for the host generated TU. */",
                   "#pragma once",
                   "static void fg153_am_init_globals(void) {"]
    if "gg_globals" in text:
        vals_all = {int(m.group(1)): int(m.group(2)) for m in
                    re.finditer(r'gg_globals\.v(\d+)\s*=\s*\(\(int64_t\)\(\(\(int64_t\)UINT64_C\((\d+)\)\)\)\);', text)}
        for i in sorted(vals_all):
            const_lines.append(f"    gg_globals.v{i} = (int64_t) UINT64_C({vals_all[i]});")
    const_lines.append("}")
    Path(args.consts).write_text("\n".join(const_lines) + "\n", encoding="utf-8")

    report = {
        "status": "PARSED_FROM_REAL_EMIT",
        "generated": str(Path(args.generated)),
        "generated_sha256": hashlib.sha256(text.encode()).hexdigest(),
        "definitions": len(DEFS),
        "entries": evidence,
        "device_closure": sorted(device_set),
        "device_reachable_functions": len(device_set),
        "no_forbidden_runtime_symbols": True,
        "device_constants": sorted({int(m.group(1)) for m in re.finditer(r'gg_globals\.v(\d+)', text)}) if 'gg_globals' in text else [],
        "note": "Device flavor is the entry whose closure reaches thread/block index primitives.",
    }
    Path(args.report).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "entries": {k: v["id"] for k, v in evidence.items()},
                      "device_functions": len(device_set)}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
