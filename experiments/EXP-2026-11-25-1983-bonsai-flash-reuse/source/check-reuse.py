"""Compare FP32 Flash layouts and verify identical outputs against EXP-1982."""
import ctypes as ct
import importlib.util
import json
from pathlib import Path
import sys

import numpy as np

experiments = Path(__file__).resolve().parents[2]
prior = experiments / 'EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py'
spec = importlib.util.spec_from_file_location('component', prior)
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
D, P, U, I, L, F = gpu.D, gpu.P, gpu.U, gpu.I, gpu.L, gpu.F


def run_case(k, events, keys, nq, rng):
    heads, group, dim, kv_bytes = 24, 4, 256, 576
    tiles = (keys + 255)//256
    partial_bytes = tiles * (dim+2) * 4
    capacity = min(nq * heads, 32*1024*1024//partial_bytes)
    ptrs = []

    def alloc(n):
        result = gpu.allocate(n)
        ptrs.append(result)
        return result

    def stage(data):
        result = component.rec.Layer.stage(data)
        ptrs.append(result)
        return result

    q = stage(rng.normal(0, 0.7, (nq, heads, dim)).astype(np.float32).tobytes())
    cfg = (L * 22)(dim, dim, nq, heads, group, group, keys, heads*1024, 1024,
                   kv_bytes, 144, kv_bytes, 144, 0, 1024, heads*1024, 256, tiles,
                   nq*heads, capacity, capacity*partial_bytes, keys-nq+1)
    tile_conf = stage(bytes(cfg))
    out_a, out_b = alloc(nq*heads*1024), alloc(nq*heads*1024)
    flash_tiles = (keys+127)//128
    flash_groups = 4*((nq+1)//2)
    group_bytes = flash_tiles * 16 * 258 * 4
    wave = min(flash_groups, (32*1024*1024)//group_bytes)
    flash_scratch = alloc(wave * group_bytes)
    key, value = alloc(keys*kv_bytes), alloc(keys*kv_bytes)
    source = alloc(keys*4096)
    scratch = alloc(capacity*partial_bytes)
    chunks = (keys+31)//32
    packed_k, packed_v = [alloc(chunks*4*16384) for _ in range(2)]
    try:
        for dst in (key, value):
            data = rng.normal(0, .7, (keys, group, dim)).astype(np.float32).tobytes()
            gpu.upload(source, (ct.c_uint8 * len(data)).from_buffer_copy(data))
            component.dispatch(k['rows'], ((keys*32+127)//128, 1), 128,
                               (source, dst, D(4096), U(32), D(keys*32)))
        gpu.call('cuMemFree_v2', source)
        ptrs.remove(source)

        def control():
            for start in range(0, nq*heads, capacity):
                points = min(capacity, nq*heads-start)
                component.dispatch(k['tiles'], (points, tiles), 32,
                    (q, key, value, D(0), scratch, tile_conf, F(1/16), I(start)))
                component.dispatch(k['merge'], (points, 1), 32, (scratch, out_a, tile_conf, I(start)))

        def flash(mode):
            if mode == 'staged':
                component.dispatch(k['stage'], (chunks,4),128,
                                   (key,value,packed_k,packed_v,U(keys),U(chunks)))
            for start in range(0,flash_groups,wave):
                active = min(wave,flash_groups-start)
                operands = (packed_k,packed_v) if mode == 'staged' else (key,value)
                extra = (U(chunks),) if mode == 'staged' else ()
                component.dispatch(k[mode], (active,flash_tiles),128,
                    (q,*operands,flash_scratch,U(nq),U(keys),U(keys-nq+1),U(flash_tiles),U(start),*extra))
                component.dispatch(k['merge_f32'], (active*12,1),32,
                    (flash_scratch,out_b,U(nq),U(keys-nq+1),U(flash_tiles),U(start)))

        control()
        a = np.ctypeslib.as_array(gpu.download(out_a, nq*heads*dim))
        computed = {}
        for mode in ('original','skewed','staged'):
            flash(mode)
            computed[mode] = np.ctypeslib.as_array(gpu.download(out_b,nq*heads*dim))
        b = computed['original']
        assert np.isfinite(a).all() and np.isfinite(b).all()
        assert all(x.tobytes()==b.tobytes() for x in computed.values()), (keys,nq)
        error = float(np.max(np.abs(a-b)))
        rel_l2 = float(np.linalg.norm(a-b) / np.linalg.norm(a))
        assert rel_l2 < 0.003 and error < 0.002
        timings = [(mode,component.elapsed(lambda: flash(mode),*events))
                   for mode in ('original','skewed','staged','staged','skewed','original')]
        return dict(kv_tokens=keys, query_tokens=nq, max_absolute_error=error, relative_l2=rel_l2,
                    ordered_gpu_ms=timings, bit_exact_f32=True, extra_staging_bytes=chunks*4*32768)
    finally:
        for ptr in reversed(ptrs):
            gpu.call('cuMemFree_v2', ptr)


def main():
    device, context = I(), P()
    gpu.call('cuInit', U(0))
    gpu.call('cuDeviceGet', ct.byref(device), I(0))
    gpu.call('cuCtxCreate_v2', ct.byref(context), U(0), device)
    try:
        module = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        frozen = component.rec.Layer.load(Path(sys.argv[4]).read_bytes())
        k = {}
        for label, name in {
            'rows':'goose_set_q4_rows', 'tiles':'goose_q4_attention_tiles',
            'merge':'goose_q4_attention_merge','original':'bonsai_flash_f32_tile',
            'skewed':'bonsai_flash_skewed_tile','staged':'bonsai_flash_staged_tile',
            'stage':'bonsai_flash_stage_kv','merge_f32':'bonsai_flash_f32_merge',
        }.items():
            kernel = P()
            gpu.call('cuModuleGetFunction', ct.byref(kernel),
                     frozen if name.startswith('goose_') else module, name.encode())
            k[label] = kernel
        events = component.cuda_event(), component.cuda_event()
        rng = np.random.default_rng(1981)
        print(json.dumps([run_case(k,events,kc,nq,rng) for kc,nq in
                          ((65,16),(257,33),(8192,128),(32768,128),(1024,1024))],indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2',context)


if __name__ == '__main__':
    main()
