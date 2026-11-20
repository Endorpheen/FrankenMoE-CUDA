#!/usr/bin/env python3
"""Materialize the qualified CUDA headers from the retained Goose compiler output."""

import argparse
import hashlib
from pathlib import Path
import re


MODULES = {
    "mma-iter": (
        "fg175mi_", ("FG175I_ITER",), "9f65119d376c0ac005c27a7dacbb4a4a7580e4e2e9fb8e18dff8893689080643",
    ),
    "mma-process": (
        "fg175mp_", ("FG175P_PROCESS",), "687a01b1b953ce4d45073491ff40aa9d4bfe06b0a8312c236cf8577b88045428",
    ),
    "value-prefill": (
        "fg175cv_", ("FG175C_PROCESS",), "7203f80ab364d90f1b81d55a1e7ee32f316cca10b19844b6ed983c4a26141d17",
    ),
    "vector": (
        "fg175vd_", ("FG175V_VEC", "FG175V_COMBINE"), "6468c815385dd42a2a0755aeae24ed7b29e6ab6eddeb208a85341f6d76f3bcc0",
    ),
}
HELPERS = ("gg_view", "gg_double_bits", "gg_index", "gg_div", "gg_mod", "gg_udiv", "gg_umod")
FUNCTION = re.compile(
    r"(?m)^static [^\n{]+? (gg_function_\d+|gg_view|gg_double_bits|gg_index|gg_div|gg_mod|gg_udiv|gg_umod)"
    r"\([^;{}]*\)\s*\{"
)


def function_body(text, match):
    depth = 1
    for cursor in range(match.end(), len(text)):
        depth += (text[cursor] == "{") - (text[cursor] == "}")
        if depth == 0:
            return text[match.start():cursor + 1]
    raise ValueError(f"Missing closing brace in {match[1]}")


def render(source, module):
    prefix, macros, _ = MODULES[module]
    main = source.rfind("static void gg_function_0(")
    calls = [name for name in re.findall(r"\b(gg_function_\d+)\(", source[main:]) if name != "gg_function_0"]
    if main < 0 or len(calls) != len(macros):
        raise ValueError(f"Unexpected number of Goose entries in {module}")
    trap = 'static void gg_fail(const char*){asm("trap;");}'
    emitted = [(match[1], function_body(source, match)) for match in FUNCTION.finditer(source)
               if match[1] != "gg_function_0"]
    if module == "vector":
        helpers = [trap] + [next(body for name, body in emitted if name == helper) for helper in HELPERS]
        functions = [body for name, body in emitted if name.startswith("gg_function_")]
    else:
        helpers = []
        functions = [trap] + [body for _, body in emitted]
    declarations = [body.split("{", 1)[0].rstrip() + ";" for body in functions]
    text = "\n".join(helpers + declarations + functions)
    text = re.sub(r"\b(gg_|ggf_|gs_)", lambda match: prefix + match[1], text)
    text = re.sub(r"(?m)^static ", "static __device__ __forceinline__ ", text)
    if any(call in text for call in ("gg_get_stack(", "gg_allocate(", "gg_append(", "gg_ref(")):
        raise ValueError(f"Host-only calls in {module} device code")
    entries = "".join(f"#define {macro} {prefix}{call}\n" for macro, call in zip(macros, calls))
    return f"#pragma once\n{text}\n{entries}"


def materialize(source_dir, output_dir, modules=MODULES):
    outputs = {}
    for module in modules:
        _, _, expected = MODULES[module]
        generated = render((source_dir / f"{module}-device-generated.c").read_text(), module).encode()
        actual = hashlib.sha256(generated).hexdigest()
        if actual != expected:
            raise ValueError(f"{module} header checksum {actual} differs from {expected}")
        outputs[f"{module}-functions-generated.cuh"] = generated

    output_dir.mkdir(parents=True, exist_ok=True)
    for name, data in outputs.items():
        path = output_dir / name
        if path.exists():
            if path.read_bytes() != data:
                raise FileExistsError(f"Existing header differs: {path}")
        else:
            with path.open("xb") as output:
                output.write(data)
    return outputs


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    for name in materialize(Path(__file__).resolve().parent / "source", args.output_dir):
        print(args.output_dir / name)
