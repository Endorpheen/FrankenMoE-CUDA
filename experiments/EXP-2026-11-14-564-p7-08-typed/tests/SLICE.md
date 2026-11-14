# SLICE.md — delta against the first draft of EXP565

The normative report is `REPORT.md`. This page records only what changed after
the draft was accepted: 12 pairs and 14/14 mutations became 24 pairs and 21/21,
the escaper was replaced, and the copy moved to the current canon head.

## Base moved

The draft sat on canon `0971716d…`. The repository head moved to `c49c095d…`
during the review (+1983 appended lines: `legacy_*`, `peg_map_gemma4`,
`peg_map_minimax`, `g_esc_inner`/`g_esc_quoted`). The slice now sits on the head;
the superseded copy is kept as `base/runtime-vintage-0971.goose`.

```
base/runtime.goose    c49c095d7e497cb0349e6c4992483dc2ada6f8ffc02964c6b0024cf6954f042e
source/runtime.goose  3be19f7fe73250d3ee79f3caca444d7f75659c00d9f6067c402d855f47e736e3
```

## Escaping fixed

The draft's private `esc_json` wrote `\xNN` above 0x7e, which is not JSON. The
head canon already has `g_esc_quoted` / `g_esc_inner`, the nlohmann-compatible
writer; the slice now calls it and keeps only a strict UTF-8 check of its own
(`utf8_ok_strict`). Native behaviour, measured with `tests/escape_probe.cpp`:

```
Привет      -> d0 9f d1 80 d0 b8 d0 b2 d0 b5 d1 82   (raw UTF-8)
🙂          -> f0 9f 99 82                           (raw UTF-8)
0x01        -> \u0001        0x0b -> \u000b        0x7f -> 7f (raw)
c0 80       -> THROW type_error.316
d0 (tail)   -> THROW incomplete UTF-8
ed a0 80    -> THROW invalid UTF-8 byte 0xA0
```

Pairs P17–P19 now match those bytes exactly; P20 checks that the port rejects
the three invalid forms with a negative return, the counterpart of the throw.

Probe link line:

```
g++-13 -O1 -std=c++17 -I$L/common -I$L/include -I$L/src -I$L/ggml/include \
  -o build/escape_probe tests/escape_probe.cpp $D/chat.cpp.o $D/json.cpp.o \
  -L$L/build-cuda/bin -lllama-common -lllama -lggml -lggml-base -lmtmd \
  -Wl,-rpath,$L/build-cuda/bin
```

## Branches that were uncovered

| branch | pairs | mutation |
|---|---|---|
| `n == 0`, both caps | P13 | 14 |
| `n == 0`, only_typed wrap | P14 | 15 |
| join + only_typed, text+media | P15 | 16 |
| join + only_typed, text+media+text | P16 | 16 |
| control-byte escaping | P19, P19b | 17 |
| raw UTF-8 above 0x7e | P17, P18 | 18 |
| invalid UTF-8 rejected | P20 ×3 | 19, 20 |

## Separators

P21 settles the question the draft left open: the oracle is the compact
`nlohmann::dump()`. The new entry equals it byte-for-byte; the module's
`chat_message_json` (spaced `", "` / `": "`) does not, while parsing to the same
value. Compact stays.

## Build

`-O1` for the gate, `-O0` for mutations. `-O2` needs 5m15s on the generated C and
is out of the 5-minute budget. Final gate: 3m39s, `pairs=24 values=34
failures=0`.

## Why the two canon functions are wrappers

Goose rejects `export fn` → `export fn` calls with `could not resolve one
concrete function signature`. `chat_concat_parts` and `chat_contains_media` were
therefore split into a private body plus a wrapper, so the normalizer can reuse
them instead of duplicating the join and the media scan. Signatures and
behaviour are unchanged; 51 base exports, 53 after the slice.
