# EXP-2026-11-18-1000: The build from current sources works, and adds 94 symbols

Status: a DSO built from today's sources loads, exports the tokenizer engine and three
filters, and samples. The accepted library still contains none of them - the gap is
"not yet accepted", not "does not build". 38 319 checks.

## What EXP999 left open

EXP999 measured that the accepted `libllama.so.0` (2026-11-16 23:09) contains **zero**
exports of the canonical unit, and stated the next step as "extend `_CARRIER_UNIT` or
rebuild". It did not answer the prior question: **does a build from current sources
work at all?** If it does not, the choice is not a choice.

## The measurement

`python3 runtime/reconstructed/build.py --build-dir <new> --runtime-libs <accepted run>`
into an empty directory, exit 0:

    modules built: 36
    goose modules compiled from sources: 10
    tokenizer unit modules composed: 14
    slru objects compiled: 3
    models compiled from vendor: 186
    linked .../libllama.so.0 5997896

Then the artifact itself:

| measurement | accepted lib | fresh build |
| --- | --- | --- |
| `btl_load`, `bt_bpe`, `bt_token_to_piece`, `tp_apply`, `tns_apply`, `dry_apply`, `gr_parse` | absent | **present** |
| carrier-unit exports present | 0 | **72 / 72** |
| exported symbols total | 6 493 | 6 587 |
| symbols lost relative to accepted | - | **0** |
| symbols gained | - | **94** |

No symbol was lost. Of the 94 gained, 49 are Goose exports (`bt*`, `gr_*`, `abi_*`,
`tp_*`, `tns_*`, `dry_*`), and the rest are helper and bookkeeping symbols
(`llama_goose_*`, `goose_tok_enabled`, `fg950_goose_init`).

`hybriddna` looked like a C++ leak on first reading; it is present 9 times in the
accepted library too, so it is existing code, not something this build introduced. Worth
saying because the first glance said "new leak".

## The smoke test

`experiments/EXP-2026-11-18-1000-fresh-dso/tests/smoke.cpp` loads the fresh library with
`dlopen`, checks the six current-source symbols, then creates a dist sampler, applies it
to eight candidates, and checks that a token is selected and the probabilities sum to
one. 11 checks, 0 failures. No model, no weights, no server.

It is now gate `fresh-dso` in `tools/verify-unit-gates.sh`. Two things had to be fixed to
make it a gate rather than a one-off:

- the library has no `libllama.so` symlink, so it is linked by filename;
- `dlopen` needs the accepted run directory on `LD_LIBRARY_PATH` for `libggml*`, and the
  path was computed two levels up instead of one - the gate reported SMOKEFAIL while the
  same command worked by hand. A check that only passes manually is not a check.

## What this changes

The decision left open by EXP999 is now a real choice with two measured sides:

- the fresh build works, loses nothing and gains 94 symbols;
- the accepted artifact predates every module from 2026-11-18.

So "not yet accepted" is the accurate description, and accepting is a question of
measuring behaviour, not of making the build work. That measurement is an A/B on a
running server, which needs approval.

## Not claimed

A loadable library that samples is not a library that generates correctly. The smoke test
says the artifact is sound enough to try; it says nothing about output quality, prefill,
decode or memory.

The fresh build still uses 26 frozen bridge objects for decisions that have since moved
into the unit. 94 new symbols means the new work is *also* present, not that the old paths
are gone - both are in the binary, which is the shape rule 17 warns about and is the next
thing to resolve.

No model was started, no benchmark was run.
