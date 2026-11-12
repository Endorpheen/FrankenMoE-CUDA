# EXP264: MoE cache copy decisions wired into a copy of llama-moe-cache.cpp

Integration half of EXP-263. The module is gated; this experiment puts it on the real call path without touching the
source tree.

## What is replaced

Three sites in `src/llama-moe-cache.cpp`, each behind `fg263c_enabled()` so the native loop remains as the fallback:

| site | native | Goose |
|---|---|---|
| `copy_experts` run length | `while (slots[e+n] == slots[e]+n)` | `fg263c_run_from_slot` |
| `copy` fill run length | `while (fills[i+n] == (expert+n, slot+n))` | `fg263c_run_from_pairs` (reads the interleaved records directly, so the vector is not split) |
| `copy` stats bucket | `ids.size() <= 8*n_expert_used` | `fg263c_stats_class` |

The switch is default on: `GGML_GOOSE_MOE_CACHE_COPY=0` restores the native loops, as rule 10 requires.

## Checks

The modified file is compiled with the flint build's own command from `compile_commands.json`, and its undefined
symbol set is compared with the unmodified object's:

    compile rc = 0
    undefined added: fg263c_enabled, fg263c_run_from_pairs, fg263c_run_from_slot, fg263c_stats_class
    undefined removed: (none)

So the substitution adds exactly the four bridge symbols and changes no other dependency. The bridge defines all four
(`nm -g --defined-only bridge.o`), and the check `needed - defined = none` passes.

## Not yet run

This is the ABI half. A live run needs `libllama` rebuilt with this object and the bridge, staged beside the accepted
set, and the 3072 MiB cache configuration exercised; the accepted profile runs with `cache_n = 0` and never reaches
this code, so it cannot validate it. That run is the next step, and its gate is the cache profile's completion plus
the `FG263C` witness and `stats_copy` counters.

## Files

- `integration/llama-moe-cache.cpp` -- the modified copy
- `bridge.c`, `copy_decisions-functions.c` -- the exported module and its primitives
- `build_and_check.py`, `abi.json`, `compile.stderr`
