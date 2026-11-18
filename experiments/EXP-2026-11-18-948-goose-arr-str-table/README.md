# EXP-2026-11-18-948: a NUL-terminated string-array table in the reader

Status: component gate PASS. The carrier wrapper is not wired from tracked
source yet, so this is a reader capability, not a closed requirement.

## Hypothesis fixed before changes

A consumer that receives GGUF string-array elements as raw spans cannot treat
them as C strings: the wire format stores strings length-prefixed with no
terminator, so the byte after the last character is the first byte of the next
element's 8-byte length word. The accepted workaround in
`exp925-tokenizer-wire-20261117` is a single export that lays a whole array out
as NUL-terminated entries in one pass. That export existed only outside the
repository. Porting it into the canonical reader should reproduce every element
byte for byte, and the size-query protocol should let a caller size its buffer in
one call.

## What was done

`goose/model/gguf-reader.goose` gains three exports that together let a host
answer the GGUF metadata questions without reading values from its own container:

- `gr_val_span(state, kv, index, out)` - wire type, payload offset, payload
  bytes and element type for one key;
- `gr_arr_str_span(data, state, kv, index, element, out)` - one string-array
  element as `(offset, length)` into the header window;
- `gr_arr_str_table(data, state, kv, index, text, offs)` - the whole array laid
  out as NUL-terminated entries. First pass walks the length words, writes
  `offs[i]` and accumulates `sum(len + 1)`; if `text` is smaller it returns
  `-(total + 1)`. Second pass copies each element and writes one zero after it.
  `-1` on a bad span, `-2` when `offs` is too small.

These are ports of the exports found in
`exp925-tokenizer-wire-20261117/obj/fg919.goose`; no new module and no adapter.

Source-of-truth check before editing: `runtime/reconstructed/src/llama-vocab.cpp`
and `runtime-build-tree-v8/our-src/llama-vocab.cpp` are byte-identical
(`sha256 665ac46d…`), so the tracked tree is the tree the accepted runtime was
built from. The EXP925 candidate copy (224207 bytes) is the only one carrying the
Goose tokenizer wiring, and it is still outside the repository.

## Gate

`tests/gate.py` builds one synthetic GGUF in memory (a string array with an empty
entry, a single ASCII byte, a 3-byte UTF-8 snowman, a 300-byte entry, and an entry
with an embedded NUL; plus an integer array and `general.alignment`), parses it
with the pure reader, and checks:

- the size query returns `-(total + 1)` and still publishes the offset table;
- the real call returns the element count and the exact offsets;
- every element's bytes and its single trailing zero;
- a short `offs` array returns `-2`; negative, out-of-range and `INT64_MAX`
  indexes and a non-string array return `-1`;
- `gr_arr_str_span`: every element's `(offset, length)` slices the header to the
  exact bytes; negative, past-the-end and `INT64_MAX` elements, an integer array
  and a bad key index are refused;
- `gr_val_span`: the array reports wire type 9 and element type 8; the scalar
  `general.alignment` reports wire type 4, absent element type `-1`, four payload
  bytes, and its offset really points at the value `32`; a bad index and a
  three-element output buffer are refused without writing;
- against native `libggml-base`, `gguf_get_arr_n` agrees and every element
  without an embedded NUL is byte-identical.

Build: `goose --standalone` on the composed 3-module unit (`gguf-headers`,
`gguf-tensors`, `gguf-reader`), then `gcc-13 -O1 -fPIC -shared -Wl,-z,defs`.
The library links libc only. `goose check` on the composed unit passes
(161 specializations).

| check | result |
| --- | --- |
| `goose check` on the composed unit | ok, 161 specializations |
| link | `-Wl,-z,defs` passes, `ldd` shows libc only |
| gate | **60 checks, 0 failures** |
| native agreement | count and all NUL-free elements identical |

## Recorded divergence, not a match

The entry `A 00 B` is stored with wire length 3. Native `gguf_get_arr_str`
returns only `A`: its container stops at the first embedded NUL and loses the
remaining wire bytes. The Goose table keeps all three bytes as the wire says.
The gate asserts this exact behaviour on both sides instead of pretending they
agree. For vocabularies without embedded NULs (Apex tokens and merges) the two
are identical.

## Not claimed

This does not close P7-06/P7-07/P7-12 and does not change the shipping path. The
carrier wrapper that calls this export (`llama-model-loader.cpp`, the
`arr_str` map with a model-lifetime NUL-terminated table) exists only in the
EXP925 candidate. Bringing that wrapper into tracked source is the next slice;
nothing may be deleted before it is wired and gated.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp948-arr-str-table-20261118
    python3 tools/compose-goose.py "$OUT/unit.goose" goose/misc/gguf-headers.goose \
        goose/model/gguf-tensors.goose goose/model/gguf-reader.goose
    $(python3 -c "import json;print(json.load(open('runtime/compiler/current.json'))['compiler'])") \
        check "$OUT/unit.goose"
    $(python3 -c "import json;print(json.load(open('runtime/compiler/current.json'))['compiler'])") \
        --standalone -o "$OUT/runtime.c" --header "$OUT/runtime.h" "$OUT/unit.goose"
    gcc-13 -O1 -g -fPIC -shared -Wl,-z,defs "$OUT/runtime.c" -lm -o "$OUT/reader.so"
    python3 experiments/EXP-2026-11-18-948-goose-arr-str-table/tests/gate.py \
        "$OUT/reader.so" --native /home/end0/AI/LlamaServer/maxwell-candidate-v10/run/libggml-base.so.0

No model was started and no runtime was rebuilt.
