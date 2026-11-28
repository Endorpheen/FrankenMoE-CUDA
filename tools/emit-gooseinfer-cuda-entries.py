"""Emit CUDA entry points for Goose device functions from their launch ABI."""

import json
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
entries = json.loads(source.read_text())
registers = {"block_x": "ctaid.x", "block_y": "ctaid.y", "block_z": "ctaid.z",
             "thread_x": "tid.x", "grid_x": "nctaid.x"}

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
        if entry.get("nonfrozen"):
            print("#ifndef GOOSE_CUDA_FROZEN_ABI", file=stream)
        parameters = ", ".join(entry["parameters"])
        print(f'__kernel void {entry["name"]}({parameters}) {{', file=stream)
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
        arguments = ", ".join("(uint64_t)" + arg[1:] if arg.startswith("@") else arg
                              for arg in entry["arguments"])
        print(f'    {entry["function"]}({arguments});\n}}', file=stream)
        if entry.get("nonfrozen"):
            print("#endif", file=stream)
