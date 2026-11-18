# EXP-2026-11-18-952: the reader owner links into the shipping library

Status: build evidence. The shipping DSO now carries the reader and its
adapter; no C++ body was removed and no behaviour changed.

## Hypothesis fixed before changes

EXP951 linked the tokenizer unit into `libllama.so.0`, but the unit only borrows
bytes: something outside Goose has to open, `mmap` and keep the tables alive.
The reader owner existed only in the untracked EXP925 candidate, so a shipping
caller had nothing to call. Porting it into the repository, compiling it into the
DSO and measuring the symbol delta should show whether the mapping side can be
wired without disturbing the accepted ABI.

## What was done

New file `integration/goose-standalone/gguf-reader-owner.cpp` (225 lines, the
only new adapter). It holds:

- one `mmap` of the header window per `gguf_context` (capped at 64 MiB, the
  reader's own limit), the `fd`, and caller-owned tables sized well above Apex
  (4096 keys, 16384 tensors, 8192/32768 slots);
- a mutex and a map keyed by context, plus the model-lifetime
  NUL-terminated string table built by `gr_arr_str_table`;
- `register`/`unregister`, and pass-throughs for key lookup, tensor index, kv and
  array shape, payload spans and one string element.

It decides nothing about the format. Every answer comes from the reader, and the
only reason the file exists at all is blocker 1 in `CODEX-HANDOFF.md`: Goose
cannot declare `open`/`mmap` because the generated unit includes libc headers
that already declare them with a different prototype.

`runtime/reconstructed/build.py` compiles it with the same CXX flags as the
carrier sources and appends it to the object list.

The adapter is counted as debt by the project's own rule and by the tool:
`tools/migration-status.sh` picks up every `integration/goose-standalone/*.cpp`.

## Result

| check | before (EXP951) | now |
| --- | --- | --- |
| build exit code | 0 | **0** |
| `libllama.so.0` bytes | 5 957 016 | **5 971 200** |
| `llama_goose_register/ready/reader/arr_str` | absent | **defined** |
| undefined `gr_`/`btl_`/`bt_` symbols | none | **none** |
| adapter debt files | 5 | **6** |
| adapter debt lines | 1452 | **1677** |

The build still links with `-Wl,-z,defs`, so every reader symbol the adapter
calls is satisfied by the unit linked in EXP951.

## Not claimed

The library can now be asked, but nobody asks: `llama-model-loader.cpp` and
`llama-vocab.cpp` still read through `gguf_*`, and no C++ body was deleted. The
adapter adds 225 lines of debt, which is the correct accounting: it is a removal
target, not migration progress. P7-06/P7-07/P7-12 stay PARTIAL. No model was
started and the accepted bundle was untouched.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp952-reader-owner-20261118
    python3 runtime/reconstructed/build.py --build-dir "$OUT/build" \
        --runtime-libs /home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
    nm -D "$OUT/build/libllama.so.0" | grep llama_goose_register
    bash tools/migration-status.sh | sed -n '/Adapter debt/,/^$/p'

Log: `$OUT/build.log`.
