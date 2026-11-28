#!/usr/bin/env python3
"""Package a Fried Goose OpenCL fragment for the FrankenMoE kernel glue."""

import argparse
from pathlib import Path
import re


# The OpenCL glue references these source types by their legacy C ABI names.
SHARED_TYPES = {
    "i2-q8-packed": {"I2Q8Vec": "I2Q8Vec_g"},
    "i2-grouped-moe": {"I2MoePair": "I2MoePair_g", "I2MoeAcc": "I2MoeAcc_g"},
    "q4-attention": {"F2": "F2_g"},
    "rms-reference": {"RmsVec": "RmsVec_g"},
    "vector": {"CVVec4": "CVVec4_g"},
    "gdn-mma": {"F4": "F4_g", "U2": "U2_g", "U4": "U4_g"},
    "flash-mma": {"F4": "Flash_F4_g", "U2": "Flash_U2_g", "U4": "Flash_U4_g"},
    "flash-vec": {"VecFlash4": "VecFlash4_g"},
    "flash-stage": {"F2": "F2_g"},
    "mmvq-pack": {"MmvqFastDiv": "MmvqFastDiv_g"},
    "dense-mma": {name: name + "_g" for name in ("DenseDot", "DenseParts")},
    "mmq": {name: name + "_g" for name in (
        "MmqDot", "MmqSum", "MmqWords", "MmqScales", "MmqQ6Scales",
        "MmqPairSum", "MmqPartition", "MmqOperand",
    )},
}
EXTERNAL_TYPES = {"flash-stage", "mmvq-pack", "dense-mma", "mmq", "vector"}
TYPE_MARKER = re.compile(r'/\* FG_SOURCE TYPE (\w+) "\S+" \d+ "([^"]*)" \*/')
FIELD_MARKER = re.compile(r'/\* FG_SOURCE FIELD (\w+)\.(f\d+) "\S+" \d+ "([^"]*)" \*/')
PRIVATE_NAME = re.compile(r"gg_(?:function|type|unaligned|frame)_\w+")
CONSTANT_MARKER = re.compile(r"/\* FG_DEVICE_CONSTANT (v\d+) (u?int(?:8|16|32|64)_t) (\d+) \*/")


def source_name(spelling):
    return bytes(int(octal, 8) for octal in re.findall(r"\\([0-7]{3})", spelling)).decode()


def fragments(module, source):
    types = source.split("/* ---- types ---- */\n", 1)[1].split("/* ---- data ---- */", 1)[0]
    body = source.split("} gg_globals;\n", 1)[1].split("/* ---- host ---- */", 1)[0]
    if re.search(r"\b(?:gs_sp|gg_get_stack|gg_nonlocal_frame)\b", body):
        raise ValueError(f"{module} uses the host stack ABI")

    shared = SHARED_TYPES.get(module, {})
    names = {
        native: shared[name]
        for native, spelling in TYPE_MARKER.findall(types)
        if (name := source_name(spelling)) in shared
    }
    if set(names.values()) != set(shared.values()) or len(names) != len(shared):
        missing = sorted(set(shared.values()) - set(names.values()))
        raise ValueError(f"{module} has missing or ambiguous ABI types (missing: {missing})")
    fields = {
        field: source_name(spelling) + "_g"
        for native, field, spelling in FIELD_MARKER.findall(types)
        if native in names
    }
    renames = names | fields
    prefix = module.replace("-", "_")
    constants = {
        name: f"(({ctype})UINT64_C({bits}))"
        for name, ctype, bits in CONSTANT_MARKER.findall(source)
    }

    def substitute_constant(match):
        name = match[1]
        if name not in constants:
            raise ValueError(f"{module} refers to unsupported device global {name}")
        return constants[name]

    body = re.sub(r"\bgg_globals\.(v\d+)\b", substitute_constant, body)
    if re.search(r"\bgg_globals\b", body):
        raise ValueError(f"{module} uses the host globals object")

    if module in EXTERNAL_TYPES:
        for native in names:
            types = re.sub(rf"(?m)^typedef struct {native} {native};\n", "", types)
            types, count = re.subn(
                rf"#pragma pack\(push, 1\)\nstruct {native} \{{.*?\n\}};\n#pragma pack\(pop\)\n",
                "", types, flags=re.S,
            )
            if count != 1:
                raise ValueError(f"{module} has no unique layout for {native}")

    def qualify(text):
        def replace(match):
            identifier = match[0]
            if identifier in renames:
                return renames[identifier]
            if PRIVATE_NAME.fullmatch(identifier):
                return f"{prefix}_{identifier}"
            return identifier

        text = re.sub(r"(?m)^/\* FG_SOURCE[^\n]*\n", "", text)
        return re.sub(r"\b\w+\b", replace, text)

    guard = f"FG_DEVICE_TYPES_{prefix.upper()}"
    types = f"#ifndef {guard}\n#define {guard}\n{qualify(types)}\n#endif\n"
    body = f'#include "{module}-types.h"\n' + qualify(body)
    return types, body


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("module")
    args = parser.parse_args()
    source = (args.output / f"{args.module}.c").read_text()
    types, body = fragments(args.module, source)
    (args.output / f"{args.module}-types.h").write_text(types)
    (args.output / f"{args.module}-device.h").write_text(body)


if __name__ == "__main__":
    main()
