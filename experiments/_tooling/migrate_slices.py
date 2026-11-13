#!/usr/bin/env python3
"""Migrate the slice-based experiments (449, 452, 455, 457) and the two-module
experiment (456) to goose-2026.3-string."""
import re
import sys
from pathlib import Path

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
GOOSE = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'

ADAPTERS = {
    '449': '''
static int64_t fg449_route_id(int64_t m,const char *p,int64_t n){
    sl_u8 s; s.data=(uint8_t*)(uintptr_t)p; s.len=n;
    return route_id(m,s);
}
static int64_t fg449_requires_key(int64_t a){ return requires_key(a); }
''',
    '452': '''
static int64_t fg452_decode(uint64_t a,int64_t b,uint64_t c,int64_t d){ return decode_component(a,b,c,d); }
static int64_t fg452_actions(int64_t n,const char *keys,int64_t klen,
                             const int64_t *koffs,const int64_t *klens,int64_t *out){
    sl_u8 k; k.data=(uint8_t*)(uintptr_t)keys; k.len=klen;
    sl_i64 so; so.data=(int64_t*)(uintptr_t)koffs; so.len=n;
    sl_i64 sl; sl.data=(int64_t*)(uintptr_t)klens; sl.len=n;
    sl_i64 sr; sr.data=(int64_t*)(uintptr_t)out;   sr.len=n;
    return form_actions(n,k,so,sl,sr);
}
static int64_t fg452_source(int64_t a,int64_t b){ return param_source(a,b); }
''',
    '455': '''
static int64_t fg455_frame(int64_t flavor,int64_t has_event,int64_t *out){
    return frame(flavor,has_event,(uint64_t)(uintptr_t)out);
}
static int64_t fg455_size(const int64_t *steps,int64_t n,int64_t payload,int64_t event,uint64_t out){
    sl_i64 s; s.data=(int64_t*)(uintptr_t)steps; s.len=n;
    return frame_size(s,payload,event,out);
}
''',
    '457': '''
static int64_t fg457_up(int64_t x,int64_t n){ return align_up(x,n); }
static int64_t fg457_ok(int64_t a){ return align_ok(a); }
static int64_t fg457_nbytes(int64_t t,int64_t b,int64_t ne,int64_t *out){
    return nbytes(t,b,ne,(uint64_t)(uintptr_t)out);
}
static int64_t fg457_layout(const int64_t *rel,const int64_t *nbv,int64_t n,int64_t align,
                            int64_t data_start,int64_t *out){
    sl_i64 r; r.data=(int64_t*)(uintptr_t)rel; r.len=n;
    sl_i64 b; b.data=(int64_t*)(uintptr_t)nbv; b.len=n;
    sl_i64 o; o.data=(int64_t*)(uintptr_t)out; o.len=n+1;
    return layout(r,b,align,data_start,o);
}
''',
}

# 456 has two modules; each has its own exports and the fixture calls both
GCP_ADAPTER = '''
static int64_t fg456_mode(const char *p,int64_t n,int64_t *out){
    return mode_enabled((uint64_t)(uintptr_t)p,n,(uint64_t)(uintptr_t)out);
}
static int64_t fg456_route(int64_t hv,const char *v,int64_t vn,const char *d,int64_t dn,
                           char *out,int64_t cap){
    return route(hv,(uint64_t)(uintptr_t)v,vn,(uint64_t)(uintptr_t)d,dn,(uint64_t)(uintptr_t)out,cap);
}
static int64_t fg456_port(int64_t en,int64_t sp,int64_t ap,int64_t *out){
    return port_select(en,sp,ap,(uint64_t)(uintptr_t)out);
}
static int64_t fg456_alias(const char *p,int64_t n,char *out,int64_t cap){
    return gcp_alias((uint64_t)(uintptr_t)p,n,(uint64_t)(uintptr_t)out,cap);
}
static int64_t fg456_pshape(int64_t o,int64_t h,int64_t c,int64_t *out){
    return predict_shape(o,h,c,(uint64_t)(uintptr_t)out);
}
static int64_t fg456_ishape(int64_t o,int64_t h,int64_t *out){
    return instance_shape(o,h,(uint64_t)(uintptr_t)out);
}
'''
PROPS_ADAPTER = '''
static int64_t fg456_keys(int64_t has,int64_t *out){ return props_keys(has,(uint64_t)(uintptr_t)out); }
static int64_t fg456_pmode(int64_t sleeping,int64_t *out){ return props_mode(sleeping,(uint64_t)(uintptr_t)out); }
static int64_t fg456_post(int64_t enabled,int64_t *out){ return post_props_decision(enabled,(uint64_t)(uintptr_t)out); }
'''

EXP = {
    '449': ('routes', ['route_id', 'requires_key'], ADAPTERS['449']),
    '452': ('multipart', ['decode_component', 'form_actions', 'param_source'], ADAPTERS['452']),
    '455': ('sse', ['frame', 'frame_size'], ADAPTERS['455']),
    '457': ('offsets', ['align_up', 'align_ok', 'nbytes', 'layout'], ADAPTERS['457']),
}

BUILD = '''#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/{expdir}
OUT=$F/runtime/{tag}/build
G={goose}
mkdir -p "$OUT"
{emit}
if [ ! -f "$OUT/gen-runtime.c" ]; then "$G" --emit-runtime "$OUT/gen-runtime.c"; fi
gcc-13 -O2 -std=c11 -c "$OUT/gen-runtime.c" -o "$OUT/gen-runtime.o"
{compile}
g++-13 -O2 -std=c++17 -I"$E/source" -I"$E/tests" -c "$E/tests/{fixture}" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" {objs} "$OUT/gen-runtime.o" -o "$OUT/{tag}-fixture"
"$OUT/{tag}-fixture"
'''


def strip_main(p):
    s = p.read_text()
    m = re.search(r'(?m)^fn main\(\)\s*\{', s)
    if not m:
        return
    i = s.index('{', m.start()); depth = 1; j = i + 1
    while depth and j < len(s):
        if s[j] == '{': depth += 1
        elif s[j] == '}': depth -= 1
        j += 1
    p.write_text(s[:m.start()] + s[j:])


def export_fns(p, names):
    s = p.read_text()
    for n in names:
        s = re.sub(rf'(?m)^fn {re.escape(n)}\(', f'export fn {n}(', s)
    p.write_text(s)


def patch_fixture(d, fixture, header):
    fx = d / 'tests' / fixture
    t = fx.read_text()
    if 'extern "C" {' in t:
        start = t.index('extern "C" {')
        end = t.index('\n}\n', start) + 3
        t = t[:start] + f'#include "{header}"\n' + t[end:]
    else:
        t = re.sub(r'(?m)^\s*(?:int64_t|uint64_t|uint32_t|void)\s+fg4\d\d_\w+\([^;]*\);\n', '', t)
        first = t.index('#include')
        eol = t.index('\n', first) + 1
        t = t[:eol] + f'#include "{header}"\n' + t[eol:]
    if 'goose_init' not in t:
        t = t.replace('int main(void) {', 'int main(void) {\n    goose_init(0, 0);', 1)
    fx.write_text(t)


def write_single(exp_id, stem, exports, adapter):
    d = sorted(F.glob(f'experiments/EXP-2026-11-13-{exp_id}-*'))[0]
    src = d / 'source' / f'{stem}.goose'
    export_fns(src, exports)
    strip_main(src)
    (d / 'tests' / 'ffi-adapter.h').write_text(f'#pragma once\n#include "{stem}-gen.h"\n' + adapter)
    fixture = [p.name for p in (d / 'tests').glob('*fixture*') if p.suffix in ('.c', '.cpp')][0]
    patch_fixture(d, fixture, 'ffi-adapter.h')
    emit = (f'"$G" -o "$E/source/{stem}-gen.c" --header "$E/source/{stem}-gen.h" "$E/source/{stem}.goose"')
    compile_ = (f'gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/source/{stem}-gen.c" -o "$OUT/gen.o"')
    (d / 'build_fixture.sh').write_text(BUILD.format(
        expdir=f'experiments/{d.name}', tag=f'exp{exp_id}', goose=GOOSE,
        emit=emit, compile=compile_, fixture=fixture, objs='"$OUT/gen.o"'))
    (d / 'build_fixture.sh').chmod(0o755)
    print('migrated', exp_id)


def write_two_module():
    d = sorted(F.glob('experiments/EXP-2026-11-13-456-*'))[0]
    for stem, exports in [('gcp', ['mode_enabled', 'route', 'port_select', 'gcp_alias',
                                   'predict_shape', 'instance_shape']),
                          ('props', ['props_keys', 'props_mode', 'post_props_decision'])]:
        src = d / 'source' / f'{stem}.goose'
        export_fns(src, exports)
        strip_main(src)
    (d / 'tests' / 'ffi-adapter.h').write_text(
        '#pragma once\n#include "gcp-gen.h"\n#include "props-gen.h"\n' + GCP_ADAPTER + PROPS_ADAPTER)
    patch_fixture(d, 'gcp_props_fixture.cpp', 'ffi-adapter.h')
    emit = ('"$G" -o "$E/source/gcp-gen.c" --header "$E/source/gcp-gen.h" "$E/source/gcp.goose"\n'
            '"$G" -o "$E/source/props-gen.c" --header "$E/source/props-gen.h" "$E/source/props.goose"')
    compile_ = ('gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/source/gcp-gen.c" -o "$OUT/gcp.o"\n'
                'gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/source/props-gen.c" -o "$OUT/props.o"')
    (d / 'build_fixture.sh').write_text(BUILD.format(
        expdir=f'experiments/{d.name}', tag='exp456', goose=GOOSE,
        emit=emit, compile=compile_, fixture='gcp_props_fixture.cpp', objs='"$OUT/gcp.o" "$OUT/props.o"'))
    (d / 'build_fixture.sh').chmod(0o755)
    print('migrated 456')


if __name__ == '__main__':
    for exp_id in sys.argv[1:]:
        if exp_id == '456':
            write_two_module()
        else:
            stem, exports, adapter = EXP[exp_id]
            write_single(exp_id, stem, exports, adapter)
