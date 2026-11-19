# EXP-2026-11-18-1001: 169 of 342 bridge exports are called by nobody

Status: the "both paths are in the binary" note from EXP1000 is now a number, per bridge
group, with the object graph behind it. Nothing was removed.

## Why this was needed

EXP1000 found 94 new symbols in a fresh build and wrote that both paths are present.
That is half a fact. A bridge object can be **linked and never called** - it then costs
size and hides what is truly missing - or it can be the **only implementation** some
models reach. Counting both as "in the binary" is how a runtime acquires two paths and no
audit, which is what rule 17 warns about.

## How it was measured

`tools/audit-bridge-symbols.py` reads the object graph, not the source text: a bridge
export is dead when no object that needs it lists it as undefined, **including the bridge
objects themselves**. Internal helpers a generated program calls stay alive.

Source grep was tried first and produced two conflicting answers; the object graph settled
it. Earlier cycles in this series recorded the same lesson, so the graph is what the tool
uses.

## Result

    bridge objects: 69 | their exports: 342
    only reachable from non-bridge objects: 166
    referenced by nobody: 169

| group | dead | of exports |
| --- | --- | --- |
| fg200g | 28 | 36 |
| fg199k | 18 | 33 |
| fg201l | 11 | 20 |
| fg263c | 5 | 9 |
| fg205g, fg233c | 4 each | 6 each |
| fg213p, fg215u, fg216e, fg217c, fg218t | 3 each | 4-5 each |

Spot-checked by hand rather than trusted:

- `fg199k_place_cell` - dead; nothing in `runtime/` or `integration/` mentions it outside
  its own bridge, and `llama-kv-cache.cpp.o` has zero undefined references to it;
- `fg199k_plan_cell` - live; `U fg199k_plan_cell` in the same object.

Same group, opposite verdicts: the measurement discriminates rather than repeating one
answer.

## A claim I made and then tested, and it was wrong

The first draft of this report said the audit might miss a bridge that is called only
behind an `fgNNN_enabled()` flag, because "the audit sees the compiled call, not whether
the flag was on". That is false, and one command showed it: a flag-guarded call is still
an undefined reference in the object file.

    $ nm -u llama-kv-cache.cpp.o | grep fg199k_plan_cell
                     U fg199k_plan_cell

`fg199k_plan_cell` is called behind `fg199k_enabled()`, and the reference is there
regardless of the flag's value at run time. So the 169 are unreferenced in **any**
configuration of this build - a stronger statement than the draft claimed, and the
weaker one would have undersold it while sounding cautious.

## What the number means, and what it does not

**Means:** 169 exported bridge symbols can be dropped from the link without any object
noticing, in every configuration of this build.

**Does not mean** the modules behind them are superseded. Several groups still have live
exports in the same object - `fg199k` has 15 live of 33. Removing the object would break
those; removal has to be per-symbol after reading what still calls in, which is the audit
rule 28 describes.

It also says nothing against the bridges that ARE called: `fg202t_*` and `fg213p_*` are
the live tokenizer path for every model that is not QWEN35, while the canonical unit
serves QWEN35 only. That is coverage, not duplication, and removing those bridges would
remove models.

## Result

| check | result |
| --- | --- |
| dead bridge exports | 169 of 342 |
| live from non-bridge objects | 166 |
| report | prints the count next to the two artifact numbers |
| files removed | none |

No gate is re-run: nothing under `goose/` changed and the DSO is the one EXP1000 built.

## Not claimed

The audit reports; it does not gate. Nothing was deleted: a symbol being uncalled says
nothing about whether its module's *decisions* are still needed elsewhere, and rule 15
forbids removal before the replacement is accepted. The 169 are a removal candidate list
for one pass after acceptance, not a verdict now.

The 342 counts bridge objects' exports only; the unit's own symbols are EXP1000's 72.

No model was started, no benchmark was run.
