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
        elif operation == "unless_frozen":
            yield "#ifndef GOOSE_CUDA_FROZEN_ABI"
            yield from section(args)
            yield "#endif"
        elif operation == "mmq":
            yield "#ifdef GOOSE_CUDA_MMQ_IMAGE"
            yield from section(args)
            yield "#endif"
        elif operation == "contract":
            enabled, = args
            yield f"#pragma OPENCL FP_CONTRACT {'ON' if enabled else 'OFF'}"
        elif operation == "packet":
            name, kind, count = args
            yield f"typedef struct {{ {type_name(kind)} words[{count}]; }} {name};"
        elif operation == "packet_word":
            name, address_type, element_type = args
            a, element = type_name(address_type), type_name(element_type)
            yield f"static {element} {name}({a} packet, int64_t index) {{"
            yield f"    return ((const {element} *)packet)[index];"
            yield "}"
        elif operation == "fail_code":
            name, = args
            yield f"static void {name}(int64_t code) {{ (void)code; device_trap(); }}"
        elif operation == "collective":
            name, instruction, clobber = args
            if instruction not in ("bar.red.popc.u32 %0, 0, active", "vote.sync.ballot.b32 %0, active, 0xffffffff"):
                raise ValueError(f"Unsupported CUDA collective for {name}")
            memory = ' : "memory"' if clobber else ""
            yield f"static uint32_t {name}(uint32_t seen) {{"
            yield "    uint32_t result;"
            yield ('    __asm__ volatile("{ .reg .pred active; setp.ne.u32 active, %1, 0; '
                   f'{instruction}; }}" : "=r"(result) : "r"(seen){memory});')
            yield "}"
        elif operation == "copy_async16":
            name, = args
            yield f"static void {name}(uint64_t dst, uint64_t src) {{"
            yield ('    __asm__ volatile("cp.async.cg.shared.global [%0], [%1], 16;"'
                   ' :: "r"((uint32_t)dst), "l"(src) : "memory");')
            yield "}"
        elif operation == "mla_mma":
            yield "#ifdef GOOSE_CUDA_MMQ_IMAGE"
            arguments = "acc,a0,a1,a2,a3,b0,b1"
            lines = [
                f"#define fg_mla_mma_f16({arguments}) ({{",
                "__typeof__(acc) d = (acc);",
                '__asm__ volatile("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 '
                '{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"',
                ': "+f"(d.a_g), "+f"(d.b_g), "+f"(d.c_g), "+f"(d.d_g)',
                ': "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(b0), "r"(b1)); d; })',
            ]
            yield (" " + chr(92) + "\n").join(lines)
            yield "#else"
            yield f"#define fg_mla_mma_f16({arguments}) (acc)"
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
        elif operation == "certified_arithmetic":
            yield "#undef gs_shl_u64"
            yield "#define gs_add_u32(a,b) ((a)+(b))"
            yield "#define gs_shl_u64(a,b) ((a)<<(b))"
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
        if Path(name).name != name or not name.endswith((".h", ".cl")):
            raise ValueError(f"Invalid CUDA module name {name}")
        (output / name).write_text("\n".join(section(commands)) + "\n")


if __name__ == "__main__":
    main()
