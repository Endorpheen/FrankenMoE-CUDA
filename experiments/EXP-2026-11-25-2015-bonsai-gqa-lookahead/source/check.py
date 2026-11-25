"""Reuse the EXP-2005 exact Q4 decode fixtures with look-ahead variants."""
from pathlib import Path

exp = Path(__file__).resolve().parents[2]
source = exp / "EXP-2026-11-25-2005-bonsai-decode-gqa/source/check.py"
code = source.read_text()
variants = {
    22: "bonsai_gqa_h2_k2_v0", 32: "bonsai_gqa_h3_k2_v0",
    34: "bonsai_gqa_h3_k4_v0", 221: "bonsai_gqa_h2_k2_v1", 321: "bonsai_gqa_h3_k2_v1",
}
code = code.replace("import numpy as np", "import numpy as np\nvariants = " + repr(variants))
code = code.replace("24 // heads, tiles", "24 // int(str(heads)[0]), tiles")
code = code.replace("for heads in (1, 2, 3):", "for heads in (2, 3, *variants):")
code = code.replace("(0, 1, 2, 3, 3, 2, 1, 0)", "(0, 2, 3, *variants, *reversed(variants), 3, 2, 0)")
code = code.replace("(0, 1, 2, 3)", "(0, 2, 3, *variants)")
code = code.replace("    events = component.cuda_event()", "    funcs.update({h: function(trial, name) for h, name in variants.items()})\n    events = component.cuda_event()")
code = code.replace("(65, 257, 1024, 8192, 32768, 33279)", "(65, 257, 4097, 8192, 16384, 32768, 33279)")
old = "        def run(heads):"
new = """        copies = [(k,v)]
        size = gpu.S(keys*576)
        for _ in range(int(sys.argv[5]) - 1 if len(sys.argv) > 5 else 0):
            next_k, next_v = reserve(keys*576), reserve(keys*576)
            gpu.call("cuMemcpyDtoD_v2", next_k,k,size)
            gpu.call("cuMemcpyDtoD_v2", next_v,v,size)
            copies.append((next_k,next_v))
        turn = 0

        def run(heads):
            nonlocal turn
            read_k, read_v = copies[turn % len(copies)]
            turn += 1"""
assert code.count(old) == 1 and code.count("(q, k, v,") == 2
code = code.replace(old, new).replace("(q, k, v,", "(q, read_k, read_v,")
assert code.count("funcs.update") == 2
exec(compile(code, str(source), "exec"), {"__file__": str(source), "__name__": "__main__"})
