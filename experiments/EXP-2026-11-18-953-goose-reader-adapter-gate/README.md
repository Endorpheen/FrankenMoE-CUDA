# EXP-2026-11-18-953: the reader adapter agrees with the native container

Status: differential gate PASS, 45 checks. Nothing calls the adapter from the
shipping path yet, so this proves the migration face, not a behaviour change.

## Hypothesis fixed before changes

EXP952 linked `integration/goose-standalone/gguf-reader-owner.cpp` into
`libllama.so.0`, but only the link was verified, not the answers. If the adapter
and the Goose reader are correct, every question the native `gguf_*` API answers
on a file must come back identically through the adapter on the same file -
including absent keys, array element counts, payload bytes and string elements.

## What was done

`tests/gate.cpp` opens one file twice: once as a native `gguf_context`
(`gguf_init_from_file`, `no_alloc`), which is the independent oracle, and once
through `llama_goose_register` on the same context. It then compares, for every
key in the file:

- the key's index from `gguf_find_key` against `llama_goose_find_kv`;
- the value type and, for arrays, the element type and element count;
- for string arrays, every element text via `strcmp`;
- for numeric arrays, the byte count and the payload bytes via `memcmp`;
- an absent key and an absent tensor, which must both report `-1`;
- tensor names, for the first 64 tensors;
- `llama_goose_ready` after registration and after `llama_goose_unregister`.

The fixture is a synthetic GGUF with nine keys and three F32 tensors: two string
arrays, an integer array, a u64 array whose elements exceed 32 bits, a bool, a
f32, a u32, plus `general.alignment`. It is generated in memory by the existing
EXP948 helper, so no model is involved.

The gate links against the EXP952 build of `libllama.so.0`, not the accepted
bundle, because that is the library that contains the adapter.

## Result

| check | result |
| --- | --- |
| native open of the fixture | 9 keys, 3 tensors |
| adapter ready after register | yes |
| comparisons | **45 checks, 0 failures** |
| unregister clears readiness | yes |

The native oracle printed nothing about a mismatch, so key order, types, counts,
string texts and payload bytes all agree. This is the first evidence that the
adapter is a faithful face onto the reader rather than merely a linkable object.

## Not claimed

`llama-model-loader.cpp` and `llama-vocab.cpp` still read through `gguf_*`; the
adapter is called only by this gate. No C++ body was removed, and the 225-line
adapter stays on the debt ledger. P7-06/P7-07/P7-12 stay PARTIAL. No model was
started.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp953-reader-gate-20261118
    NEW=/home/end0/AI/LlamaServer/exp952-reader-owner-20261118/build
    LIB=/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
    g++-13 -O1 -g -std=c++17 -w \
        experiments/EXP-2026-11-18-953-goose-reader-adapter-gate/tests/gate.cpp \
        -Ivendor/flint/ggml/include "$NEW/libllama.so.0" "$LIB/libggml-base.so.0" \
        -Wl,-rpath,"$NEW" -Wl,-rpath,"$LIB" -o "$OUT/gate"
    "$OUT/gate" /tmp/exp953-fixture.gguf
