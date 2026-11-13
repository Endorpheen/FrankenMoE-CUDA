# Provenance audit: every linked object has a committed source (2026-11-10)

The route audit asks *what the runtime contains*. This one asks the complement: **can the object in
the runtime be rebuilt from what is committed?** That gap is what let EXP219/220 lose modules — the
artefact looked authoritative and its provenance had drifted.

## Method

For each object that `runtime/exp220/run/libllama-exp220.so` links, read its undefined route
symbols with `nm -u`, then confirm a committed integration source in `experiments/*/integration/`
provides them. The working tree is clean, so "committed" means "in git".

| object | route symbols | committed source |
|---|---|---|
| `exp200/build/llama-hparams.o` | `fg200g_` | EXP200 `integration/` (3 files) |
| `exp200/build/llama-graph.o` | `fg200g_` | same |
| `exp213/build/llama-model-loader.o` | `fg201l_` | EXP201 `integration/llama-model-loader.cpp` |
| `exp210/build/llama-kv-cache.o` | `fg199k_ fg204x_ fg205g_ fg206d_ fg209i_` | EXP209 `integration/llama-kv-cache.cpp` + `llama-kv-cells.h` |
| `exp210/build/llama-memory-recurrent.o` | `fg210k_` | EXP210 `integration/llama-memory-recurrent.cpp` |
| `exp220/build/unicode.o` | `fg202t_ fg215u_ fg216e_ fg217c_ fg218t_` | EXP220 `integration/unicode.cpp` |
| `exp220/build/llama-vocab.o` | `fg202t_ fg213p_` | EXP220 `integration/llama-vocab.cpp` |

Every object maps to a committed source. No orphan.

## The one defect this found

`experiments/EXP-2026-11-10-209-.../integration/llama-kv-cache.cpp` was **modified and uncommitted**.
It is the source of the cumulative five-route KV object that the chain already ships. Committed as
`88a3963` after verifying it compiles clean with the project's own `compile_commands.json` entry and
that the resulting object's undefined-symbol set is exactly the five prefixes above — identical to
the shipped object.

Without that commit, a binary in the live chain had no committed provenance. That is the same class
of drift as EXP219/220, caught this time before it caused a loss.

## What is still not covered

Rebuilding the object from the committed source and comparing **machine code** to the shipped object
was done for symbol sets, not byte-for-byte. The objects were compiled by different commands at
different times, so byte equality is not expected and would not be the right criterion. The
meaningful check — same routes, same sources, source compiles — is what the table establishes.

Two audits now exist and they answer different questions:

- `route-audit-2026-11-10.md` — what the runtime **runs** (presence and execution per module);
- this file — whether the runtime's objects **come from committed sources**.

Neither proves the modules compute the right values; that is the 17-fixture gate run in
`gates-2026-11-10.md`.
