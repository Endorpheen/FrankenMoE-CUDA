"""Compare the five-row Attention partitions to the exact single-query oracle."""
from pathlib import Path


experiments = Path(__file__).resolve().parents[2]
source = experiments / "EXP-2026-11-25-2033-bonsai-mtp-attention-reuse/source/check.py"
text = source.read_text()
text = text.replace("range(3)", "range(5)").replace("(3, 24, 256)", "(5, 24, 256)")
text = text.replace("allocate(3 *", "allocate(5 *").replace("gpu.download(output, 3 *", "gpu.download(output, 5 *")
text = text.replace('"exact_output_rows": 3', '"exact_output_rows": 5')
a = text.index("        def run(mode):")
b = text.index("\n        def capture()", a)
text = text[:a] + '''        def run(mode):
            if mode != "serial":
                grid_x, grid_z = (8, 3) if mode == "221" else (12, 2)
                args = (queries, k, v, scratch, U(first), U(max_tiles), F(1 / 16))
                params = (P * len(args))(*(ct.addressof(arg) for arg in args))
                gpu.call("cuLaunchKernel", kernels[mode], U(grid_x), U(max_tiles), U(grid_z),
                         U(32), U(1), U(1), U(0), P(), params, P())
                dispatch(kernels["exact_merge"], (120, 1), 128, (scratch, output, U(first), U(max_tiles)))
                return
            for t, (keys, tiles) in enumerate(zip(active, tile_counts)):
                partial = D(scratch.value + t * row_bytes)
                result = D(output.value + t * 24576)
                heads = 3 if keys >= 32768 else 2
                dispatch(kernels[f"single{heads}"], (24 // heads, tiles), 32,
                         (D(queries.value + t * 24576), k, v, partial, U(keys), U(tiles), F(1 / 16)))
                dispatch(kernels["merge"], (24, 1), 32, (partial, result, configs[t], I(0)))
''' + text[b:]
text = text.replace('("shared1", "shared2"):', '("221", "32"):')
text = text.replace('("serial", "shared1", "shared2")}', '("221", "32")}')
text = text.replace('("serial", "shared1", "shared2", "shared2", "shared1", "serial") * 3:',
                    '("221", "32", "32", "221") * 3:')
old = '''            "shared1": function(image, "bonsai_verify_q4_gqa1"),
            "shared2": function(image, "bonsai_verify_q4_gqa2"),'''
assert text.count(old) == 1
text = text.replace(old, '''            "221": function(image, "bonsai_verify_five_221"),
            "32": function(image, "bonsai_verify_five_32"),
            "exact_merge": function(image, "bonsai_verify_merge_parallel"),''')
exec(compile(text, str(source), "exec"), {"__file__": str(source), "__name__": "__main__"})
