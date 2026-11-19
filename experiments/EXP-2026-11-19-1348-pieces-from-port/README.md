# EXP-2026-11-19-1348: the token-piece cache is built by the port

## What was converted

`token_to_piece_for_cache` produces the piece for every one of the model's 248 320 tokens at load time, and that cache
is what turns a generated id into text. It now asks the port first:

```
FG_TK_PIECES_SERVED from_port=65536
FG_TK_PIECES_SERVED from_port=131072
FG_TK_PIECES_SERVED from_port=196608
```

The native producer remains as the fallback, and the diagnostic comparison from EXP-2026-11-19-1317 keeps running when
its flag is set.

## Why it could be converted without a risk period

The equality of `bt_token_to_piece` with the native producer was established over **all 248 320 tokens, byte for byte**
before this change. A conversion whose subject is already proven equal cannot introduce a difference; what it can
introduce is a *cost*, which is why the acceptance was run anyway:

```
decode  62.41 / 62.83 / 62.93
prefill 2689.4 / 2942.3 / 2945.6
hash    c2ff42cc1219
```

Median decode 62.83 against the 62.73 baseline, hash unchanged, no diagnostic diff line.

## Where this leaves the detokeniser side

| piece | status |
|---|---|
| per-token piece (cache) | **served by the port** |
| end-of-generation decision | served by the port (EXP-2026-11-19-1347) |
| buffer form `token_to_piece(token, buf, ...)` | native |
| sequence form `bt_detokenize` over ids | not measured, not converted |

The buffer form and the sequence form are the remainder, and the sequence form still needs its comparison before it can
be anything else.
