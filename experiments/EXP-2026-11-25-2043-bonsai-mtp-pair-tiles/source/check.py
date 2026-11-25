"""Time and verify the full four-row tile and merge against scalar-query attention."""
from pathlib import Path


experiments = Path(__file__).resolve().parents[2]
source = experiments / "EXP-2026-11-25-2033-bonsai-mtp-attention-reuse/source/check.py"
text = source.read_text()
text = text.replace("range(3)", "range(4)").replace("(3, 24, 256)", "(4, 24, 256)")
text = text.replace("allocate(3 *", "allocate(4 *").replace("gpu.download(output, 3 *", "gpu.download(output, 4 *")
text = text.replace('"exact_output_rows": 3', '"exact_output_rows": 4')
a = text.index("        def run(mode):")
b = text.index("\n        def capture()", a)
text = text[:a] + '''        def run(mode):
            if mode != "serial":
                is_pair = mode.startswith("pair")
                heads = 3 if mode == "pair3" else 2
                args = (queries, k, v, scratch, U(first), U(max_tiles), F(1 / 16))
                params = (P * len(args))(*(ct.addressof(arg) for arg in args))
                gpu.call("cuLaunchKernel", kernels[mode], U(24 // heads), U(max_tiles),
                         U(2 if is_pair else 1), U(32), U(1), U(1), U(0), P(), params, P())
                dispatch(kernels["exact_merge"], (96, 1), 128, (scratch, output, U(first), U(max_tiles)))
                return
            for t, (keys, tiles) in enumerate(zip(active, tile_counts)):
                partial = D(scratch.value + t * row_bytes)
                result = D(output.value + t * 24576)
                heads = 3 if keys >= 32768 else 2
                dispatch(kernels[f"single{heads}"], (24 // heads, tiles), 32,
                         (D(queries.value + t * 24576), k, v, partial, U(keys), U(tiles), F(1 / 16)))
                dispatch(kernels["merge"], (24, 1), 32, (partial, result, configs[t], I(0)))
''' + text[b:]
text = text.replace('for mode in ("shared1", "shared2"):', 'for mode in ("control", "tail", "pair2", "pair3"):')
text = text.replace('("serial", "shared1", "shared2")}', '("control", "tail", "pair2", "pair3")}')
text = text.replace('("serial", "shared1", "shared2", "shared2", "shared1", "serial") * 3:',
    '("control", "tail", "pair2", "pair3", "pair3", "pair2", "tail", "control") * 3:')
old = '''            "shared1": function(image, "bonsai_verify_q4_gqa1"),
            "shared2": function(image, "bonsai_verify_q4_gqa2"),'''
assert text.count(old) == 1
text = text.replace(old, '''            "control": function(image, "bonsai_verify_q4_four_gqa2"),
            "tail": function(image, "bonsai_verify_four_tail2"),
            "pair2": function(image, "bonsai_verify_pair_tail2"),
            "pair3": function(image, "bonsai_verify_pair_tail3"),
            "exact_merge": function(image, "bonsai_verify_merge_parallel"),''')
exec(compile(text, str(source), "exec"), {"__file__": str(source), "__name__": "__main__"})
