#!/usr/bin/env python3
"""Emit a C wrapper whose argument order follows the generated prototype.

The compiler does not keep the declaration order of parameters, so a wrapper written
from an assumption fails to type-check. Here the prototype is parsed and the
arguments are taken from a per-type queue supplied by the caller description:
gg_slice parameters consume slice names in order, int64_t consumes ints, float
consumes floats.
"""
import re, sys, json

def prototype_by_shape(src, shape):
    """Find the single prototype whose parameter types match `shape` exactly."""
    found = []
    for m in re.finditer(r'static int64_t (gg_function_\d+)\(([^)]*)\);', src):
        params = [p.strip().rsplit(' ', 1)[0].replace('const ', '').strip()
                  for p in m.group(2).split(',') if p.strip() != 'int64_t base']
        if params == shape:
            found.append((m.group(1), params))
    if len(found) != 1:
        sys.exit('shape matches %d prototypes, expected exactly 1: %s'
                 % (len(found), ' '.join(shape)))
    return found[0]

def prototype(src, arity):
    for m in re.finditer(r'static int64_t (gg_function_\d+)\(([^)]*)\);', src):
        params = [p.strip() for p in m.group(2).split(',')]
        if len(params) - 1 == arity:           # last one is 'base'
            return m.group(1), params
    sys.exit('no prototype with %d parameters' % arity)

def emit(func, params, slices, ints, floats, name, ret):
    args = []
    si = ii = fi = 0
    for p in params:
        pt, pn = p.rsplit(' ', 1) if ' ' in p else (p, '')
        pt = pt.replace('const ', '').strip()
        if pt == 'gg_slice':
            args.append(slices[si]); si += 1
        elif pt in ('int64_t', 'uint64_t'):
            args.append(ints[ii]); ii += 1
        elif pt == 'float':
            args.append(floats[fi]); fi += 1
        elif pt == 'int64_t base':
            args.append('0')
    args.append('0')
    body = '%s(%s)' % (func, ', '.join(args))
    return name, body, (si, ii, fi)

if __name__ == '__main__':
    src = open(sys.argv[1]).read()
    desc = json.load(open(sys.argv[2]))
    out = []
    for e in desc:
        func, params = prototype(src, e['arity'])
        name, body, used = emit(func, params, e['slices'], e['ints'], e['floats'], e['name'], 'int64_t')
        if used != (len(e['slices']), len(e['ints']), len(e['floats'])):
            sys.exit('%s: prototype uses %s but %d/%d/%d were offered'
                     % (e['name'], used, len(e['slices']), len(e['ints']), len(e['floats'])))
        out.append('int64_t %s {\n    return %s;\n}\n' % (name, body))
    print('\n'.join(out))
