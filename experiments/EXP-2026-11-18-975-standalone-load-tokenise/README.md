# EXP-2026-11-18-975: the standalone calls its own tokenizer engine

Status: load-and-tokenise path proven in the standalone, 25 checks against a
recorded oracle. This closes the "in the artifact but nothing calls it" line.

## Hypothesis fixed before changes

EXP974 put `btl_load`, `bt_bpe` and the sampler filters into
`frankenmoe-v4`, but nothing in the standalone called them. If a caller performs
the sequence the shipping library performs - mmap, `gr_parse`, `btl_load`, then
`bt_bpe` - the ids it produces must equal what the accepted libllama produces for
the same text and the same file.

## What was done

`integration/goose-standalone/tests/tokenize_load.c` is that caller:

1. `mmap` the GGUF header window;
2. `gr_parse` over it, with table sizes taken from the container header itself
   (read `n_tensors` and `n_kv` at offsets 8 and 16) rather than guessed;
3. `gr_find_kv` for `tokenizer.ggml.tokens` and `tokenizer.ggml.merges`, then
   `gr_arr_n` for the counts - so the tables are sized by the reader;
4. `btl_load` to build the BPE tables;
5. `bt_bpe` per corpus string.

The expected ids are **recorded from the accepted library**, not recomputed here:

    llama_tokenize(add_special=false, parse_special=true) on the same file:
    "abc"      -> 257
    "abc ab a" -> 257 32 256 32 97
    "a b c"    -> 97 32 98 32 99
    "aab"      -> 97 256
    "  x"      -> 32 32 120
    ""         -> (none)

Recording them keeps libllama out of the gate's link, so the standalone build needs
no llama library. The provenance is this section.

## Result

| check | result |
| --- | --- |
| gate | **25 checks, 0 failures** |
| container read | 13 keys, 0 tensors, 4167-byte window |
| reader counts | 260 tokens, 2 merges |
| `btl_load` | rc=0, 260 tokens, 2 merges, 1 special, 1 eog |
| ids vs oracle | identical for all six corpus strings |
| build integration | `tokenize_load` is built by `build-standalone-runtime.sh` |

The build script runs it when `FG_TOKENIZE_FIXTURE` names a vocabulary, and prints
the result inline:

    gate:   tokenize_load PASS on /tmp/tok29/vocab.gguf

## Mechanical notes, each met rather than assumed

- linking `main.o` alone fails with hundreds of undefined host symbols; the gate
  needs `main-gc.o` (function sections, `--gc-sections`) plus the host primitives,
  exactly as the old tokenizer gate did;
- `goose_host.c` needs `-I builds/goose-standalone/inc` for the staged
  `goose-unit.h` shim;
- the gate calls `goose_init`, not `fg950_goose_init`: in the standalone the unit is
  the only Goose image, so there is no rename. This is the opposite of the shipping
  DSO, where the rename exists to avoid colliding with per-module inits - the two
  builds differ here and the gates must match their own build.

## Not claimed

The standalone still has no CLI that a user drives: this is a gate, not a shipped
entry point, and the status line says "called by the load-and-tokenise gate", not
"a tokenizer CLI exists". CUDA, HTTP transport and the full 41-layer execution
remain absent.

Four unit modules still export nothing (`goose-tokenizer`, `goose-model-graph`,
`goose-model-loader`, `cuda/dispatch`); unchanged from EXP974, still recorded and
not removed.

No model was started: the fixture is 4167 bytes of metadata, and the gate runs on
`mmap` plus the composed unit.

## Reproduction

    python3 experiments/EXP-2026-11-18-954-goose-vocab-wire/tests/make_vocab.py /tmp/vocab.gguf
    FG_SUBSTRATE=/tmp/opencode/sub-668/flint-cpu/build-off/ggml/src \
    FG_TOKENIZE_FIXTURE=/tmp/vocab.gguf \
        bash tools/build-standalone-runtime.sh
    # the oracle, run once and recorded:
    #   llama_tokenize on /tmp/vocab.gguf with the accepted library
