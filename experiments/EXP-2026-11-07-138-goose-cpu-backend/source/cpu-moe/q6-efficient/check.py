from pathlib import Path
import concurrent.futures
import ctypes
import json
import random
import re

r = Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
base = ctypes.CDLL(str(r / 'run/libggml-base.so.0.27.0'), mode=ctypes.RTLD_GLOBAL)
cpu = ctypes.CDLL(str(r / 'run/libggml-cpu.so.0.27.0'))
goose = ctypes.CDLL(str(r / 'build/libgoose-moe-q6-efficient.so'))
cpu.ggml_cpu_init()
fp = ctypes.POINTER(ctypes.c_float)
base.ggml_quantize_chunk.argtypes = [ctypes.c_int, fp, ctypes.c_void_p, ctypes.c_int64,
    ctypes.c_int64, ctypes.c_int64, fp]
base.ggml_quantize_chunk.restype = ctypes.c_size_t
cpu.quantize_row_q8_K.argtypes = [fp, ctypes.c_void_p, ctypes.c_int64]
cpu.ggml_vec_dot_q6_K_q8_K.argtypes = [ctypes.c_int, fp, ctypes.c_size_t,
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
goose.fg_selfhost_q6_tile.argtypes = [ctypes.c_int, ctypes.c_int, fp, ctypes.c_void_p, ctypes.c_void_p]
goose.fg_selfhost_q6_tile.restype = None
rng = random.Random(20261107)
cases = []
for n in [256, 512, 2048, 4096]:
    for rows in [1, 2, 7, 16]:
        for repeat in range(4):
            xf = (ctypes.c_float * (n * rows))(*[rng.uniform(-3, 3) for _ in range(n * rows)])
            yf = (ctypes.c_float * n)(*[rng.uniform(-3, 3) for _ in range(n)])
            x = ctypes.create_string_buffer(n // 256 * 210 * rows)
            y = ctypes.create_string_buffer(n // 256 * 292)
            base.ggml_quantize_chunk(14, xf, x, 0, rows, n, None)
            cpu.quantize_row_q8_K(yf, y, n)
            expected = (ctypes.c_float * rows)()
            out = (ctypes.c_float * rows)()
            for row in range(rows):
                target = ctypes.cast(ctypes.byref(expected, row * 4), fp)
                cpu.ggml_vec_dot_q6_K_q8_K(n, target, 0,
                    ctypes.byref(x, row * n // 256 * 210), 0, y, 0, 1)
            goose.fg_selfhost_q6_tile(n, rows, out, x, y)
            assert bytes(expected) == bytes(out), (n, rows, repeat)
            cases.append((n, rows, x, y, bytes(expected)))
def call(case):
    n, rows, x, y, expected = case
    out = (ctypes.c_float * rows)()
    goose.fg_selfhost_q6_tile(n, rows, out, x, y)
    return bytes(out) == expected
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    results = list(pool.map(call, cases * 8))
assert all(results)
c = (r / 'generated/cpu-moe-q6-efficient.c').read_text()
start = re.search(r'^static (?:float|gg_type_\d+) gg_function_1\([^;]*\) \{', c, re.M).start()
end = re.search(r'^static void gg_function_0\([^;]*\) \{', c, re.M).start()
report = {'status': 'PASS', 'single_thread_tile_cases': len(cases),
    'eight_thread_tile_calls': len(results), 'widths': [256, 512, 2048, 4096],
    'rows': [1, 2, 7, 16], 'all_bitwise_equal': True,
    'tile_body_global_stack_use': bool(re.search(r'gg_(?:get_stack|stacks|allocate|append)', c[start:end]))}
(r / 'results/cpu-moe-q6-efficient-correctness.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
