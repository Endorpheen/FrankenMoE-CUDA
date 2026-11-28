"""Emit CUDA entry points for Goose device functions from their launch ABI."""

import json
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
entries = json.loads(source.read_text())
registers = {"block_x": "ctaid.x", "block_y": "ctaid.y", "block_z": "ctaid.z",
             "thread_x": "tid.x", "block_width": "ntid.x", "grid_x": "nctaid.x", "grid_y": "nctaid.y"}

def argument(value):
    if isinstance(value, str):
        return "(uint64_t)" + value[1:] if value.startswith("@") else value
    (operation, operands), = value.items()
    if operation == "signed_address":
        return f"(int64_t){operands}"
    if operation == "nonzero":
        return f"({operands} != 0)"
    if operation in ("linear", "linear_u64"):
        group, width, lane = operands
        cast = "uint64_t" if operation == "linear_u64" else "int64_t"
        return f"(({cast}){group} * {width} + {argument(lane)})"
    if operation in ("div", "rem"):
        index, width = operands
        operator = "/" if operation == "div" else "%"
        return f"({index} {operator} {width})"
    raise ValueError(f"Unsupported CUDA argument operation {operation}")


with output.open("w") as stream:
    print("/* Generated from the GooseInfer CUDA launch ABI. */", file=stream)
    dynamic_symbols = {}
    for entry in entries:
        for _, symbol, alignment in entry.get("dynamic_shared", []):
            if dynamic_symbols.setdefault(symbol, alignment) != alignment:
                raise ValueError(f"Conflicting alignment for {symbol}")
    for symbol, alignment in dynamic_symbols.items():
        print(f'__asm__(".extern .shared .align {alignment} .b8 {symbol}[];");', file=stream)
    for entry in entries:
        condition = entry.get("condition")
        if entry.get("nonfrozen"):
            condition = "!GOOSE_CUDA_FROZEN_ABI"
        elif entry.get("frozen"):
            condition = "GOOSE_CUDA_FROZEN_ABI"
        if condition:
            if condition.startswith("!"):
                print(f"#ifndef {condition[1:]}", file=stream)
            else:
                print(f"#ifdef {condition}", file=stream)
        parameters = ", ".join(entry["parameters"])
        workgroup = entry.get("workgroup")
        qualifier = "__kernel"
        if workgroup:
            shape = ", ".join(map(str, workgroup))
            qualifier += f" __attribute__((reqd_work_group_size({shape})))"
        print(f'{qualifier} void {entry["name"]}({parameters}) {{', file=stream)
        for kind, name, length, align in entry.get("shared", []):
            extent = f"[{length}]" if length else ""
            alignment = f" __attribute__((aligned({align})))" if align else ""
            print(f"    __local {kind} {name}{extent}{alignment};", file=stream)
        for name, symbol, _ in entry.get("dynamic_shared", []):
            print(f"    uint64_t {name};", file=stream)
            print(f'    __asm__("mov.u64 %0, {symbol};" : "=l"({name}));', file=stream)
        for name, register in entry.get("registers", {}).items():
            print(f"    uint32_t {name};", file=stream)
            print(f'    __asm__("mov.u32 %0, %%{registers[register]};" : "=r"({name}));', file=stream)
        for name, array in entry.get("shared_addresses", {}).items():
            print(f"    uint64_t {name};", file=stream)
            print(f'    __asm__("cvta.shared.u64 %0, %1;" : "=l"({name}) : "l"((uint64_t){array}));',
                  file=stream)
        arguments = ", ".join(argument(arg) for arg in entry["arguments"])
        print(f'    {entry["function"]}({arguments});\n}}', file=stream)
        if condition:
            print("#endif", file=stream)
