#!/usr/bin/env python3
"""Emit C ABI metadata from the GooseInfer quantization type table."""

import json
from pathlib import Path
import re
import sys


def read_array(text, name):
    match = re.search(rf"^const {name}: [^\n]+? = \[\n(.*?)^\];", text, re.M | re.S)
    if not match:
        raise ValueError(f"Missing Goose type array {name}")
    rows = re.sub(r"//[^\n]*", "", match[1]).strip().rstrip(",")
    return json.loads("[" + rows + "]")


def write_operations(source, output):
    text = source.read_text()
    with output.open("w") as header:
        for label, limit in (("GGML_OP_NAME", "GGML_OP_COUNT"),
                             ("GGML_UNARY_OP_NAME", "GGML_UNARY_OP_COUNT"),
                             ("GGML_GLU_OP_NAME", "GGML_GLU_OP_COUNT")):
            entries = read_array(text, label)
            if any(not isinstance(e, str) for e in entries):
                raise ValueError(f"Non-string operation name in {label}")
            print(f"_Static_assert({limit} == {len(entries)}, \"Goose operation name count\");", file=header)
            print(f"static const char * {label}[{limit}] = {{", file=header)
            for name in entries:
                print("    " + json.dumps(name, ensure_ascii=True) + ",", file=header)
            print("};", file=header)


def write_goose_metadata(types, ops, output):
    def emit_array(file, name, type_name, rows):
        print(f"const {name}: {type_name}[{len(rows)}] = [", file=file)
        for row in rows:
            value = json.dumps(row, ensure_ascii=True)[:-1] + r'\x00"' if isinstance(row, str) else str(row)
            print(f"    {value},", file=file)
        print("];", file=file)

    types_text = types.read_text()
    ops_text = ops.read_text()
    fields = [
        ("GGML_META_TYPE_NAME", "const u8[:]", read_array(types_text, "AB_NAME"), 43),
        ("GGML_META_BLOCK", "i64", read_array(types_text, "AB_BLCK"), 43),
        ("GGML_META_SIZE", "u64", read_array(types_text, "AB_SIZE"), 43),
        ("GGML_META_QUANTIZED", "i64", read_array(types_text, "AB_ISQ"), 43),
        ("GGML_META_OP_NAME", "const u8[:]", read_array(ops_text, "GGML_OP_NAME"), 101),
        ("GGML_META_UNARY_NAME", "const u8[:]", read_array(ops_text, "GGML_UNARY_OP_NAME"), 22),
        ("GGML_META_GLU_NAME", "const u8[:]", read_array(ops_text, "GGML_GLU_OP_NAME"), 7),
    ]
    with output.open("w") as file:
        print("const GGML_META_TYPE_COUNT: i64 = 43;", file=file)
        for name, type_name, values, expected in fields:
            if len(values) != expected:
                raise ValueError(f"{name}: expected {expected} entries")
            if type_name == "const u8[:]":
                if any(not isinstance(value, str) or not value.isascii() or "\x00" in value for value in values):
                    raise ValueError(f"{name}: expected ASCII names without embedded NULs")
            elif any(type(value) is not int or value < 0 for value in values):
                raise ValueError(f"{name}: invalid numeric metadata")
            emit_array(file, name, type_name, values)


if sys.argv[1] == "--goose":
    write_goose_metadata(*map(Path, sys.argv[2:5]))
    sys.exit(0)

source, output = map(Path, sys.argv[1:3])
if len(sys.argv) == 5:
    write_operations(Path(sys.argv[3]), Path(sys.argv[4]))
text = source.read_text()
names, blocks, sizes, quantized = (
    read_array(text, field) for field in ("AB_NAME", "AB_BLCK", "AB_SIZE", "AB_ISQ")
)
if not len(names) == len(blocks) == len(sizes) == len(quantized):
    raise ValueError("Goose type metadata arrays have different lengths")

with output.open("w") as header:
    print(f"_Static_assert(GGML_TYPE_COUNT == {len(names)}, \"Goose type metadata count\");", file=header)
    print("static const struct ggml_type_traits type_traits[GGML_TYPE_COUNT] = {", file=header)
    for index, (name, block, size, quant) in enumerate(zip(names, blocks, sizes, quantized)):
        if not isinstance(name, str) or any(type(v) is not int or v < 0 for v in (block, size, quant)) or quant not in (0, 1):
            raise ValueError(f"Invalid Goose type metadata at {index}")
        label = json.dumps(name, ensure_ascii=True)
        print(f"    [{index}] = {{.type_name={label}, .blck_size={block}, .type_size={size}, .is_quantized={quant}}},",
              file=header)
    print("};", file=header)
