#!/usr/bin/env python3
"""Generate the signature/hash-bound native CUDA source patch for worker-9.

The patch is mechanical:
  * one include of apex-cuda-goose-ops.h per target translation unit;
  * one default-on hook at the top of each native ggml_cuda_op_* entry point.

Every substitution anchor is fixed text in the frozen snapshot. The generator
records SHA-256 of each input file and of the produced patch/header, and refuses
to run if a previously recorded input hash no longer matches.
"""
import argparse, difflib, hashlib, json, sys
from pathlib import Path

INCLUDE = '#include "apex-cuda-goose-ops.h"'

# (file, include_anchor_line, [(function_start_line, hook_body)])
TARGETS = {
    'unary.cu': {
        'include_after': '#include "convert.cuh"',
        'hooks': [
            ('void ggml_cuda_op_silu(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::unary(ctx, dst, 0)) { return; }'),
            ('void ggml_cuda_op_sigmoid(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::unary(ctx, dst, 1)) { return; }'),
            ('void ggml_cuda_op_softplus(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::unary(ctx, dst, 2)) { return; }'),
            ('void ggml_cuda_op_swiglu(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::swiglu(ctx, dst)) { return; }'),
        ],
    },
    'binbcast.cu': {
        'include_after': '#include "convert.cuh"',
        'hooks': [
            ('void ggml_cuda_op_add(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::bin_bcast(ctx, dst, 0)) { return; }'),
            ('void ggml_cuda_op_sub(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::bin_bcast(ctx, dst, 1)) { return; }'),
            ('void ggml_cuda_op_mul(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::bin_bcast(ctx, dst, 2)) { return; }'),
            ('void ggml_cuda_op_div(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::bin_bcast(ctx, dst, 3)) { return; }'),
        ],
    },
    'norm.cu': {
        'include_after': '#include <cstdint>',
        'hooks': [
            ('void ggml_cuda_op_norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::norm(ctx, dst)) { return; }'),
            ('void ggml_cuda_op_group_norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::group_norm(ctx, dst)) { return; }'),
            ('void ggml_cuda_op_rms_norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::rms_norm(ctx, dst)) { return; }'),
            ('void ggml_cuda_op_l2_norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::l2_norm(ctx, dst)) { return; }'),
        ],
    },
    'softmax.cu': {
        'include_after': '#include "softmax.cuh"',
        'hooks': [
            ('void ggml_cuda_op_soft_max(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::soft_max(ctx, dst)) { return; }'),
        ],
    },
    'rope.cu': {
        'include_after': '#include "rope.cuh"',
        'hooks': [
            ('void ggml_cuda_op_rope(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {',
             '    if (apex_cuda_goose::rope(ctx, dst)) { return; }'),
        ],
    },
}

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def patch_file(name, spec, text):
    inc = spec['include_after']
    if text.count(inc) != 1:
        sys.exit(f'{name}: include anchor not unique: {inc!r}')
    text = text.replace(inc, inc + '\n' + INCLUDE, 1)
    for anchor, hook in spec['hooks']:
        if text.count(anchor) != 1:
            sys.exit(f'{name}: hook anchor not unique: {anchor!r}')
        injected = anchor + '\n#if APEX_CUDA_GOOSE_OPS\n' + hook + '\n#endif'
        text = text.replace(anchor, injected, 1)
    return text

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src-dir', required=True)
    ap.add_argument('--out-dir', required=True)
    ap.add_argument('--header', required=True)
    a = ap.parse_args()
    src = Path(a.src_dir); out = Path(a.out_dir); out.mkdir(parents=True, exist_ok=True)
    hashes_path = out / 'hashes.json'
    header = Path(a.header)
    inputs = {}
    for name in TARGETS:
        p = src / name
        if not p.is_file():
            sys.exit(f'missing source {p}')
        inputs[name] = sha(p)
    if hashes_path.exists():
        old = json.loads(hashes_path.read_text())
        if old.get('inputs') != inputs:
            sys.exit('input hash drift: frozen sources changed; refusing to regenerate')

    chunks = []
    header_lines = header.read_text().splitlines(True)
    chunks.extend(difflib.unified_diff([], header_lines,
                                       fromfile='/dev/null',
                                       tofile='b/ggml/src/ggml-cuda/apex-cuda-goose-ops.h', n=3))
    for name, spec in TARGETS.items():
        original = (src / name).read_text()
        patched = patch_file(name, spec, original)
        rel = f'ggml/src/ggml-cuda/{name}'
        diff = difflib.unified_diff(original.splitlines(True), patched.splitlines(True),
                                    fromfile='a/' + rel, tofile='b/' + rel, n=3)
        chunks.extend(diff)
    patch_text = '# worker-9 Apex CUDA Goose hooks (default-on; -DAPEX_CUDA_GOOSE_OPS=0 rollback)\n' + ''.join(chunks)
    patch_path = out / 'apex-cuda-elementwise-norm-rope.patch'
    patch_path.write_text(patch_text)
    header_copy = out / 'apex-cuda-goose-ops.h'
    if header.resolve() != header_copy.resolve():
        header_copy.write_text(header.read_text())
    meta = {
        'producer': 'worker-9/tools/make_patch.py',
        'inputs': inputs,
        'patch_sha256': sha(patch_path),
        'header_sha256': sha(header_copy),
        'default': 'APEX_CUDA_GOOSE_OPS=1',
        'rollback': 'compile with -DAPEX_CUDA_GOOSE_OPS=0',
        'hooks': {name: [h[0] for h in spec['hooks']] for name, spec in TARGETS.items()},
    }
    hashes_path.write_text(json.dumps(meta, indent=2) + '\n')
    print(json.dumps(meta, indent=2))

if __name__ == '__main__':
    main()
