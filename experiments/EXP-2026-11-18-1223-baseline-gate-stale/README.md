# EXP-2026-11-18-1223: the cumulative gate looks for an artefact the design stopped producing

## The discrepancy

`tools/check-cumulative-baseline.py` requires a file named `libmoe_mmid_tiled.so` in the run directory:

```python
if not (d / "libmoe_mmid_tiled.so").exists():
    print("MISSING Goose kernel object libmoe_mmid_tiled.so")
```

Nothing in this tree produces that file. `find` over `builds/`, `runtime/`, `releases/`, `Backup/`,
`integration/` and `reference/` returns nothing, and the only occurrence of the name anywhere is the
check itself.

## What the design actually does now

The kernel was integrated **at source level** (tasks.json, P1-12):

```
vendor/flint/ggml/src/ggml-cpu/tiled/tiled.cpp:1168
static bool ggml_goose_mmid_q4_k(const struct ggml_compute_params * params, ...)
```

and it is reachable from the type dispatch at line 1188. The built library carries its identity string:

```
strings builds/flint-acc/bin/libggml-cpu.so | grep -c GOOSE_MMID_Q4K   -> 2
```

So the component **is** present, in a form the gate does not look for: a static function inside
`libggml-cpu`, not a separate object.

## Why the gate was not changed here

It guards an accepted component, and rule 15/31 say a gate is not weakened to make a run pass. Editing it
to accept "the symbol is in libggml-cpu" is a **legitimate** correction, but it is a change to an
acceptance gate, and that is Igor's call, not mine.

## What is claimed instead, and it is exact

- the three Goose components the gate recognises by library content are present in the measured build;
- the Q4_K tiled kernel is present as a source-level integration, evidenced by its definition site, its
  call site and its identity string in the built `.so`;
- the gate's fourth item is stale with respect to that design and needs either an updated rule or, if the
  separate object is still intended, a producer for it.

## Proposal for the owner of the gate

Replace the artefact check with a content check on `libggml-cpu.so` (the string it already prints at
runtime), which is the same kind of check the other three components use, and keep a note that the
separate object is no longer part of the shipped set.
