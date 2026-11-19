# EXP-2026-11-19-1243: the architecture registry answers on a live model load

The architecture registry was one of the ported-but-never-called family: `llama-arch.cpp` held zero Goose
touchpoints while `goose/model/arch-registry.goose` (263 lines, four exported functions) sat in the unit.

Wired at the decision point the native code actually uses:

```c
llm_arch llm_arch_from_string(const std::string & name) {
    fg_check_arch(name);                     // <-- the ported registry
    for (const auto & kv : LLM_ARCH_NAMES) { ... }   // native answer is still what returns
```

The check asks the port to **find** the name and then **round-trip** it
(`ar_arch(ar_find(name)) == name`), which tests both functions without assuming anything about row order.

```
FG_ARCH_TOTAL calls=1 disagreements=0
```

One call, because the native function is called once per model load - and that is the honest count for a
load-time decision, not a reason to inflate it with a loop.

## Why this one is worth more than its count

- it is the first call path into a native file that had **zero** touchpoints (EXP-1241's map: loader, graph,
  context, model, quant all at 0; `llama-arch.cpp` was in that group);
- it proves the interface shape that the GGUF/loader family needs: the host passes a bounded byte slice and a
  small out buffer, and the port answers - no GGUF packing, no container types;
- the round-trip form (`find` then `arch`) is a check that cannot pass by luck: a wrong row would return a
  different name.

## What it does not do

It does not replace the native parser, and it does not touch the 17 000 lines of loader/graph/context/model/quant.
It is one decision on the path, verified live, in a family where nothing was.

## Next

The same shape applies to `llm_arch_to_str` and to the MoE recipe lookup behind `ar_find`'s rows - both take and
return small values. The GGUF packing problem remains the separate, larger interface decision.
