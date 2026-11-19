# EXP-2026-11-19-1244: the architecture registry is an eight-entry subset, and one sample hid it

The single-name check from the previous card passed - `FG_ARCH_TOTAL calls=1 disagreements=0` - on the model's
own architecture. A full sweep over every name in `LLM_ARCH_NAMES` says something else:

```
FG_ARCH_SWEEP names=163 disagreements=154
```

Nine of 163 round-trip. The reason is in the port itself:

```
goose/model/arch-registry.goose   (263 lines, 8 entries)
  "qwen3moe" "qwen2moe" "qwen35moe" "gemma4" "lfm2moe" "deepseek4" "bailingmoe3" "qwen4exp"
```

It is a **specialised registry for the MoE architectures of this project**, not the runtime's 163-name table.
For every other name `ar_find` returns -1.

## Why the first check looked green

Apex-35B's architecture is in those eight, so the one call the model makes found its row and round-tripped.
Everything else was never asked. That is the same defect as the grammar "identical output" of an earlier
session and the "no objects" of the dead-object check: **a sample that cannot fail is not evidence**.

The sweep costs 163 lookups once per load and answers a question the single call could not.

## What this means for P9-01

P9-01 is "port standalone config, recipe and architecture registry". What exists is the **recipe** side: the MoE
architectures the project actually runs. The architecture registry proper - all 163 names with their suffixes -
is not ported, and the previous card's "the registry answers" was true only for eight names.

## What is claimed now, exactly

- the ported subset answers correctly for the eight architectures it holds (nine round-tripped, including the
  model's own);
- it does not answer for the other 155;
- the hook stays, so a model of any other architecture shows a disagreement instead of passing silently.

## Next

Either port the remaining names from `LLM_ARCH_NAMES` (a data exercise, exact and gateable) or narrow the claim
and the door's name to what it is: an MoE recipe registry. The first is honest for P9-01; the second is honest
about today.
