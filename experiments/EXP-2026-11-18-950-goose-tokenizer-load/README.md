# EXP-2026-11-18-950: the tokeniser load path, in Goose

Status: component gate PASS, 284 checks. The shipping server still calls the
C++ tokenizer, so the requirements stay PARTIAL.

## Hypothesis fixed before changes

The engine (`bpe-tokenize.goose`) and the reader (`gguf-reader.goose`) could
each answer their own question, but nothing joined them: a host had to find
`tokenizer.ggml.tokens`, `tokenizer.ggml.merges` and `tokenizer.ggml.token_type`,
index the two string arrays, split each `(off, len)` pair stream, call
`bt_build`, read the token types, then run `bt_attrs` and `bt_special_order` in
that order. That is a decision about the GGUF format, so it belongs in Goose, not
in a C++ carrier where it would be adapter debt.

## What was done

A new module `goose/model/bpe-load.goose` exports `btl_load`, which performs
exactly that sequence and reports a stage code instead of failing silently:

- finds the three keys through the reader;
- sizes everything from `gr_arr_n` and refuses a short caller buffer with
  `-2` and the stage in `stats[6]`, before writing anything;
- indexes tokens and merges, splits the pairs, and calls `bt_build`;
- reads `token_type` element by element, then `bt_attrs`, `bt_special_order`
  and `bt_eog_ids`;
- reports counts in `stats`: tokens, merges, duplicate tokens replaced,
  duplicate merges ignored, special count, eog count.

Calling an exported function from inside the unit does not typecheck: an export
must keep one concrete signature. The established pattern in the reader is a
private body with a forwarding export, so it was applied to the six functions
this path calls (`gr_find_kv`, `gr_arr_n`, `gr_string_array_index`,
`gr_array_int`, `bt_build`, `bt_attrs`, `bt_special_order`, `bt_eog_ids`).
The bodies are unchanged - the diff is additions only, no deleted line - and the
C ABI is untouched.

The module joins `tools/build-main-runtime.sh`.

## Measurements

| check | before | after |
| --- | --- | --- |
| modules in the canonical unit | 135 | **136** |
| `goose check` specializations | 4206 | **4219** |
| `goose check` result | ok | **ok** |
| gate | - | **284 checks, 0 failures** |
| bodies changed by the export/impl split | - | **0 lines** |

The gate (`tests/gate.py`) builds one synthetic vocabulary in memory with the
GPT-2 byte alphabet (256 byte tokens plus `ab`, `abc`, one control token), then:

- runs the manual sequence exactly as the gated EXP920 gate does it, and runs
  `btl_load` on the same bytes;
- requires identical token and merge tables, identical hash slots, identical
  token types, identical attributes, identical special order;
- tokenises `abc ab a` through both table sets and requires the same ids, with
  the first id being `abc` (257), which proves a merge actually ran;
- refuses a short offset buffer with `-2` and stage 1, and a short `stats`
  with `-1`.

## Not claimed

The load path is now Goose and tracked, but nothing in the shipping library
calls it: `llama-vocab.cpp` still tokenises in its own body. P7-06, P7-07 and
P7-12 therefore stay PARTIAL. The unit also needs host primitives at link time,
so a full link was not attempted here; generated C and the object-level
duplicate check are recorded in `gen.log`.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp950-bpe-load-20261118
    mapfile -t MODS < <(sed -n '/^MODULES=(/,/^)/p' tools/build-main-runtime.sh \
        | grep -oE 'goose/[a-z0-9/_-]+\.goose')
    python3 tools/compose-goose.py "$OUT/main/unit.goose" "${MODS[@]}"
    $(python3 -c "import json;print(json.load(open('runtime/compiler/current.json'))['compiler'])") \
        check "$OUT/main/unit.goose"
    # gate library: the ten modules the tokenizer path needs
    python3 experiments/EXP-2026-11-18-950-goose-tokenizer-load/tests/gate.py "$OUT/gate/lib.so"

No model was started.
