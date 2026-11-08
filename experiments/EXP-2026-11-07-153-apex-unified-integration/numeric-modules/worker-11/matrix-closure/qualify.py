#!/usr/bin/env python3
"""Mechanical Goose-C -> CUDA qualifier pass (matrix-closure copy).

Statements are untouched.  Only pure helpers and emitted gg_function_<n>
(n >= 1) get __host__ __device__, the host runtime fault path gets a device
trap, and four raw void* assignments in the emitted host runtime get the C++
casts nvcc requires.
"""
import hashlib, re, sys
from pathlib import Path

HELPERS = ["gg_view", "gg_sub", "gg_byte_view", "gg_index",
           "gg_div", "gg_mod", "gg_udiv", "gg_umod"]
GG_FAIL_OLD = '''static void gg_fail(const char *message) {
    fprintf(stderr, "goose runtime error: %s\\n", message); exit(1);
}'''
GG_FAIL_NEW = '''#if defined(__CUDACC__)
#include <cuda_runtime.h>
#endif
static __host__ __device__ void gg_fail(const char *message) {
#if defined(__CUDA_ARCH__)
    (void) message;
    __trap();
#else
    fprintf(stderr, "goose runtime error: %s\\n", message); exit(1);
#endif
}'''

def qualify(text: str) -> str:
    if GG_FAIL_OLD not in text:
        raise SystemExit("qualify: gg_fail prologue not found; emitter changed")
    text = text.replace(GG_FAIL_OLD, GG_FAIL_NEW, 1)
    for name in HELPERS:
        pat = re.compile(r'(^static )([A-Za-z0-9_ ]+? )(%s\()' % re.escape(name), re.M)
        text, n = pat.subn(r'\1__host__ __device__ \2\3', text, count=1)
        if n != 1:
            raise SystemExit(f"qualify: helper {name} not found")
    for old, new in [
        ("s->base = mmap(", "s->base = (unsigned char *) mmap("),
        ("p=gg_allocate(", "p=(unsigned char *) gg_allocate("),
        ("to=gg_allocate(", "to=(unsigned char *) gg_allocate("),
        ("char *s=malloc(", "char *s=(char *) malloc("),
    ]:
        text = text.replace(old, new)
    pat = re.compile(r'^static ([A-Za-z0-9_ ]+? )(gg_function_(\d+)\()', re.M)
    def repl(m):
        if int(m.group(3)) == 0:
            return m.group(0)
        return 'static __host__ __device__ ' + m.group(1) + m.group(2)
    text = pat.sub(repl, text)
    return text

def main(argv):
    if len(argv) != 3:
        raise SystemExit("usage: qualify.py input.c output.cu")
    src, dst = Path(argv[1]), Path(argv[2])
    raw = src.read_text(encoding="utf-8")
    out = qualify(raw)
    dst.write_text(out, encoding="utf-8")
    print({"input": str(src), "input_sha256": hashlib.sha256(raw.encode()).hexdigest(),
           "output": str(dst), "output_sha256": hashlib.sha256(out.encode()).hexdigest()})
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
