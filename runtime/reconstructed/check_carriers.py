#!/usr/bin/env python3
"""Check that every Goose symbol the compiled carriers reference is defined by some compiled source.

Why this exists: the two copies of a carrier (runtime/reconstructed/src and vendor/flint/src) are
not interchangeable, and a file-level rule that picks one of them is wrong in one direction or the
other. Picking the staged copy drops the fg_port_* family that vendor's llama-impl.cpp needs;
picking the vendor copy drops the expert-cache doors (fg262a, fg263c, fg756s) that only the staged
moe-cache copy calls. See docs/goose-migration/CARRIER-DIVERGENCE-20261119.md.

The link is the authority on whether a symbol is missing, and it is expensive to reach. This check
answers the same question in about a second, before anything is compiled: it reads the same set of
sources the build will compile, collects what they define and what they reference, and reports the
difference.

Usage: python3 runtime/reconstructed/check_carriers.py [--root <repo>]
Exit 0 when nothing is unresolved, 1 with the list otherwise.
"""
import argparse
import pathlib
import re
import sys

CHANGED = [
    'llama-batch.cpp', 'llama-context.cpp', 'llama-graph.cpp', 'llama-hparams.cpp',
    'llama-kv-cache.cpp', 'llama-model.cpp', 'llama-memory-recurrent.cpp', 'llama-memory-hybrid.cpp',
    'llama-model-loader.cpp', 'llama-sampler.cpp', 'llama-vocab.cpp', 'llama-moe-cache.cpp',
    'unicode.cpp',
]
# A door name starts with fg and a number: fg262a, fg756s, fg229i_try_fill, fg_port_array_str.
REF = re.compile(r'\b(fg[0-9][0-9a-z]*_[a-z0-9_]+)\s*\(')
DEF = re.compile(r'extern "C"[^;{]*?\b(fg[0-9][0-9a-z]*_[a-z0-9_]+)\s*\(')
# A definition that is not an extern "C" block: what the file gives the linker for that name.
DEF_ANY = re.compile(r'^\s*(?:[A-Za-z_][\w:<>*&,\s]*\s+)?(fg[0-9][0-9a-z]*_[a-z0-9_]+)\s*\(', re.M)


def compiled_sources(root: pathlib.Path):
    staged = [root / 'runtime/reconstructed/src' / n for n in CHANGED]
    vendor = [p for p in (root / 'vendor/flint/src').glob('llama-*.cpp') if p.name not in CHANGED]
    models = list((root / 'vendor/flint/src/models').glob('*.cpp'))
    bridges = [
        root / 'integration/goose-standalone/gguf-reader-owner.cpp',
        root / 'integration/m1/common/fg-goose-model-bridge.cpp',
        root / 'integration/m1/common/fg-arch-registry-bridge.cpp',
    ]
    return [p for p in staged + vendor + models + bridges if p.exists()]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root', type=pathlib.Path,
                    default=pathlib.Path(__file__).resolve().parents[2])
    args = ap.parse_args()
    sources = compiled_sources(args.root)
    defined, refs = set(), {}
    for p in sources:
        text = p.read_text(errors='replace')
        defined |= set(DEF.findall(text))
        defined |= set(DEF_ANY.findall(text))
        for s in REF.findall(text):
            refs.setdefault(s, set()).add(p.relative_to(args.root).as_posix())
    missing = {s: sorted(v) for s, v in refs.items() if s not in defined}
    print(f'carriers: {len(sources)} source(s), {len(defined)} Goose symbol(s) defined, '
          f'{len(refs)} referenced')
    if missing:
        print(f'UNRESOLVED {len(missing)} symbol(s):')
        for s, users in sorted(missing.items()):
            print(f'  {s} <- {", ".join(users[:3])}')
        return 1
    print('PASS every referenced Goose symbol is defined by a compiled source')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
