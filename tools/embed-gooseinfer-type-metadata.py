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
