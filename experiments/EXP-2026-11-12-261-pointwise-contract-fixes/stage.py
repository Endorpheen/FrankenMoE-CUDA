#!/usr/bin/env python3
"""EXP261: stage the CPU pointwise module, apply the three FIX-LIST corrections, regenerate the kernel.

Nothing in the live tree is modified: the source files are copied here, the kernel and the generated C are
produced inside this experiment, and the live generated/cpu-backend.c is used only as a diff reference.
"""
from pathlib import Path
import hashlib, json, re, shutil, subprocess, sys

LIVE = Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
SRC = LIVE / 'source/cpu-backend'
F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-261-pointwise-contract-fixes'
S = E / 'source'
S.mkdir(parents=True, exist_ok=True)

MODULES = ['tensor-base.goose', 'pointwise.goose', 'reductions.goose', 'attention.goose', 'transforms.goose',
           'matrix.goose', 'recurrent.goose', 'rope.goose', 'indexed.goose', 'image.goose',
           'codec-transform.goose', 'entry.goose']

for m in MODULES + ['operation-map.json']:
    shutil.copy2(SRC / m, S / m)
shutil.copytree(SRC / 'codec', S / 'codec', dirs_exist_ok=True)

p = S / 'pointwise.goose'
src = p.read_text()

# --- fix 1: the unary SILU path must use the same AVX2-compatible association as the vectorised native loop.
# The measured divergence (247 of 1000 values, spread over the whole array, not the tail) proves the module
# used libm expf for every element of the standalone SILU op while native uses ggml_v_silu for the head.
old_unary_sig = 'fn unary_value(p:const Node&,op:i64,x:f32)->f32 {'
new_unary_sig = 'fn unary_value(p:const Node&,op:i64,x:f32,col:i64,width:i64)->f32 {'
assert src.count(old_unary_sig) == 1
src = src.replace(old_unary_sig, new_unary_sig, 1)

old_silu = 'assert(op==UNARY_OP_SILU);return silu(x);'
new_silu = 'assert(op==UNARY_OP_SILU);return silu_compat(p,x,col,width);'
assert src.count(old_silu) == 1
src = src.replace(old_silu, new_silu, 1)

old_call = 'value=unary_value(p,p.ip[0],a);'
new_call = 'value=unary_value(p,p.ip[0],a,i%p.d.ne[0],p.d.ne[0]);'
assert src.count(old_call) == 1
src = src.replace(old_call, new_call, 1)

# --- fix 2: native evaluates MAX(MIN(x, hi), lo) with the MIN/MAX macros. Macros are not fminf/fmaxf:
# measured at run time, fminf(0x7f800001, 0.5) returns 0x7fc00001 (a quieted NaN) while the macro returns 0.5,
# so clamp has to reproduce the macro form exactly, including the order that puts the second operand on NaN.
old_clamp = 'value=min32(max32(a,p.fp[0]),p.fp[1]);'
new_clamp = 'value=native_max(native_min(a,p.fp[1]),p.fp[0]);'
assert src.count(old_clamp) == 1
src = src.replace(old_clamp, new_clamp, 1)

anchor_helpers = 'fn cpp_min(a:f32,b:f32)->f32 {return if b<a {b}else {a};}'
assert src.count(anchor_helpers) == 1
helpers = ('fn native_min(a:f32,b:f32)->f32 {return if a<b {a}else {b};}\n'
           'fn native_max(a:f32,b:f32)->f32 {return if a>b {a}else {b};}\n' + anchor_helpers)
src = src.replace(anchor_helpers, helpers, 1)

# --- fix 3: the two native scale kernels differ. The F32 forward skips the addend when the bias is zero
# (memcpy + vec_scale), so -0 stays -0; the BF16 forward always computes s*x + b, so (-0)+(+0) becomes +0.
# Measured: scale_bf16 on -0 gives native 0000 and the port 8000 before this change.
old_scale = 'value=a*p.fp[0]+p.fp[1];'
new_scale = ('value=if p.d.kind==0 as i64 {if p.fp[1]==(0 as f32) {a*p.fp[0]} else {a*p.fp[0]+p.fp[1]}} '
             'else {a*p.fp[0]+p.fp[1]};')
assert src.count(old_scale) == 1
src = src.replace(old_scale, new_scale, 1)

p.write_text(src)

# --- regenerate the kernel exactly as reproduce/fg143_generate.py does, but with the pinned compiler.
mapping = json.loads((S / 'operation-map.json').read_text())
text = '\n'.join((S / n).read_text() for n in MODULES)
used = set(re.findall(r'\b(?:OP_|UNARY_OP_|GLU_OP_)[A-Z0-9_]+\b', text))
assert used <= mapping.keys(), sorted(used - mapping.keys())
text = re.sub(r'\b(?:OP_|UNARY_OP_|GLU_OP_)[A-Z0-9_]+\b', lambda m: str(mapping[m.group(0)]), text)
(S / 'kernel.goose').write_text(text)

import sys
CC = sys.argv[1] if len(sys.argv) > 1 else json.loads((F / 'runtime/compiler/current.json').read_text())['compiler']
print('compiler', CC, hashlib.sha256(Path(CC).read_bytes()).hexdigest()[:16])
q = subprocess.run([CC, '-o', str(S / 'cpu-backend.c'), str(S / 'kernel.goose')], capture_output=True, text=True)
(S / 'generation.log').write_text(q.stdout + q.stderr)
if q.returncode:
    print(q.stdout[-3000:]); print(q.stderr[-3000:]); sys.exit(q.returncode)
print('generated', (S / 'cpu-backend.c').stat().st_size, 'bytes with the pinned compiler')
print('live generated C size', (LIVE / 'generated/cpu-backend.c').stat().st_size)
