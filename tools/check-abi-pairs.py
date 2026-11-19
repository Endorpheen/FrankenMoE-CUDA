#!/usr/bin/env python3
"""Compare the declaration of a cross-module function with its definition.

Why: `mt_fmt_g6` was declared and never defined, and the library that relied on it could not be loaded. That is
the extreme case. The commoner one is a declaration and a definition that agree on the name and disagree on a
parameter type - the link succeeds, the call reads garbage, and nothing reports it.

Each pair below is (consumer file, symbol, provider file). Both sides are C linkage or both C++, the parameter
lists must match after removing names, and the return type must match.
"""
import re
import sys
import pathlib

PAIRS = [
    ("runtime/reconstructed/src/llama-sampler.cpp", "fg386s_pick",
     "integration/modules/EXP-2026-11-11-386-goose-sampler-dist-pick-live/source/bridge.c"),
    ("runtime/reconstructed/src/llama-sampler.cpp", "fg386s_enabled",
     "integration/modules/EXP-2026-11-11-386-goose-sampler-dist-pick-live/source/bridge.c"),
]

def signature(path, symbol):
    text = pathlib.Path(path).read_text(errors="ignore")
    # a declaration or definition: return type, name, parameters, optional const qualifier
    m = re.search(r"([A-Za-z_][\w\s\*]*?)\b" + re.escape(symbol) + r"\s*\(([^;{)]*)\)", text)
    if not m:
        return None
    ret = m.group(1).strip()
    ret = re.sub(r"\b(extern|static|inline|const)\b", " ", ret)
    ret = " ".join(ret.split())
    params = m.group(2)
    out = []
    for i, p in enumerate(params.split(",")):
        p = p.strip()
        if not p or p == "void":
            continue
        p = re.sub(r"\b(const|volatile)\b", " ", p)
        p = re.sub(r"\b[A-Za-z_]\w*\s*$", "", p).strip()   # drop the parameter name
        p = re.sub(r"\s+", " ", p).strip()
        if p.endswith("*"):
            p = p[:-1].strip() + "*"
        out.append(p)
    return ret, out

def main():
    bad = 0
    for consumer, symbol, provider in PAIRS:
        a = signature(consumer, symbol)
        b = signature(provider, symbol)
        if a is None:
            print(f"check-abi-pairs: {symbol}: not declared in {consumer}"); bad += 1; continue
        if b is None:
            print(f"check-abi-pairs: {symbol}: not defined in {provider}"); bad += 1; continue
        if a != b:
            print(f"check-abi-pairs: {symbol}: MISMATCH")
            print(f"  consumer {consumer}: {a[0]} ({', '.join(a[1])})")
            print(f"  provider {provider}: {b[0]} ({', '.join(b[1])})")
            bad += 1
        else:
            print(f"check-abi-pairs: {symbol}: agrees -> {a[0]} ({', '.join(a[1])})")
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
