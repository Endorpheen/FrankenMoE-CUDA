"""Generate fixed-signature CUDA intrinsics from a PTX ABI description."""

import json
from pathlib import Path
import re
import sys


def declaration(kernel):
    name, result, params, instruction, results, registers, volatile, barrier = kernel
    if not re.fullmatch(r"[a-z][\w.]*[\s\[\],%0-9a-z.\-]*;", instruction):
        raise ValueError(f"Invalid instruction for {name}")
    signature = ", ".join(f"{kind} {param}" for kind, param in params) or "void"
    output = f'"{results[0]}"(result)' if results else ""
    inputs = ", ".join(f'"{register}"({param})' for register, param in registers)
    clobber = ' : "memory"' if barrier else ""
    effect = " volatile" if volatile else ""
    lines = [f"static {result} {name}({signature}) {{"]
    if results:
        lines.append(f"    {result} result;")
    operands = f" : {output} : {inputs}" if results or inputs or barrier else ""
    lines.append(f'    __asm__{effect}("{instruction}"{operands}{clobber});')
    if results:
        lines.append("    return result;")
    return "\n".join([*lines, "}"])


def main():
    manifest, output = map(Path, sys.argv[1:])
    for header, entries in json.loads(manifest.read_text()).items():
        if Path(header).name != header or not header.endswith(".h"):
            raise ValueError(f"Invalid CUDA primitive header {header}")
        (output / header).write_text("\n\n".join(map(declaration, entries)) + "\n")


if __name__ == "__main__":
    main()
