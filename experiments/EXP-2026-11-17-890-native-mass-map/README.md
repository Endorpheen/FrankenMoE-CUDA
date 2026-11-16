# EXP-2026-11-17-890 — where the remaining C++ actually is: 15,739 native function lines

Decision coverage (EXP-885) and native mass are different measurements and disagree on purpose. A file can have
zero decision coverage while its decisions live elsewhere - `llama-model.cpp` asks 336 questions through
`hparams`, which is Goose-routed - and a file can have high decision coverage while still holding thousands of
lines of plumbing. Charting the migration by either number alone picks the wrong next batch.

`tools/native-mass.py` counts the second thing: top-level function bodies containing no `fgNNN_`/`fg_xxx_` bridge
call and no `goose` reference, in lines.

## The map

```
native lines   fns file lines  file
        3790   103       4096  llama-model.cpp
        2133   143       4486  llama-sampler.cpp
        1900   145       4875  llama-context.cpp
        1852   127       4925  llama-vocab.cpp
        1469    29       1729  unicode.cpp
        1455    75       4553  llama-graph.cpp
         940    70       3517  llama-kv-cache.cpp
         811    35       1522  llama-memory-recurrent.cpp
         495    40       1870  llama-batch.cpp
         417    19       2006  llama-model-loader.cpp

total native function lines: 15739
```

## Reading it

`llama-model.cpp` is the largest single mass and the least suitable next batch: its 103 native functions are
mostly tensor plumbing and hparam loading, and its decisions are already answered by Goose through `hparams`.
Converting it would move lines without moving decisions.

`llama-hparams.cpp` is the opposite trap, and it is the one I nearly fell into. Its remaining "decisions" are
per-layer flag accessors and two small loops. `is_swa(il)` is called per layer per token; routing each call
through an FFI boundary to raise a coverage counter would be a straight performance loss. If those move at all,
it is as one Goose computation that fills the flag arrays at load time, with the hot predicates still reading
memory - never as a per-call bridge.

`unicode.cpp` is the best next batch on the evidence: **29 functions holding 1469 native lines**, i.e. large,
self-contained, pure algorithms. Nine of them are `unicode_regex_split_custom_*` - per-architecture
pre-tokenisation splitters, 1138 to 169 lines each, about 1000 lines together:

```
169  unicode_regex_split_custom_kimi_k2
146  unicode_regex_split_custom_k2_horizon
139  unicode_regex_split_custom_llama3
133  unicode_regex_split_custom_qwen2
127  unicode_regex_split_custom_qwen35      <- the architecture Apex runs
116  unicode_regex_split_custom_gpt2
 72  unicode_regex_split_custom_afmoe
```

One of them, `_qwen35`, is exercised by the pinned profile, so the batch can be checked end to end against the
reference hash rather than only by unit equivalence.

## Next batch

All nine splitters as one Goose module, with a differential gate over a corpus of strings (including the CJK,
combining-mark and emoji cases each architecture's rules distinguish), then one Apex run for the whole batch.
The `_qwen35` path is the end-to-end witness; the other eight get the differential gate only, which is exactly
the gap that a mis-spliced body would hide - so the gate is per-splitter, not per-module.
