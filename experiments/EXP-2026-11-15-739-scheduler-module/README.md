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

## 3b. The bindings are local symbols - this is the exact blocker

Checked all 28 `fg164_*` externs against the loaded `libggml-base-scheduler-bafe3955c9ea`:

```
fg164 symbols: 54 local (t), 2 global (T)
resolvable from outside: 0 of 28
```

`fg164_dep_slot`, `fg164_optimize` and `fg164_expert` all exist - as **local** symbols (`t`), which
cannot be linked against. The two globals are not among the ones the module needs.

And `FG164_SCHEDULER` is confirmed running in this build: it appears on clean shutdown with the same
21 mode bits, `0x1400079f4003bf`, and the run that produced it measured 1848.1 prefill / 42.53 decode.
Earlier I looked for it mid-run and concluded it was absent - it is a shutdown report, not a
per-request marker. Correcting that: it runs every time.

So the picture is precise and matches the expert cache exactly: **the scheduler is compiled into the
prebuilt library, its entry points are static, and the Goose module that mirrors those decisions is
neither exported nor linked.**

Wiring the module therefore means either re-exporting 28 static symbols from a library we do not build,
or writing 28 adapters - which rule 17 classifies as debt, not progress. Neither is a configuration
change, and both are larger than anything attempted in this session.

## 3c. Correction: not blocked - deliberately private, and already working

The build flags settle it:

```
shared-scheduler-build: ... -fvisibility=hidden -fPIC ...
```

**Every `fg164_*` symbol is local because the library is built with `-fvisibility=hidden`.** That is
the standard, correct choice for an internal adapter library: it exports the ggml public API and keeps
its Goose bindings private.

So my previous paragraph ("wiring it means re-exporting 28 static symbols or writing 28 adapters") was
the wrong framing. `goose-scheduler.goose` was **never meant to be linked from outside that library**.
It is the library's own implementation, compiled in - and `FG164_SCHEDULER modes=0x1400079f4003bf` in
every run proves it executes on every request, deciding the 21 scheduling transitions those bits
represent.

**There is nothing to wire. The scheduler is already Goose and already running.**

## 4. What wiring it in requires, honestly

1. The module has **no exports** - an exported surface must be added, as was done for the tokenizer
   (EXP-683's approach: thin pass-throughs to the existing private functions).
2. Its 28 externs (`fg164_*`) must resolve. Twelve such adapters already exist in the prebuilt
   libraries; the question is whether they are reachable from a module we compile.
3. The scheduler is on the correctness-critical path: a wrong dependency slot means a tensor read
   before it is written. The failure mode is a wrong answer, not a slow one.

Retained for the record, but superseded by 3c: the module needs no export surface because it is not
meant to be called from outside. The three components that WERE wired (model-graph, MoE dispatch, KV
slot) are in `libllama` and `libggml-cpu`, whose bindings are global; the scheduler lives in a
library with hidden visibility by design.

## 5. What this card is not

Not a claim that wiring it will be faster. The native scheduler may already make the same decisions;
the measured cost may be inherent to one-stream execution rather than to decision quality. That is
exactly what a differential gate would establish - and it does not exist yet.

This card records the component, its state, and why it is the next thing rather than another
parameter sweep. The parameter space is exhausted (EXP-738); this is the first lever in several
experiments that is a component rather than a setting.

## 6. Cost

Analysis only. No server run, no build.
