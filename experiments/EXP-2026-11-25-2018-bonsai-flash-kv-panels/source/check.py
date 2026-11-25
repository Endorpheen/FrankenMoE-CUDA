"""Measure shared K/V panels through the existing full Flash chain fixture."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = root / "EXP-2026-11-25-2011-bonsai-flash-direct-q4/source/check.py"
s = source.read_text()
a = s.index("MODES =")
b = s.index("\n\ndef run", a)
s = s[:a] + '''MODES = ("base2", "base4", "even2", "even4", "panel2", "panel4", "lean2", "lean4")


def launch_panel(kernel, groups, tiles, args, arena):
    params = (P * len(args))(*(ct.addressof(arg) for arg in args))
    gpu.call("cuLaunchKernel", kernel, U(groups), U(tiles), U(1),
             U(256), U(1), U(1), U(arena), P(), params, P())
''' + s[b:]
a = s.index("        def chain(mode):")
b = s.index("\n        ref = None", a)
s = s[:a] + '''        def chain(mode):
            component.dispatch(functions["pack"], (chunks,4), 128,
                               (key,value,packed_k,packed_v,U(k),U(chunks)))
            aligned = mode.startswith(("even", "panel", "lean"))
            panel = mode.startswith(("panel", "lean"))
            assert (groups // 4) % 2 == 0
            wave = (capacity & ~1) if aligned else capacity
            for start in range(0, groups, wave):
                count = min(wave, groups-start)
                params = U(n),U(first),U(tiles),U(start)
                group = int(mode[-1])
                ntiles = (tiles + group - 1) // group
                args = (q,packed_k,packed_v,scratch,params[0],U(k),*params[1:],U(chunks))
                if panel:
                    assert count % 2 == 0
                    launch_panel(functions[mode], count // 2, ntiles, args, 50176 if mode.startswith("lean") else 61952)
                else:
                    component.dispatch(functions[mode], (count,ntiles),128,args)
                component.dispatch(functions["merge"], (count*16,1),128,(scratch,output,*params))
''' + s[b:]
s = s.replace("ref = dst, partial", "ref = dst").replace("assert (dst,partial) == ref, (mode,k,n)",
    "assert dst == ref, (mode,k,n)\n            if mode.startswith('even'): aligned_ref[mode[-1]] = partial\n            if mode.startswith(('panel','lean')): assert partial == aligned_ref[mode[-1]], (mode,k,n,'partial')")
s = s.replace("        ref = None", "        ref = None\n        aligned_ref = {}")
a = s.index("    names = dict(")
b = s.index("    rng=", a)
s = s[:a] + '''    names = dict(row="goose_set_q4_rows",pack="bonsai_flash_stage_kv",merge="bonsai_flash_compact_ordered_merge",
        **{label:f"bonsai_flash_{'kv_panel' if label.startswith('panel') else 'tile_group'}{label[-1]}"
           for label in MODES})
    for suffix in ('2','4'): names['lean'+suffix] = 'bonsai_flash_kv_lean'+suffix
    for label,name in names.items():
        f=P()
        module=frozen if label=="row" else trial if label.startswith(("panel","lean")) else base
        gpu.call("cuModuleGetFunction",ct.byref(f),module,name.encode())
        if label.startswith(("panel","lean")):
            gpu.call("cuFuncSetAttribute",f,I(8),I(50176 if label.startswith("lean") else 61952))
        functions[label]=f
''' + s[b:]
s = s.replace("(257,33)", "(257,31)")
exec(compile(s,str(source),"exec"),{"__file__":str(source),"__name__":"__main__"})
