# EXP-2026-11-15-739 — the scheduler module that controls dependencies exists and is not wired

Base: `9a66f2c8`. Following the conclusion that the remaining cost is 12.6 pipeline drains per token -
a graph-construction property - this looks for the Goose module that owns graph scheduling.

## 1. What was found

`goose/cuda/goose-scheduler.goose`: **28 externs, 0 exports, not in the build set.**

Its header says exactly what it is:

```
// Full scheduler decisions/state transitions; imports project fields or execute effects.
extern "fg164_dep_slot"  fn dep_slot(dep, t) -> u64;
extern "fg164_optimize"  fn optimize(backend, graph, deps);
extern "fg164_expert"    fn expert(s, b, op, src, dst, value) -> u64;
extern "fg164_pointer"   ...
```

`dep_slot` computes a dependency slot, `optimize` takes a graph **and a deps table**, and `expert`
handles expert copies. This is the component that decides graph execution order - the thing whose
absence the last four cards measured as 4.83 s of GPU idle and 12.6 synchronisations per token.

## 2. It IS running, but from the native side

`FG164_SCHEDULER` appears in the live log with 21 of 64 mode bits set:

```
FG164_SCHEDULER modes=0x1400079f4003bf,live_scheduler_heaps=0
```

So a scheduler is executing - but the Goose module that would own those decisions is not in the build,
which means the calls come from the prebuilt library, not from a module we compile. Exactly the
situation EXP-725 found for the expert cache: the Goose source is present, the compiled artefact runs,
and the two are not connected.

**22 mode bits are unset**, including the block 10-21. Whether any of those correspond to a
copy/compute overlap path is unknown - it needs the module's own mode table, which is in the source
that is not compiled.

## 3. Why this is the right next component

Every measurement this session converges on the same component:

| measurement | what it implies |
|---|---|
| 4.83 s GPU idle of a 17 s window | work is not queued far enough ahead |
| 12.6 `cudaStreamSynchronize` per token | the CPU waits for drains it could avoid |
| 0% copy/compute overlap | copies and compute are ordered on one stream |
| 321 long stalls hold 75% of the wall time | a few dependency boundaries cost almost everything |

A scheduler that owns dependency slots and graph optimisation is precisely the mechanism that would
change those numbers, and it exists as Goose source in this repository.

## 4. What wiring it in requires, honestly

1. The module has **no exports** - an exported surface must be added, as was done for the tokenizer
   (EXP-683's approach: thin pass-throughs to the existing private functions).
2. Its 28 externs (`fg164_*`) must resolve. Twelve such adapters already exist in the prebuilt
   libraries; the question is whether they are reachable from a module we compile.
3. The scheduler is on the correctness-critical path: a wrong dependency slot means a tensor read
   before it is written. The failure mode is a wrong answer, not a slow one.

None of that is speculative - it is the same shape as the three components already wired
(model-graph, MoE dispatch, KV slot), each of which needed an export surface and a differential gate.

## 5. What this card is not

Not a claim that wiring it will be faster. The native scheduler may already make the same decisions;
the measured cost may be inherent to one-stream execution rather than to decision quality. That is
exactly what a differential gate would establish - and it does not exist yet.

This card records the component, its state, and why it is the next thing rather than another
parameter sweep. The parameter space is exhausted (EXP-738); this is the first lever in several
experiments that is a component rather than a setting.

## 6. Cost

Analysis only. No server run, no build.
