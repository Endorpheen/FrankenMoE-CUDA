# EXP-2026-11-19-1314: 63 of the unit's 1192 exports reach the shipped library, and the detokenizer is not among them

Rule 33 says one growing cumulative build, and that a compile is not proof of presence. This measures the gap
directly, by comparing the composed unit's exports with the shipped library's.

## The numbers

```
build/goose-main/main.o, T exports          1192
libllama.so.0, T exports                    2379   (mostly native llama_* and ggml_*)
main-unit exports present in the library      63
main-unit exports absent from the library   1129
bt_* (detokenizer) in the shipped library       0
```

The 63 that do reach it are the ones a compiled unit actually needs at run time: the sampler doors, the speculative
doors, the tokenizer cross-check doors (`fg_tk_*`), the metrics doors (`mt_*`), the workload helpers (`wl_*`,
`lo_*`, `sc_*`), `cfg_validate`, `goose_init` and the artifact registry (`ar_*`).

Everything else is built and verified but never called by the shipped runtime. The most consequential member is
`bt_detokenize`: the detokenizer port sits in the unit, its module is listed in `tools/build-main-runtime.sh`, and no
C++ file calls it - the shipped server detokenizes natively.

## Why this is not a violation, and why it is still a debt

Nothing is lost by it: the native path is what runs, and it is verified by every acceptance run. But the migration's
own definition of covered is "the shipped binary's call path reaches the Goose export", and by that definition
detokenization is **not** covered - it is ported, verified in isolation, and unconnected.

## What this says about the accounting tool

`tools/migration-status.sh` reports 149 composed modules. It says so in its own header: the count is the composed
unit, not the coverage of the accepted binary. This card is the number that header warns about.

## Next step, named

Connect `bt_detokenize` the way host registration was connected in EXP-2026-11-19-1298: find the native call site in
`llama-vocab.cpp`, add a door resolved the same way `fg_tk_*` already is, and require equality on the generated text
of an acceptance run before the token hash is trusted. The detokenizer has 337 lines of ported logic and a
verification history; it needs a call path, not a rewrite.
