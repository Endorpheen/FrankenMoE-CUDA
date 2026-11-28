"""Compose generated CUDA declarations and adapt vector register layouts."""

import json
from pathlib import Path
import sys


TYPES = {"u32": "uint32_t", "u64": "uint64_t", "i32": "int32_t",
         "i64": "int64_t", "f32": "float", "void": "void"}


def type_name(name):
    return TYPES.get(name, name)


def adapted_vector(load, store, result, fields, index):
    count = len(fields)
    vector = f"float{count}"
    lanes = "xyzw"[:count]
    parameters = "uint64_t address"
    if index == "elements":
        parameters += ", int32_t index"
        read = f"((__global const {vector} *)address)[index]"
        write = f"((__global {vector} *)address)[index]"
    else:
        if index == "scalars":
            parameters += ", uint64_t index"
            address = "(address + 4 * index)"
        elif index == "address":
            address = "address"
        else:
            raise ValueError(f"Unknown CUDA vector indexing mode {index}")
        read = f"*(__global const {vector} *){address}"
        write = f"*(__global {vector} *){address}"
    values = ", ".join(f"value.{lane}" for lane in lanes)
    yield f"static {result} {load}({parameters}) {{"
    yield f"    const {vector} value = {read};"
    yield f"    return ({result}){{{values}}};"
    yield "}"
    if store:
        values = ", ".join(f"value.{field}" for field in fields)
        yield f"static void {store}({parameters}, {result} value) {{"
        yield f"    {write} = ({vector})({values});"
        yield "}"


def section(commands):
    for command in commands:
        operation, *args = command
        if operation == "include":
            yield f'#include "{args[0]}"'
        elif operation == "frozen":
            yield "#ifdef GOOSE_CUDA_FROZEN_ABI"
            yield from section(args)
            yield "#endif"
        elif operation == "record":
            name, *fields = args
            members = " ".join(f"{type_name(kind)} {field};" for kind, field in fields)
            yield f"typedef struct {{ {members} }} {name};"
        elif operation == "vector":
            yield from adapted_vector(*args)
        elif operation == "copy16":
            (name,) = args
            yield f"static void {name}(uint64_t dst, uint64_t src) {{"
            yield "    *(__local uint4 *)dst = *(__global const uint4 *)src;"
            yield "}"
        elif operation == "forward":
            name, callee, parameters, forwarded, unused = args
            signature = ", ".join(f"{type_name(kind)} {param}" for kind, param in parameters)
            yield f"static void {name}({signature}) {{"
            for param in unused:
                yield f"    (void){param};"
            yield f"    {callee}({', '.join(forwarded)});"
            yield "}"
        elif operation == "register_view":
            name, callee, source, result, fields = args
            yield f"static {result} {name}(uint64_t address) {{"
            yield f"    const {source} value = {callee}(address);"
            yield f"    return ({result}){{{', '.join(f'value.{field}' for field in fields)}}};"
            yield "}"
        elif operation == "alias":
            name, target = args
            yield f"#define {name} {target}"
        elif operation == "undef":
            yield f"#undef {args[0]}"
        elif operation == "pack_width":
            yield "#define gs_add_u32(a, b) ((uint32_t)((a) + (b)))"
        elif operation == "signed_shift":
            yield "#define gs_shl_i32(a, b) as_int((uint32_t)(a) << ((b) & 31))"
        elif operation == "noinline":
            yield "#define GS_NOINLINE __attribute__((noinline))"
        else:
            raise ValueError(f"Unknown CUDA module operation {operation}")


def main():
    source, output = map(Path, sys.argv[1:])
    for name, commands in json.loads(source.read_text()).items():
        if Path(name).name != name or not name.endswith(".h"):
            raise ValueError(f"Invalid CUDA module name {name}")
        (output / name).write_text("\n".join(section(commands)) + "\n")


if __name__ == "__main__":
    main()
