"""Adapt the existing raw CUDA address ABI to typed device functions."""

import json
from pathlib import Path
import sys


TYPES = {
    "i8": "char", "i32": "int", "i64": "long", "u8": "uchar",
    "u16": "ushort", "u32": "uint", "u64": "ulong", "f32": "float", "f64": "double",
}
SPACES = {"shared": "__local", "global": "__global"}


def bridge(entry):
    name, parameters, arguments = entry
    if not name.isidentifier() or any(not parameter.isidentifier() for parameter in parameters):
        raise ValueError(f"Invalid CUDA memory ABI symbol {name}")
    operands = []
    for argument in arguments:
        if len(argument) == 1:
            parameter, = argument
            if parameter not in parameters:
                raise ValueError(f"Unknown parameter {parameter} for {name}")
            operands.append(f"({parameter})")
            continue
        space, element, immutable, base, offset = argument
        if base not in parameters or offset is not None and offset not in parameters:
            raise ValueError(f"Invalid address operands for {name}")
        pointer = f"{SPACES[space]}{' const' if immutable else ''} {TYPES[element]} *"
        address = f"({base})" if offset is None else f"({base}) + ({offset})"
        operands.append(f"(({pointer})({address}))")
    return f"#define {name}({', '.join(parameters)}) fg_memory_{name}({', '.join(operands)})"


def main():
    source, output = map(Path, sys.argv[1:])
    output.write_text("\n".join(map(bridge, json.loads(source.read_text()))) + "\n")


if __name__ == "__main__":
    main()
