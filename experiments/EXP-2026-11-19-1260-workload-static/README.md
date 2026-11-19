# EXP-2026-11-19-1260: the first verified piece of the deterministic workload family

`DeterministicWorkload` is pure - `fstream`/`sstream` only - so a white-box include reaches it with no ggml, no
llama and no model. `check_static`, the guard that a replay only proceeds against the model the workload was
captured from, is ported as a three-way agreement:

```
workload check_static: cases=2000 fails=0
```

2000 random cases against the real class: capture metadata, then a "current model" that agrees on 2 of 3 fields
two thirds of the time, so both outcomes are exercised.

## What this is worth, said plainly

Not much on its own: the decision is three comparisons ANDed, and a port of it can hardly be wrong. Its value is
that it establishes the **route** for this family - include the native class, exercise it directly - and that
the workload family had nothing verified before.

## What was attempted and deliberately not committed

A full port of the file parser (`load()`: magic, version, key order, three counted sections, route records with
an internal `n_ids == top_k * token_n` consistency rule). It compiled, but the interface I hacked together
returned position and value packed into one integer, and the word reader carried dead arithmetic from the
iteration that produced it. Committing that would have been a second `xl_unlink`-style mistake: code that passes
its own test and is unreadable.

The parser is worth doing properly, with a struct-free but explicit interface (position and value as separate
out-parameters), and it is the next slice here.

## What the parser needs to be gated

The native `load()` reads a file; the harness writes crafted files - one valid, then one per failure mode
(header, version, each key, each truncation, a route with `n_ids` inconsistent with `top_k * token_n`) - and
compares the native outcome with the ported rule index. Eight failure modes, eight files, no model.
