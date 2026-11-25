"""Measure full Flash stages and merges with once-per-layer packed Q."""
from pathlib import Path


experiments = Path(__file__).resolve().parents[2]
source = experiments / "EXP-2026-11-25-2011-bonsai-flash-direct-q4/source/check.py"
text = source.read_text()
a = text.index("MODES =")
b = text.index("\n\ndef run", a)
text = text[:a] + '''MODES = ("control", "packed", "direct", "linear")


def launch_panel(kernel, groups, tiles, args, arena):
    params = (P * len(args))(*(ct.addressof(arg) for arg in args))
    gpu.call("cuLaunchKernel", kernel, U(groups), U(tiles), U(1),
             U(256), U(1), U(1), U(arena), P(), params, P())
''' + text[b:]
text = text.replace("        q = alloc(q0.nbytes)", "        q = alloc(q0.nbytes)\n        half = alloc(groups * 2048 * 4)")
a = text.index("        def chain(mode):")
b = text.index("\n        ref = None", a)
text = text[:a] + '''        def chain(mode):
            component.dispatch(functions["pack"], (chunks, 4), 128,
                               (key, value, packed_k, packed_v, U(k), U(chunks)))
            if mode == "linear":
                component.dispatch(functions["panel_pack"], (groups, 1), 256, (q, half, U(n), U(groups // 4)))
            elif mode != "control":
                pairs = out_count // 2
                component.dispatch(functions["query_pack"], ((pairs + 255)//256, 1), 256, (q, half, U(pairs)))
            assert (groups // 4) % 2 == 0
            wave = capacity & ~1
            query = q if mode == "control" else half
            for start in range(0, groups, wave):
                count = min(wave, groups - start)
                params = U(n), U(first), U(tiles), U(start)
                args = (query, packed_k, packed_v, scratch, params[0], U(k), *params[1:], U(chunks))
                launch_panel(functions[mode], count // 2, (tiles + 1) // 2,
                             args, 33280 if mode == "direct" else 50176)
                component.dispatch(functions["merge"], (count * 16, 1), 128, (scratch, output, *params))
''' + text[b:]
a = text.index("    frozen,base,trial=")
b = text.index("    rng=", a)
text = text[:a] + '''    frozen, image = (component.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5])
    functions = {}
    names = {
        "row": "goose_set_q4_rows", "pack": "bonsai_flash_stage_kv",
        "query_pack": "bonsai_pack_half", "panel_pack": "bonsai_pack_query_panels", "merge": "bonsai_flash_compact_ordered_merge",
        "control": "bonsai_flash_kv_lean2", "packed": "bonsai_flash_q_packed_panel",
        "direct": "bonsai_flash_q_direct_panel", "linear": "bonsai_flash_q_linear_panel",
    }
    for label, name in names.items():
        f = P()
        gpu.call("cuModuleGetFunction", ct.byref(f), frozen if label == "row" else image, name.encode())
        if label in MODES:
            gpu.call("cuFuncSetAttribute", f, I(8), I(33280 if label == "direct" else 50176))
        functions[label] = f
''' + text[b:]
text = text.replace("((257,33),(4096,1024),(8192,1024),(16384,1024),(32768,1024),(32767,127))",
                    "((257,31),(8192,1024),(16384,1024),(32768,1024),(32767,1023),(32767,127))")
exec(compile(text, str(source), "exec"), {"__file__": str(source), "__name__": "__main__"})
