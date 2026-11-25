"""Check padded Bonsai Flash on short query tiles at several cache lengths."""
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


def launch_shared(kernel, grid, threads, shared, args):
    params = (P * len(args))(*(ct.addressof(v) for v in args))
    gpu.call('cuLaunchKernel', kernel, U(grid[0]), U(grid[1]), U(1), U(threads),
             U(1), U(1), U(shared), P(), params, P())


def run_case(k, events, keys, nq, rng):
    heads, group, dim, kv_bytes = 24, 4, 256, 576
    tiles = (keys + 255)//256
    partial_bytes = tiles * (dim+2) * 4
    capacity = min(nq * heads, 32*1024*1024//partial_bytes)
    padded_keys = (keys+31) & ~31
    kv_span = padded_keys * group * dim * 2
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
    cfg[3] = 32
    cfg[7] = cfg[15] = 32768
    flash_conf = stage(bytes(cfg))
    out_a, out_b = alloc(nq*heads*1024), alloc(nq*heads*1024)
    q_pad, out_pad = alloc(nq*32768), alloc(nq*32768)
    k_half, v_half = alloc(kv_span), alloc(kv_span)
    key, value = alloc(keys*kv_bytes), alloc(keys*kv_bytes)
    source = alloc(keys*4096)
    scratch = alloc(capacity*partial_bytes)
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

        def flash():
            component.dispatch(k['pad'], (nq*32, 1), 256, (q, q_pad))
            for x, y in ((key,k_half),(value,v_half)):
                component.dispatch(k['stage'], (padded_keys, group), 128,
                                   (x, y, L(keys), L(padded_keys), L(kv_bytes), L(144)))
            launch_shared(k['flash'], ((nq+7)//8,group), 128, 33792,
                          (q_pad, k_half, v_half, out_pad, flash_conf, F(1/16)))
            component.dispatch(k['unpad'], (nq*heads, 1), 256, (out_pad, out_b))

        control()
        flash()
        a = np.ctypeslib.as_array(gpu.download(out_a, nq*heads*dim))
        b = np.ctypeslib.as_array(gpu.download(out_b, nq*heads*dim))
        assert np.isfinite(a).all() and np.isfinite(b).all(), (keys,nq)
        error = float(np.max(np.abs(a-b)))
        rel_l2 = float(np.linalg.norm(a-b) / np.linalg.norm(a))
        pass_error_gate = rel_l2 < 0.003 and error < 0.002
        tests = [(name,component.elapsed(run,*events)) for name,run in
                 (('tiles',control),('flash',flash),('flash',flash),('tiles',control))]
        return dict(kv_tokens=keys, query_tokens=nq, max_absolute_error=error, relative_l2=rel_l2,
                    ordered_gpu_ms=tests, pass_error_gate=pass_error_gate,
                    additional_scratch_bytes=2*kv_span+2*nq*32768)
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
            'merge':'goose_q4_attention_merge', 'stage':'goose_flash_q4_stage',
            'flash':'goose_flash_causal_mma', 'pad':'bonsai_pad_query',
            'unpad':'bonsai_unpad_attention',
        }.items():
            kernel = P()
            gpu.call('cuModuleGetFunction', ct.byref(kernel),
                     frozen if name.startswith('goose_') else module, name.encode())
            k[label] = kernel
        events = component.cuda_event(), component.cuda_event()
        rng = np.random.default_rng(1981)
        print(json.dumps([run_case(k,events,kc,nq,rng) for kc,nq in
                          ((65,16),(257,33),(8192,128),(32768,128))],indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2',context)


if __name__ == '__main__':
    main()
