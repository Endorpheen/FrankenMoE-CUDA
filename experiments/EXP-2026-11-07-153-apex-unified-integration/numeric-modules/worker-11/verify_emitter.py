#!/usr/bin/env python3
"""Verify the Goose emitter id used by the callable units. Never guess."""
import hashlib, json, re, sys
from pathlib import Path

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    gen = Path("apex_matrix_generated.c")
    if not gen.is_file():
        print("FAIL: generated C missing")
        return 2
    text = gen.read_text()
    decls = re.findall(r'^static ([A-Za-z0-9_ ]+?) gg_function_(\d+)\(([^;]*?)\);', text, re.M)
    ids = sorted({int(i) for _, i, _ in decls})
    matches = []
    for ret, i, params in decls:
        parts = [p.strip() for p in params.split(',')]
        nslice = sum(1 for p in parts if p.startswith('gg_slice '))
        nint = sum(1 for p in parts if p.startswith('int64_t '))
        if ret.strip() == 'int64_t' and nslice == 3 and nint == 6 and len(parts) == 9 and parts[-1] == 'int64_t base':
            matches.append(int(i))
    if matches != [1]:
        print(f"FAIL: expected the single 8-argument entry at gg_function_1, found {matches}")
        return 3
    unit = Path("apex_matrix_unit.c").read_text()
    dev = Path("apex_matrix_device.cu").read_text()
    fix = Path("apex_matrix_fixture.c").read_text()
    for name, src in (("unit", unit), ("device", dev), ("fixture", fix)):
        if "gg_function_1(" not in src:
            print(f"FAIL: {name} does not call gg_function_1")
            return 4
    out = {
        "status": "PARSED_FROM_GENERATED_C",
        "entry_id": 1,
        "entry_signature": "gg_function_1(int64_t mode, gg_slice w, gg_slice x, gg_slice y, int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base)",
        "definition_count": len(ids),
        "ids": ids,
        "generated_sha256": sha(gen),
        "unit_sha256": sha("apex_matrix_unit.c"),
        "device_sha256": sha("apex_matrix_device.cu"),
        "note": "The entry id is bound by exact arity/type shape and by the explicit gg_function_1 call in every wrapper.",
    }
    Path("emitter_ids.json").write_text(json.dumps(out, indent=2) + "\n")
    print(json.dumps({"status": out["status"], "entry_id": 1, "ids": len(ids)}))
    return 0

if __name__ == "__main__":
    sys.exit(main())
