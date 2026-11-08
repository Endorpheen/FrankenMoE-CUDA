#!/usr/bin/env python3
"""Mechanical Goose-C -> CUDA qualifier adapter for worker-12.

The frozen Goose compiler emits portable C.  This adapter does not rewrite
program logic: it extracts the structs and gg_function_* definitions from that
exact emission, adds CUDA storage qualifiers chosen from a reachable-primitive
analysis, and emits the same function bodies for the device unit and for the
host mathematical fixture.  gg_function IDs are discovered and cross-checked
against role fingerprints; nothing is hard-coded except the primitive set.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

DEVICE_PRIMS = {
    "dfa_tid", "dfa_bid", "dfa_gdim", "dfa_shfl_xor_f32", "dfa_sync",
    "dfa_syncwarp", "dfa_sh_f32", "dfa_sh_set_f32", "dfa_sh_u32",
    "dfa_sh_set_u32",
}
HD_PRIMS = {
    "dfa_ld_u8", "dfa_ld_u16", "dfa_ld_u32", "dfa_ld_f32", "dfa_st_f32",
    "dfa_half", "dfa_f2i", "dfa_dp4a", "dfa_neg_half_flt_max",
}

DEVICE_PRELUDE = r'''
// Generated device prelude for fg012 Apex Q4_0 FlashAttention.
#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <cuda_fp16.h>

typedef struct { void *data; int64_t len; } gg_slice;

static __host__ __device__ __forceinline__ int64_t gg_index(int64_t i, int64_t) { return i; }
static __host__ __device__ __forceinline__ gg_slice gg_view(void * p, int64_t n) { gg_slice s; s.data = p; s.len = n; return s; }
static __host__ __device__ __forceinline__ int64_t gg_div(int64_t a, int64_t b) { return a / b; }
static __host__ __device__ __forceinline__ int64_t gg_mod(int64_t a, int64_t b) { int64_t r = a % b; return r < 0 ? r + (b < 0 ? -b : b) : r; }
static __host__ __device__ __forceinline__ uint64_t gg_udiv(uint64_t a, uint64_t b) { return a / b; }
static __host__ __device__ __forceinline__ uint64_t gg_umod(uint64_t a, uint64_t b) { return a % b; }

static __host__ __device__ __forceinline__ double gg_double_bits(uint64_t b) { double d; memcpy(&d, &b, 8); return d; }
static __host__ __device__ __forceinline__ uint32_t dfa_ld_u8(int64_t p, int64_t o) {
    return *reinterpret_cast<const uint8_t *>(p + o);
}
static __host__ __device__ __forceinline__ uint32_t dfa_ld_u16(int64_t p, int64_t o) {
    uint16_t v; memcpy(&v, reinterpret_cast<const void *>(p + o), 2); return v;
}
static __host__ __device__ __forceinline__ uint32_t dfa_ld_u32(int64_t p, int64_t o) {
    uint16_t a, b;
    memcpy(&a, reinterpret_cast<const void *>(p + o), 2);
    memcpy(&b, reinterpret_cast<const void *>(p + o + 2), 2);
    return uint32_t(a) | (uint32_t(b) << 16);
}
static __host__ __device__ __forceinline__ float dfa_ld_f32(int64_t p, int64_t o) {
    float v; memcpy(&v, reinterpret_cast<const void *>(p + o), 4); return v;
}
static __host__ __device__ __forceinline__ void dfa_st_f32(int64_t p, int64_t o, float v) {
    memcpy(reinterpret_cast<void *>(p + o), &v, 4);
}
static __host__ __device__ __forceinline__ float dfa_half(uint32_t v) {
    return __half2float(__ushort_as_half(static_cast<unsigned short>(v)));
}
static __host__ __device__ __forceinline__ int32_t dfa_f2i(float v) { return static_cast<int32_t>(v); }
static __host__ __device__ __forceinline__ int32_t dfa_dp4a(int32_t a, int32_t b, int32_t c) {
#if defined(__CUDA_ARCH__) && __CUDA_ARCH__ >= 610
    return __dp4a(a, b, c);
#else
    const int8_t * pa = reinterpret_cast<const int8_t *>(&a);
    const int8_t * pb = reinterpret_cast<const int8_t *>(&b);
    return c + int32_t(pa[0]) * int32_t(pb[0]) + int32_t(pa[1]) * int32_t(pb[1])
             + int32_t(pa[2]) * int32_t(pb[2]) + int32_t(pa[3]) * int32_t(pb[3]);
#endif
}
static __host__ __device__ __forceinline__ float dfa_neg_half_flt_max() { return -FLT_MAX / 2.0f; }

static __device__ __forceinline__ int64_t dfa_tid(int64_t a) { return a == 0 ? threadIdx.x : (a == 1 ? threadIdx.y : threadIdx.z); }
static __device__ __forceinline__ int64_t dfa_bid(int64_t a) { return a == 0 ? blockIdx.x : (a == 1 ? blockIdx.y : blockIdx.z); }
static __device__ __forceinline__ int64_t dfa_gdim(int64_t a) { return a == 0 ? gridDim.x : (a == 1 ? gridDim.y : gridDim.z); }
static __device__ __forceinline__ float dfa_shfl_xor_f32(float v, int64_t m) { return __shfl_xor_sync(0xffffffffu, v, static_cast<int>(m), 32); }
static __device__ __forceinline__ void dfa_sync() { __syncthreads(); }
static __device__ __forceinline__ void dfa_syncwarp() { __syncwarp(); }

// Per-block shared scratch: max(ne_KQ, ne_combine) <= 1024 plus max/sum tables.
static __shared__ float dfa_shbuf[1280];
static __device__ __forceinline__ float dfa_sh_f32(int64_t i) { return dfa_shbuf[i]; }
static __device__ __forceinline__ void dfa_sh_set_f32(int64_t i, float v) { dfa_shbuf[i] = v; }
static __device__ __forceinline__ uint32_t dfa_sh_u32(int64_t i) {
    uint32_t v; memcpy(&v, &dfa_shbuf[i], 4); return v;
}
static __device__ __forceinline__ void dfa_sh_set_u32(int64_t i, uint32_t v) {
    memcpy(&dfa_shbuf[i], &v, 4);
}
'''

HOST_PRELUDE = r'''
// Generated host prelude for the fg012 fixture.  Same names and same
// semantics as the device primitives; no CUDA runtime is required.
#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cfloat>

#ifndef __forceinline__
#define __forceinline__ inline
#endif
#ifndef __host__
#define __host__
#endif
#ifndef __device__
#define __device__
#endif

typedef struct { void *data; int64_t len; } gg_slice;

static inline int64_t gg_index(int64_t i, int64_t) { return i; }
static inline gg_slice gg_view(void * p, int64_t n) { gg_slice s; s.data = p; s.len = n; return s; }
static inline int64_t gg_div(int64_t a, int64_t b) { return a / b; }
static inline int64_t gg_mod(int64_t a, int64_t b) { int64_t r = a % b; return r < 0 ? r + (b < 0 ? -b : b) : r; }
static inline uint64_t gg_udiv(uint64_t a, uint64_t b) { return a / b; }
static inline uint64_t gg_umod(uint64_t a, uint64_t b) { return a % b; }

static inline double gg_double_bits(uint64_t b) { double d; memcpy(&d, &b, 8); return d; }
static inline uint32_t dfa_ld_u8(int64_t p, int64_t o) { return *reinterpret_cast<const uint8_t *>(p + o); }
static inline uint32_t dfa_ld_u16(int64_t p, int64_t o) { uint16_t v; memcpy(&v, reinterpret_cast<const void *>(p + o), 2); return v; }
static inline uint32_t dfa_ld_u32(int64_t p, int64_t o) {
    uint16_t a, b;
    memcpy(&a, reinterpret_cast<const void *>(p + o), 2);
    memcpy(&b, reinterpret_cast<const void *>(p + o + 2), 2);
    return uint32_t(a) | (uint32_t(b) << 16);
}
static inline float dfa_ld_f32(int64_t p, int64_t o) { float v; memcpy(&v, reinterpret_cast<const void *>(p + o), 4); return v; }
static inline void dfa_st_f32(int64_t p, int64_t o, float v) { memcpy(reinterpret_cast<void *>(p + o), &v, 4); }
static inline float dfa_half(uint32_t h) {
    uint32_t sign = (h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1Fu;
    uint32_t man = h & 0x3FFu;
    if (exp == 0) {
        float v = static_cast<float>(man) * 5.9604644775390625e-08f; // 2^-24
        return sign ? -v : v;
    }
    if (exp == 31) {
        uint32_t f = sign | 0x7F800000u | (man ? 0x007FFFFFu : 0u);
        float v; memcpy(&v, &f, 4); return v;
    }
    uint32_t f = sign | ((exp - 15 + 127) << 23) | (man << 13);
    float v; memcpy(&v, &f, 4); return v;
}
static inline int32_t dfa_f2i(float v) { return static_cast<int32_t>(v); }
static inline int32_t dfa_dp4a(int32_t a, int32_t b, int32_t c) {
    const int8_t * pa = reinterpret_cast<const int8_t *>(&a);
    const int8_t * pb = reinterpret_cast<const int8_t *>(&b);
    return c + int32_t(pa[0]) * int32_t(pb[0]) + int32_t(pa[1]) * int32_t(pb[1])
             + int32_t(pa[2]) * int32_t(pb[2]) + int32_t(pa[3]) * int32_t(pb[3]);
}
static inline float dfa_neg_half_flt_max() { return -FLT_MAX / 2.0f; }
'''


def brace_end(text, start):
    depth = 0
    i = start
    while i < len(text):
        c = text[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError("unbalanced braces")


def parse_functions(text):
    pattern = re.compile(r'static\s+([A-Za-z_][\w \*]*?)\s+(gg_function_\d+)\s*\(([^)]*)\)\s*\{')
    funcs = {}
    for m in pattern.finditer(text):
        name = m.group(2)
        start = m.end() - 1
        end = brace_end(text, start)
        ret = m.group(1).strip()
        params = [p.strip() for p in m.group(3).split(',') if p.strip()]
        if params:
            params = params[:-1]  # trailing Goose stack-frame parameter
        ptypes = []
        for p in params:
            ptypes.append(re.sub(r'\s+[A-Za-z_]\w*$', '', p).strip())
        funcs[name] = {
            'def': text[m.start():end + 1],
            'ret': ret,
            'params': params,
            'ptypes': ptypes,
            'body': text[start:end + 1],
            'pos': m.start(),
        }
    return funcs


def parse_types(text):
    start = text.find('typedef struct gg_type_')
    if start < 0:
        return ""
    ends = [x for x in (text.find('static void gg_quote', start),
                        text.find('static uint8_t gg_eq_', start)) if x >= 0]
    end = min(ends) if ends else text.find('\n\n', start)
    return text[start:end].strip() + "\n"


def analyze(funcs):
    calls = {}
    prims = {}
    for name, f in funcs.items():
        calls[name] = set('gg_function_' + x for x in re.findall(r'gg_function_(\d+)', f['body']))
        prims[name] = set(re.findall(r'\b(dfa_\w+)\b', f['body']))
    device = set()
    changed = True
    while changed:
        changed = False
        for name, f in funcs.items():
            if name in device:
                continue
            if prims[name] & DEVICE_PRIMS or (calls[name] & device):
                device.add(name)
                changed = True
    params = {n: len(f['ptypes']) for n, f in funcs.items()}
    roles = {}

    def setrole(role, pred, unique=True):
        hits = [n for n, f in funcs.items() if pred(n, f)]
        if len(hits) != 1:
            raise SystemExit(f"role {role}: expected 1 emitted function, got {hits}")
        roles[role] = hits[0]

    setrole('ALIBI', lambda n, f: f['ret'] == 'float' and f['ptypes'] == ['float', 'int64_t', 'int64_t', 'float', 'float'] and 'powf' in f['body'])
    setrole('Q4_GROUP', lambda n, f: f['ret'].startswith('gg_type_') and f['ptypes'] == ['uint32_t', 'int64_t', 'float'] and n not in device)
    setrole('DEQ4', lambda n, f: f['ret'].startswith('gg_type_') and f['ptypes'] == ['int64_t', 'int64_t'] and n not in device)
    setrole('DEQ4_SCALAR', lambda n, f: f['ret'] == 'float' and f['ptypes'] == ['int64_t', 'int64_t', 'int64_t'] and 'dfa_ld_u16' in f['body'])
    setrole('Q4_NIBBLE', lambda n, f: f['ret'] == 'float' and f['ptypes'] == ['uint32_t', 'int64_t', 'int64_t', 'float'] and n not in device)
    setrole('Q4_DOT', lambda n, f: f['ret'] == 'float' and len(f['ptypes']) == 10 and f['ptypes'][0] == 'int64_t' and 'dfa_dp4a' in f['body'])
    setrole('Q8_PACK', lambda n, f: f['ret'] == 'uint32_t' and f['ptypes'] == ['float'] * 5)
    setrole('Q8_ROUND', lambda n, f: f['ret'] == 'int32_t' and f['ptypes'] == ['float', 'float'] and 'dfa_f2i' in f['body'])
    setrole('Q8_AMAX4', lambda n, f: f['ret'] == 'float' and f['ptypes'] == ['float'] * 4 and 'fmaxf' in f['body'])
    sum4 = [n for n, f in funcs.items() if f['ret'] == 'float' and f['ptypes'] == ['float'] * 4
            and not re.search(r'\*|fmaxf|fabsf|dfa_|gg_function_', f['body']) and n not in device]
    mixv = [n for n, f in funcs.items() if f['ret'] == 'float' and f['ptypes'] == ['float'] * 4 and '*' in f['body'] and n not in device]
    if len(sum4) != 1 or len(mixv) != 1:
        raise SystemExit(f"role Q8_SUM4/SM_VALUE ambiguous: {sum4} {mixv}")
    roles['Q8_SUM4'] = sum4[0]
    roles['SM_VALUE'] = mixv[0]
    setrole('SM_EXP', lambda n, f: f['ret'] == 'float' and f['ptypes'] == ['float', 'float'] and 'expf' in f['body'])
    setrole('SM_MAX_NEW', lambda n, f: f['ret'] == 'float' and f['ptypes'] == ['float', 'float']
            and any(params[c] == 0 for c in calls[n]))
    setrole('SM_MIX', lambda n, f: f['ret'] == 'float' and f['ptypes'] == ['float'] * 3 and '*' in f['body'] and n not in device)
    setrole('KQ_OFFSET', lambda n, f: f['ret'] == 'float' and f['ptypes'] == [] and n not in device)
    setrole('APPLY_MASK', lambda n, f: f['ret'] == 'float' and f['ptypes'] == ['float', 'float', 'uint32_t'] and 'dfa_half' in f['body'])
    setrole('APPLY_SOFTCAP', lambda n, f: f['ret'] == 'float' and f['ptypes'] == ['float', 'float'] and 'tanhf' in f['body'])
    setrole('Q8_LANE_BYTE', lambda n, f: f['ret'] == 'int64_t' and f['ptypes'] == ['int64_t'] and '*' in f['body'] and n not in device and '16' in f['body'])
    return calls, prims, device, roles


def qualify(f, device_only):
    q = '__device__ __forceinline__' if device_only else '__host__ __device__ __forceinline__'
    return f['def'].replace('static', 'static ' + q, 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--generated', default='generated/fattn-q4.c')
    ap.add_argument('--device-out', default='generated/fattn-q4-device.cu')
    ap.add_argument('--host-out', default='generated/fattn-q4-host.inc')
    ap.add_argument('--manifest', default='generated/fattn-q4-manifest.json')
    args = ap.parse_args()

    gen_path = Path(args.generated)
    text = gen_path.read_text()
    types = parse_types(text)
    funcs = parse_functions(text)
    if not funcs:
        raise SystemExit('no gg_function_* definitions found')
    calls, prims, device, roles = analyze(funcs)

    if 'gg_function_0' not in funcs:
        raise SystemExit('gg_function_0 (Goose main) missing')
    entries = [n for n, f in funcs.items() if len(f['ptypes']) == 42 and n in device]
    if len(entries) != 1:
        raise SystemExit(f'expected one 42-argument device entry, got {entries}')
    entry = entries[0]
    if entry not in calls['gg_function_0']:
        raise SystemExit('Goose main does not reach the kernel entry')

    reach = set()
    stack = [entry]
    while stack:
        cur = stack.pop()
        if cur in reach:
            continue
        reach.add(cur)
        stack.extend(calls[cur])
    emit = (reach | set(roles.values())) - {'gg_function_0'}
    order = sorted(emit, key=lambda n: funcs[n]['pos'])
    dev_chunks = [DEVICE_PRELUDE, '\n', types, '\n']
    host_chunks = [HOST_PRELUDE, '\n', types, '\n']
    for name in order:
        qual = qualify(funcs[name], name in device)
        dev_chunks.append(qual + '\n')
        if name not in device:
            host_chunks.append(qual.replace('__host__ __device__ ', '') + '\n')
    host_chunks.append('\n// Semantic aliases resolved from emitter fingerprints.\n')
    for role, name in sorted(roles.items()):
        if funcs[name]['ptypes']:
            host_chunks.append(f'#define FG012_{role}(...) {name}(__VA_ARGS__, 0)\n')
        else:
            host_chunks.append(f'#define FG012_{role}() {name}(0)\n')
    host_chunks.append(f'#define FG012_DEQ4_TYPE {funcs[roles["DEQ4"]]["ret"]}\n')

    Path(args.device_out).write_text(''.join(dev_chunks))
    Path(args.host_out).write_text(''.join(host_chunks))

    manifest = {
        'schema': 'fg012.fattn-q4-qualifier/1',
        'generated_source': str(gen_path),
        'generated_sha256': hashlib.sha256(text.encode()).hexdigest(),
        'entry': entry,
        'entry_arity': len(funcs[entry]['ptypes']),
        'device_only': sorted(emit & device, key=lambda n: int(n.rsplit('_', 1)[1])),
        'host_device': sorted(emit - device, key=lambda n: int(n.rsplit('_', 1)[1])),
        'emitted': sorted(emit, key=lambda n: int(n.rsplit('_', 1)[1])),
        'roles': roles,
        'device_prelude_primitives': sorted(DEVICE_PRIMS | HD_PRIMS),
        'checks': {
            'main_reaches_entry': True,
            'entry_reaches_device_primitive': True,
            'entry_arity_42': True,
            'all_roles_unique': True,
        },
    }
    Path(args.manifest).write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'entry': entry, 'device': len(emit & device), 'host': len(emit - device),
                      'roles': roles}, indent=2))


if __name__ == '__main__':
    main()
