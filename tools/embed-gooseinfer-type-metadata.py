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


source, output = map(Path, sys.argv[1:])
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
