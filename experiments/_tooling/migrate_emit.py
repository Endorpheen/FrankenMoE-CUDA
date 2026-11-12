#!/usr/bin/env python3
"""Migrate an experiment's emit step to goose-2026.3-string.

For each experiment: mark the entry functions `export`, drop `main`, emit the C
and its header with the pinned binary, and write an adapter header so the
existing fixture keeps its call sites. The adapters are the only per-experiment
text.
"""
import re
import subprocess
import sys
from pathlib import Path

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
GOOSE = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'

# experiment id -> (module file stem, exported function names, adapter C)
EXP = {
    '445': ('startup', ['decide'], '''
static int64_t fg445_decide(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f,
                            int64_t g,int64_t h,int64_t i,int64_t j,int64_t k,int64_t l,
                            int64_t m,uint64_t n){
    sl_u8 o; o.data=(uint8_t*)(uintptr_t)n; o.len=8;
    return decide(a,b,c,d,e,f,g,h,i,j,k,l,m,o);
}
'''),
    '446': ('prerouting', ['preroute'], '''
static int64_t fg446_preroute(int64_t a,int64_t b,int64_t c,uint64_t d,int64_t e,int64_t f,
                              int64_t g,int64_t h,int64_t i,int64_t j,uint64_t k,int64_t l,
                              uint64_t m,int64_t n,uint64_t o,int64_t p,uint64_t q){
    return preroute(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q);
}
'''),
    '448': ('lifecycle', ['on_signal', 'shutdown_plan', 'exit_code'], '''
static int64_t fg448_on_signal(int64_t a,int64_t b,uint64_t c){ return on_signal(a,b,c); }
static int64_t fg448_shutdown_plan(int64_t a,int64_t b,uint64_t c){ return shutdown_plan(a,b,c); }
static int64_t fg448_exit_code(int64_t a,uint64_t c){ return exit_code(a,c); }
'''),
    '450': ('errors', ['classify_exception', 'error_code', 'status_from_payload'], '''
static int64_t fg450_classify(int64_t a,uint64_t b){ return classify_exception(a,b); }
static int64_t fg450_code(int64_t a,uint64_t b){ return error_code(a,b); }
static int64_t fg450_status(int64_t a,int64_t c,uint64_t b){ return status_from_payload(a,c,b); }
'''),
    '451': ('required', ['missing'], '''
static int64_t fg451_missing(int64_t a,int64_t b,uint64_t c){ return missing(a,b,c); }
'''),
    '453': ('stream', ['append', 'read_decision'], '''
static int64_t fg453_session_append(int64_t a,int64_t b,int64_t c,int64_t d,uint64_t e){
    return append(a,b,c,d,e);
}
static int64_t fg453_session_read(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,uint64_t f){
    return read_decision(a,b,c,d,e,f);
}
'''),
    '454': ('manager', ['retire_plan', 'stop_all_plan', 'gc_cutoff', 'gc_decision'], '''
static int64_t fg454_retire(int64_t op,int64_t found,uint64_t out){ return retire_plan(op,found,out); }
static int64_t fg454_stop(uint64_t out){ return stop_all_plan(out); }
static int64_t fg454_cutoff(int64_t now,int64_t ttl,uint64_t out){ return gc_cutoff(now,ttl,out); }
static int64_t fg454_gc(int64_t completed,int64_t now,int64_t ttl,uint64_t out){
    return gc_decision(completed,now,ttl,out);
}
'''),
    '458': ('io', ['fits', 'array_check', 'string_check', 'seek', 'read_raw', 'file_remain'], '''
static int64_t fg458_fits(int64_t s,int64_t r){ return fits(s,r); }
static int64_t fg458_arr(int64_t n,int64_t e,int64_t r,int64_t *out){ return array_check(n,e,r,out); }
static int64_t fg458_str(int64_t s,int64_t r,int64_t *out){ return string_check(s,r,out); }
static int64_t fg458_seek(int64_t a,int64_t o,int64_t r,int64_t *out){ return seek(a,o,r,out); }
static int64_t fg458_raw(int64_t s,int64_t c,int64_t o,int64_t r,int64_t end,int64_t *out){
    return read_raw(s,c,o,r,end,out);
}
static int64_t fg458_fremain(int64_t end,int64_t cur,int64_t *out){ return file_remain(end,cur,out); }
'''),
}

BUILD = '''#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/{expdir}
OUT=$F/runtime/{tag}/build
G={goose}
mkdir -p "$OUT"
"$G" -o "$E/source/{stem}-gen.c" --header "$E/source/{stem}-gen.h" "$E/source/{stem}.goose"
if [ ! -f "$OUT/gen-runtime.c" ]; then "$G" --emit-runtime "$OUT/gen-runtime.c"; fi
gcc-13 -O2 -std=c11 -c "$OUT/gen-runtime.c" -o "$OUT/gen-runtime.o"
gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/source/{stem}-gen.c" -o "$OUT/gen.o"
case "{fixture}" in
  *.cpp) g++-13 -O2 -std=c++17 -I"$E/source" -c "$E/tests/{fixture}" -o "$OUT/fixture.o"
         g++-13 "$OUT/fixture.o" "$OUT/gen.o" "$OUT/gen-runtime.o" -o "$OUT/{tag}-fixture" ;;
  *)     gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/tests/{fixture}" -o "$OUT/fixture.o"
         gcc-13 "$OUT/fixture.o" "$OUT/gen.o" "$OUT/gen-runtime.o" -o "$OUT/{tag}-fixture" ;;
esac
"$OUT/{tag}-fixture"
'''


def migrate(exp_id, expdir, stem, exports, adapter, fixture):
    src = expdir / 'source' / f'{stem}.goose'
    text = src.read_text()
    for name in exports:
        text = re.sub(rf'(?m)^fn {re.escape(name)}\(', f'export fn {name}(', text)
    # drop main
    m = re.search(r'(?m)^fn main\(\)\{.*\n\}\n?', text, re.S)
    if m:
        text = text[:m.start()] + text[m.end():]
    src.write_text(text)

    header = f'#pragma once\n#include "{stem}-gen.h"\n' + adapter
    (expdir / 'tests' / 'ffi-adapter.h').write_text(header)

    fx = expdir / 'tests' / fixture
    ftext = fx.read_text()
    if 'extern "C" {' in ftext:
        start = ftext.index('extern "C" {')
        end = ftext.index('\n}\n', start) + 3
        ftext = ftext[:start] + '#include "ffi-adapter.h"\n' + ftext[end:]
    else:
        # plain C fixture: drop the old prototypes, then include the adapter
        ftext = re.sub(r'(?m)^\s*(?:int64_t|uint64_t|uint32_t|void)\s+fg4\d\d_\w+\([^;]*\);\n',
                       '', ftext)
        first = ftext.index('#include')
        eol = ftext.index('\n', first) + 1
        ftext = ftext[:eol] + '#include "ffi-adapter.h"\n' + ftext[eol:]
    if 'goose_init' not in ftext:
        ftext = ftext.replace('int main(void) {', 'int main(void) {\n    goose_init(0, nullptr);', 1)
    fx.write_text(ftext)

    (expdir / 'build_fixture.sh').write_text(BUILD.format(
        expdir=f'experiments/{expdir.name}', tag=f'exp{exp_id}', goose=GOOSE,
        stem=stem, fixture=fixture))
    (expdir / 'build_fixture.sh').chmod(0o755)
    print(f'migrated {exp_id} {expdir.name}')


if __name__ == '__main__':
    for exp_id in sys.argv[1:]:
        stem, exports, adapter = EXP[exp_id]
        expdirs = sorted(F.glob(f'experiments/EXP-2026-11-13-{exp_id}-*'))
        assert len(expdirs) == 1, expdirs
        fixtures = [p.name for p in (expdirs[0] / 'tests').glob('*fixture*') if p.suffix in ('.c', '.cpp')]
        assert len(fixtures) == 1, fixtures
        migrate(exp_id, expdirs[0], stem, exports, adapter, fixtures[0])
