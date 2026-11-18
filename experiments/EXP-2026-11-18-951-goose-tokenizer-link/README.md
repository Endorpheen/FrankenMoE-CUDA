# EXP-2026-11-18-951: the tokeniser unit links into the shipping library

Status: build evidence. The library now carries the Goose tokenizer; the C++
bodies are still the ones that run, so nothing is removed.

## Hypothesis fixed before changes

EXP950 gave the tokenizer path a Goose entry (`btl_load`), but the shipping
`libllama.so.0` is not built from the standalone unit: `runtime/reconstructed/build.py`
compiles single Goose modules with per-module bridges and links those. A
composed unit had never been part of that link. Before adding one, the
hypothesis was that the tokenizer unit is symbol-safe to link beside the ten
existing Goose modules - and if it is, the shipping library can call it.

## What was measured first

The 11-module unit (reader, type tables, Unicode, pre-tokenizer, BPE engine,
`btl_load`, special partition) exports 67 non-static globals. Intersecting them
with the dynamic symbol table of the accepted library
(`apex-independent-fast-20261116/run/libllama.so.0`) leaves exactly one name:
`goose_init`, which is renamed to `fg950_goose_init` at compile time. The shared
prefixes (`gr_`, `bt_`, `ggh_`, `uc_`, `pt_`) do not occur in the accepted set,
so the unit does not shadow or duplicate anything.

## What was done

`runtime/reconstructed/build.py` gains one block: compose the 11 modules with
`tools/compose-goose.py`, compile the unit with the pinned Goose compiler, build
it with `gcc-13 -O2 -fPIC -Dgoose_init=fg950_goose_init`, and append the object
to the Goose object list that the `-Wl,-z,defs` link consumes. No C++ source and
no carrier file was touched.

## Result

| check | accepted library | this build |
| --- | --- | --- |
| build exit code | - | **0** |
| `libllama.so.0` bytes | 5 766 688 | **5 957 016** |
| defined dynamic symbols | 6493 | **6560** |
| symbols present before and gone now | - | **0** |
| symbols added | - | **67** |
| `btl_load` / `bt_build` / `gr_arr_str_table` | absent | **defined** |
| undefined new symbols | - | **none** |

The build compiled 10 single-module Goose bridges, the 11-module tokenizer unit,
3 SLRU objects and 186 vendored model sources, then linked with `-Wl,-z,defs`,
which fails on any unresolved symbol. The 67 added symbols are exactly the
unit's export set, so the change is additive at the ABI level.

## Not claimed

The library carries the engine; it does not use it. `llama-vocab.cpp` still
builds its vocabulary and tokenizes in its own body, so no measured behaviour
changes and no C++ is removed. P7-06/P7-07/P7-12 stay PARTIAL. No model was
started, and the accepted bundle was not touched: the build wrote to its own
directory.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp951-tokenizer-link-20261118
    python3 runtime/reconstructed/build.py --build-dir "$OUT/build" \
        --runtime-libs /home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
    nm -D "$OUT/build/libllama.so.0" | grep -E 'btl_load|bt_build|gr_arr_str_table'
    nm -D --undefined-only "$OUT/build/libllama.so.0" | grep -E 'btl_|gr_|bt_'

Log: `$OUT/build.log`.
