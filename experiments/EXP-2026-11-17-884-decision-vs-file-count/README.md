# EXP-2026-11-17-884 — a file's Goose-call count does not measure how much of its decisions are native

While looking for where to wire `q35_plan`, the honest answer turned out to be "nowhere": the decision it
computes is **already answered by Goose** in the working runtime.

## The correction

EXP-883 concluded that Apex's layer kinds were "decided in C++ inside the shipped runtime". That was wrong, and
the check that disproves it is short:

```
llama-hparams.cpp:268  bool llama_hparams::is_recr(uint32_t il) const {
    if (fg200g_enabled()) {
        const int64_t r = fg200g_is_recr((int64_t)il, (int64_t)n_layer_all, ...);
        if (r >= 0) return r != 0;
    }
    ...
```

`llama-model.cpp:3227` and the memory-construction path both ask `hparams.is_recr(il)`. So the per-layer
attention/recurrent split - the heart of the plan - is a Goose answer today. `q35_plan` is a **second**
Goose implementation of the same decision, not a replacement for a C++ one. Wiring it in would add a
duplicate decider, which rule 17 forbids.

## Why the file count misled me

The measurement I had been using counts `fgNNN_*` occurrences per file:

```
llama-model.cpp   4095 lines   0 goose calls     <- looked like the biggest native surface
```

`llama-model.cpp` makes 336 references through `hparams.`, and `llama-hparams.cpp` is Goose-routed
(8 calls, `fg200g_is_recr` among them). A file can be a large *consumer* of decisions without containing any
of them. Counting call sites measures where the text is, not where the decision is made.

## What this changes

- The "next frontier is llama-model.cpp, 4095 lines with zero Goose" plan is withdrawn as stated. The right
  question is per decision, not per file: which decisions are still settled in C++, wherever their callers sit.
- `q35_plan`/`qg_split` are now composed into the DSO (EXP-883) and verified 41/10/30/1. That is still useful -
  the plan can answer from the shipped library - but they are **not** the wiring of a native path, and no call
  site will be added to duplicate `fg200g`.
- `fg200g` and `fg158` are the modules that actually own the graph decisions. Duplicating them is the risk to
  guard against, which is why the five extracted FFI functions were kept to exactly five symbols.

## Next, measured properly

Enumerate decisions rather than call sites: for each `is_*`/`select_*`/`can_*` predicate in the graph and
memory code, record whether its body is Goose or C++. That is the real native frontier, and it cannot be read
off a file's call count.
