#!/usr/bin/env python3
"""Emit the grammar wrapper after build-sampler-chain.sh has produced the C.

The chain is built from `export fn` entry points, so the grammar doors are found by their
exported names in the generated header and the wrapper is generated from those prototypes.
The ABI types therefore come from the compiler: a slice whose element type changes cannot
keep an old hand-typed declaration, which is how the caret-era wrapper drifted.
"""
import os
import re
import sys

SLICE = 'fg_slice_rec'


def prototype(header, symbol):
    m = re.search(r'^int64_t ' + symbol + r'\(([^)]*)\);', header, re.M)
    if not m:
        sys.exit('exported grammar entry point not found in the header: ' + symbol)
    parts = [q.strip() for q in m.group(1).split(',')]
    return [(q.rsplit(' ', 1)[0], q.rsplit(' ', 1)[1]) for q in parts]


def wrapper(symbol, call_name, params, marker):
    sig = []
    body = []
    args = []
    # the last parameter is the in-module marker; the host never passes it, it is a constant
    params = params[:-1]
    for i, (t, _) in enumerate(params):
        if t.startswith('sl_'):
            elem = {'i64': 'int64_t', 'f32': 'float', 'u8': 'uint8_t'}.get(t[3:], t[3:])
            sig.append('%s a%d' % (SLICE, i))
            body.append('    %s h%d = { (%s *) a%d.data, a%d.len };' % (t, i, elem, i, i))
            args.append('h%d' % i)
        else:
            sig.append('int64_t a%d' % i)
            args.append('a%d' % i)
    args.append(str(marker))
    return ('\nint64_t %s(%s) {\n%s\n    return %s(%s);\n}\n'
            % (call_name, ', '.join(sig), '\n'.join(body), symbol, ', '.join(args)))


def main(out):
    header = open(os.path.join(out, 'sampler_chain.h')).read()
    shim = os.path.join(out, 'sampler_chain_shim.c')
    text = open(shim).read()
    if 'typedef struct { void * data; int64_t len; } ' + SLICE not in text:
        with open(shim, 'a') as f:
            f.write('\ntypedef struct { void * data; int64_t len; } %s;\n' % SLICE)

    text = open(shim).read()
    for symbol, call_name, marker in (('fg_grammar_reject', 'fg_grammar_reject_call', 180),
                                      ('fg_grammar_first_set', 'fg_grammar_first_set_call', 181)):
        if call_name in text:
            continue
        with open(shim, 'a') as f:
            f.write(wrapper(symbol, call_name, prototype(header, symbol), marker))
    print('grammar entries: fg_grammar_reject, fg_grammar_first_set')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'build/sampler-chain')
