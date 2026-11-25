"""Check the three device bridge kernels against the existing Goose numeric unit."""

import ctypes as ct
import json
import sys
from pathlib import Path
from random import Random
from statistics import median
from time import perf_counter_ns

ROOT = Path(__file__).resolve().parent
WIDTH, LANES = 2304, 128
F, D, P, U, I, B = ct.c_float, ct.c_uint64, ct.c_void_p, ct.c_uint, ct.c_int, ct.c_size_t
RNG = Random(1957)
CUDA = ct.CDLL("libcuda.so.1")
REF = ct.CDLL(str(ROOT / "reference.so"))
REF.i2_candidate_init.argtypes = [I, P]
REF.i2_candidate_init(0, None)


class Slice(ct.Structure):
    _fields_ = [("data", P), ("length", ct.c_int64)]


def view(array, length=None):
    return Slice(ct.addressof(array), len(array) if length is None else length)


def cuda(name, *args):
    code = getattr(CUDA, name)(*args)
    if code:
        raise RuntimeError(f"{name} returned CUDA status {code}")


def load_kernel(module, name):
    kernel = P()
    cuda("cuModuleGetFunction", ct.byref(kernel), module, name.encode())
    return kernel


def launch(kernel, blocks, values):
    args = (P * len(values))(*(ct.addressof(value) for value in values))
    cuda("cuLaunchKernel", kernel, U(blocks), U(1), U(1), U(LANES), U(1), U(1), U(0), P(), args, P())


def buffer(count):
    pointer = D()
    cuda("cuMemAlloc_v2", ct.byref(pointer), B(count * ct.sizeof(F)))
    return pointer


def upload(destination, array):
    cuda("cuMemcpyHtoD_v2", destination, array, B(ct.sizeof(array)))


def download(source, count):
    array = (F * count)()
    cuda("cuMemcpyDtoH_v2", array, source, B(ct.sizeof(array)))
    return array


def random_vector(length, sigma):
    return (F * length)(*(RNG.gauss(0.0, sigma) for _ in range(length)))


def mismatch_count(a, b):
    a_bits = (U * len(a)).from_buffer(a)
    b_bits = (U * len(b)).from_buffer(b)
    return sum(x != y for x, y in zip(a_bits, b_bits))


def run(steps):
    cuda("cuInit", U(0))
    device = I()
    cuda("cuDeviceGet", ct.byref(device), I(0))
    context, module = P(), P()
    cuda("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        cuda("cuModuleLoad", ct.byref(module), str(ROOT / "attention-bridge.cubin").encode())
        prepare, score, mix = (load_kernel(module, "i2_bridge_" + name) for name in ("prepare", "score", "mix"))
        attention, weights, history, residual, next_row, normalized, past, query, scores, output = (
            buffer(size) for size in (WIDTH, 4 * WIDTH, 3 * WIDTH, WIDTH, WIDTH, WIDTH, 11 * WIDTH, WIDTH, 12, WIDTH)
        )
        begin, end = P(), P()
        cuda("cuEventCreate", ct.byref(begin), U(0))
        cuda("cuEventCreate", ct.byref(end), U(0))
        cancellation = (F * WIDTH)()
        cancellation[0], cancellation[1], cancellation[2] = 2.0**60, 1.0, -2.0**60
        ones = (F * WIDTH)(*([1.0] * WIDTH))
        upload(normalized, cancellation)
        upload(query, ones)
        launch(score, 1, [past, normalized, query, scores, I(0)])
        assert download(scores, 1)[0] == 0.0
        report = []
        for rows in range(12):
            for add in range(2):
                hi = random_vector(3 * WIDTH, 0.7)
                hw = random_vector(4 * WIDTH, 0.12)
                hp = random_vector(max(1, rows * WIDTH), 1.0)
                hq = random_vector(WIDTH, 0.8)
                hr = random_vector(WIDTH, 1.3)
                for destination, source in zip((history, weights, past, query, residual), (hi, hw, hp, hq, hr)):
                    upload(destination, source)
                wall_us = []
                diffs = dict(history=0, residual=0, normalized=0, mixed=0)
                first_output_mismatch = None
                prep_args = [attention, weights, history, residual, next_row, normalized, I(add)]
                score_args = [past, normalized, query, scores, I(rows)]
                mix_args = [past, normalized, scores, output, I(rows)]
                for step in range(steps):
                    ha = random_vector(WIDTH, (0.2, 0.8, 2.0)[step % 3])
                    if steps > 12:
                        hq = random_vector(WIDTH, (0.1, 0.8, 3.0, 8.0)[step % 4])
                        upload(query, hq)
                    cpu_next, cpu_norm, cpu_out = ((F * WIDTH)() for _ in range(3))
                    prob = (F * (rows + 1))()
                    upload(attention, ha)
                    start = perf_counter_ns()
                    assert REF.inkling_row_major_i2_postconv_token(view(ha), view(hw), view(hi), view(cpu_next)) == WIDTH
                    if add:
                        for i in range(WIDTH):
                            cpu_next[i] += hr[i]
                    assert REF.inkling_row_major_i2_rms_gain(view(cpu_next), Slice(None, 0),
                                                           F(1e-6), view(cpu_norm)) == WIDTH
                    assert REF.i2_depth_mix_cached(view(hp, rows * WIDTH), view(cpu_norm), view(hq),
                                                   view(prob), view(cpu_out)) == 1
                    if not add:
                        wall_us.append((perf_counter_ns() - start) / 1000)
                    launch(prepare, 1, prep_args)
                    launch(score, rows + 1, score_args)
                    launch(mix, WIDTH // LANES, mix_args)
                    for name, data, expected in (("history", history, hi), ("residual", next_row, cpu_next),
                                                 ("normalized", normalized, cpu_norm), ("mixed", output, cpu_out)):
                        actual = download(data, len(expected))
                        misses = mismatch_count(actual, expected)
                        diffs[name] += misses
                        if name == "mixed" and misses and first_output_mismatch is None:
                            first_output_mismatch = {
                                "step": step, "max_abs_error": max(abs(x - y) for x, y in zip(actual, expected)),
                                "scores": list(download(scores, rows + 1)),
                            }
                gpu_us = []
                for _ in range(5):
                    cuda("cuEventRecord", begin, P())
                    for _ in range(60):
                        launch(prepare, 1, prep_args)
                        launch(score, rows + 1, score_args)
                        launch(mix, WIDTH // LANES, mix_args)
                    cuda("cuEventRecord", end, P())
                    cuda("cuEventSynchronize", end)
                    elapsed = F()
                    cuda("cuEventElapsedTime", ct.byref(elapsed), begin, end)
                    gpu_us.append(elapsed.value * 1000 / 60)
                report.append({"history_rows": rows, "add_residual": bool(add), "tokens": steps, "mismatch": diffs,
                               "median_resident_gpu_us": median(gpu_us), "gpu_us_samples": gpu_us,
                               "median_goose_cpu_us_without_residual_add": median(wall_us) if wall_us else None,
                               "first_output_mismatch": first_output_mismatch})
        print(json.dumps(report, indent=2))
    finally:
        cuda("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    run(int(sys.argv[1]) if len(sys.argv) > 1 else 12)
