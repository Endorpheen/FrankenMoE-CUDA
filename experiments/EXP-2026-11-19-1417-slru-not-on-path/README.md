# EXP-2026-11-19-1417: the shadow comparison never ran, which says something about the cache

## What was built

`plan()` in `llama-moe-cache.cpp` now snapshots its state, lets the native decision run, then runs the ported door on the
snapshot and compares fills, hit count and order - all behind `FG_SLRU_DIAG`.

The code is in the artefact: `strings libllama.so.0 | grep -c FG_SLRU` is 1, and the object was rebuilt.

## What happened when it ran

An acceptance run with the flag on:

```
decode 62.48 / 62.91 / 62.81   prefill 2697.9 / 2947.9 / 2953.4   hash c2ff42cc1219
```

and the server log contains **no `FG_SLRU` line at all** - not the counter, not a difference. `plan()` is never called on
this profile.

## Why that matters, and what it does not mean

It does not mean the cache is inactive: shrinking it from 4992 to 3072 MiB earlier cost 21 % of decode, so the cache
matters. It means the decision that matters on this profile is not taken in `plan()`.

The candidates, in order of plausibility:

1. `get_slot_map()` returns `nullptr` for these layers, so the graph never asks the cache and the 21 % came from a
   different mechanism - the resident mmap expert ring, which `GGML_MMAP_EXPERT_PIN` controls and which EXP1298 showed to
   be worth 43 % on its own;
2. `plan()` runs on a path only used when experts are not host-pinned, which `--cpu-moe` makes them;
3. the call is there but on a thread whose stderr is not captured - unlikely, since other counters from worker threads do
   appear.

## The one measurement that separates them

A print in `llama_moe_cache`'s constructor and in `get_slot_map()` reporting whether a slot map is returned, once per
layer at load. That is a single quiet line and it says immediately whether the cache is consulted at all for this model on
this profile.

## What this changes about P9-03

The item asks for 25 call sites in the expert cache to become Goose decisions. Before any of them can be replaced, the
first question is which of them run on the profile that is measured - and that question now has an answer: not the
policy's `plan()`. Replacing a decision that never executes would be a change with no effect and a report claiming
otherwise, which is the failure mode this project has paid for repeatedly.

## Shipped path

Inert: `FG_SLRU_DIAG` unset means no snapshot, no call, no allocation. Acceptance unchanged.
