"""Check real PQ2 projections and time the ordered CUDA component pairs."""
import ctypes as ct
from hashlib import sha256
import importlib.util
import json
import mmap
from pathlib import Path
import sys

import numpy as np

experiments = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    'recurrent', experiments / 'EXP-2026-11-25-1974-bonsai-recurrent/source/check-layer.py')
rec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rec)
gpu = rec.gpu
D, F, I, P, U = gpu.D, gpu.F, gpu.I, gpu.P, gpu.U


def cuda_event():
    event = P()
    gpu.call('cuEventCreate', ct.byref(event), U(0))
    return event


def elapsed(run, start, stop):
    gpu.call('cuEventRecord', start, P())
    run()
    gpu.call('cuEventRecord', stop, P())
    gpu.call('cuEventSynchronize', stop)
    ms = F()
    gpu.call('cuEventElapsedTime', ct.byref(ms), start, stop)
    return ms.value


def dispatch(kernel, grid, lanes, args, shared_bytes=0):
    params = (P * len(args))(*(ct.addressof(v) for v in args))
    gpu.call('cuLaunchKernel', kernel, U(grid[0]), U(grid[1]), U(1),
             U(lanes), U(1), U(1), U(shared_bytes), P(), params, P())


def run_case(kernels, event_pair, matrix, width, rows, tokens, rng):
    weights = rec.Layer.stage(matrix)
    x = np.ascontiguousarray(rng.normal(0, 0.7, (tokens, width)), dtype=np.float32)
    in_dev = rec.Layer.stage(x.tobytes())
    half = gpu.allocate(tokens * width * 2)
    baseline = gpu.allocate(rows * tokens * 4)
    mmq = gpu.allocate(rows * tokens * 4)
    try:
        def a():
            for t in range(tokens):
                dispatch(kernels['pq2_matvec'], ((rows + 3)//4, 1), 128,
                         (weights, D(in_dev.value + t * width * 4), D(baseline.value + t * rows * 4),
                          U(width), U(rows)))

        def b():
            pairs = tokens * width // 2
            dispatch(kernels['pack_half'], ((pairs + 255)//256, 1), 256, (in_dev, half, U(pairs)))
            dispatch(kernels['pq2_mmq'], ((rows + 31)//32, (tokens + 31)//32), 128,
                     (weights, half, mmq, U(width), U(rows), U(tokens)))

        a()
        b()
        ref = np.ctypeslib.as_array(gpu.download(baseline, rows * tokens)).reshape(tokens, rows)
        result = np.ctypeslib.as_array(gpu.download(mmq, rows * tokens)).reshape(tokens, rows)
        assert np.isfinite(ref).all() and np.isfinite(result).all()
        diff = np.abs(ref - result)
        rel_l2 = float(np.linalg.norm(diff) / np.linalg.norm(ref))
        assert rel_l2 < 5e-4, rel_l2

        # The CPU test rounds both operands exactly as the MMA input contract.
        # Compare sampled entries against an independent double-precision dot.
        weight_bytes = width // 128 * 34
        samples = [0, 3, 7, 8, 15, 16, 23, 24, 31, 32, rows//2, rows-1]
        selected = b''.join(matrix[i*weight_bytes:(i+1)*weight_bytes] for i in samples)
        w = rec.unpack(selected, len(samples), width).astype(np.float16).astype(np.float64)
        expected = x.astype(np.float16).astype(np.float64) @ w.T
        error = float(np.max(np.abs(expected - result[:, samples])))
        np.testing.assert_allclose(result[:, samples], expected, atol=2e-4, rtol=4e-5)
        timings = [(name, elapsed(run, *event_pair)) for name, run in
                   (('scalar', a), ('mmq', b), ('mmq', b), ('scalar', a))]
        return dict(rows=rows, width=width, tokens=tokens, relative_l2_to_f32_matvec=rel_l2,
                    max_abs_to_f32_matvec=float(np.max(diff)),
                    max_abs_to_independent_rounded_reference=error, ordered_kernel_ms=timings)
    finally:
        for ptr in (mmq, baseline, half, in_dev, weights):
            gpu.call('cuMemFree_v2', ptr)


def main():
    image = Path(sys.argv[3]).read_bytes()
    device, context = I(), P()
    gpu.call('cuInit', U(0))
    gpu.call('cuDeviceGet', ct.byref(device), I(0))
    gpu.call('cuCtxCreate_v2', ct.byref(context), U(0), device)
    try:
        module = rec.Layer.load(image)
        kernels = {}
        for name in ('pq2_matvec', 'pack_half', 'pq2_mmq'):
            kernels[name] = P()
            gpu.call('cuModuleGetFunction', ct.byref(kernels[name]), module, ('bonsai_' + name).encode())
        event_pair = cuda_event(), cuda_event()
        tensors = {t['name']:t for t in gpu.inventory['tensors']}
        rng = np.random.default_rng(1978)
        result = []
        with gpu.model.open('rb') as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as model:
            for name, tokens, full_rows in (
                ('blk.0.attn_qkv.weight', 33, False),
                ('blk.0.attn_qkv.weight', 128, True),
                ('blk.3.attn_output.weight', 128, True),
                ('blk.0.ffn_down.weight', 256, True),
            ):
                tensor = tensors[name]
                width, rows = tensor['shape']
                rows -= not full_rows
                assert tensor['type'] == 142
                start = gpu.inventory['data_offset'] + tensor['offset']
                data = model[start:start + rows * (width // 128) * 34]
                observation = run_case(kernels, event_pair, data, width, rows, tokens, rng)
                result.append(dict(tensor=name, **observation))
        print(json.dumps(dict(candidate_cubin_sha256=sha256(image).hexdigest(), cases=result), indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2', context)


if __name__ == '__main__':
    main()
