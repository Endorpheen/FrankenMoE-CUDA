"""Verify exact FP32 bytes and measure the three single-token PQ2 kernels."""
import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
import sys

import numpy as np

experiments = Path(__file__).resolve().parents[2]
prior = experiments / 'EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py'
spec = importlib.util.spec_from_file_location('component', prior)
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
P, U, I = gpu.P, gpu.U, gpu.I


def check_matrix(kernels, events, packed_weights, width, rows, input_values):
    weights = component.rec.Layer.stage(packed_weights)
    operand = component.rec.Layer.stage(input_values.tobytes())
    output = gpu.allocate(rows * 4)
    try:
        args = weights, operand, output, U(width), U(rows)

        def run(key):
            component.dispatch(kernels[key], ((rows + 3)//4, 1), 128, args)

        outputs = {}
        for key in kernels:
            run(key)
            outputs[key] = bytes(gpu.download(output, rows))
        for name, data in outputs.items():
            assert data == outputs['scalar'], name
        assert np.isfinite(np.frombuffer(outputs['scalar'], dtype=np.float32)).all()
        timings = {name: [] for name in kernels}
        for name in ('scalar', 'ordered', 'exchange', 'exchange', 'ordered', 'scalar'):
            def repeat():
                for _ in range(30):
                    run(name)
            timings[name].append(component.elapsed(repeat, *events) / 30)
        return dict(width=width, rows=rows, bit_exact=True, ms_per_projection=timings)
    finally:
        for ptr in (output, operand, weights):
            gpu.call('cuMemFree_v2', ptr)


def main():
    p = Path(sys.argv[3])
    device, context = I(), P()
    gpu.call('cuInit', U(0))
    gpu.call('cuDeviceGet', ct.byref(device), I(0))
    gpu.call('cuCtxCreate_v2', ct.byref(context), U(0), device)
    try:
        mod = component.rec.Layer.load(p.read_bytes())
        kernels = {}
        for name, suffix in [('scalar',''), ('ordered','_ordered'), ('exchange','_exchange')]:
            kernel = P()
            gpu.call('cuModuleGetFunction', ct.byref(kernel), mod, ('bonsai_pq2_matvec'+suffix).encode())
            kernels[name] = kernel
        events = component.cuda_event(), component.cuda_event()
        tensors = {t['name']:t for t in gpu.inventory['tensors']}
        rng = np.random.default_rng(1980)
        results = {}
        with gpu.model.open('rb') as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as model:
            for name in ('blk.0.attn_qkv.weight','blk.0.ssm_out.weight','blk.0.ffn_down.weight','output.weight'):
                tensor = tensors[name]
                width, rows = tensor['shape']
                assert tensor['type'] == 142
                start = gpu.inventory['data_offset'] + tensor['offset']
                data = model[start:start+rows*(width//128)*34]
                x = rng.normal(0, .7, width).astype(np.float32)
                results[name] = check_matrix(kernels, events, data, width, rows, x)
        print(json.dumps(results, indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2', context)


if __name__ == '__main__':
    main()
