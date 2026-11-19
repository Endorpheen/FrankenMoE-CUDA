# EXP-2026-11-19-1351: the buffer form is served by the port, and the short-buffer case is why it took two steps

## The conversion

`llama_token_to_piece` - the call that emits each token to the client - now asks the port first and serves its bytes
when the door produces the whole piece within the caller's buffer:

```
FG_TK_BUF_SERVED from_port=6144
FG_TK_BUF_SERVED from_port=8192
decode  62.31 / 62.96 / 62.66
hash    c2ff42cc1219
```

Responses of 2255 characters, unchanged.

## The case that had to be handled first

The native function returns **`-needed`** when the buffer is short, so the caller can resize and retry. The door
returns a byte count. A conversion that served from the door unconditionally would **truncate silently** and report a
complete piece - a wrong answer that no equality test on successful calls would ever catch, because the successful
calls would still be equal.

So the door is used only when `served <= length`, and everything else falls back to the native call. The rule is
written in the code where the decision is made, not only in this card.

## Why the measurement came first

The buffer form passes `special = false` for ordinary tokens and applies `lstrip`; the cache build exercises neither.
The equality of the cache was therefore not evidence for this call, and EXP-2026-11-19-1350 measured 9217 real calls
with zero disagreements before this change was made.

## Where P7-07 now stands

| piece | state |
|---|---|
| piece cache (248 320) | served by the port |
| buffer form | **served by the port** |
| end-of-generation decision | served by the port |
| sequence form `bt_detokenize` | **not measured, not converted** |

One door remains on this side, and it is the only one in the whole report whose comparison has never been made.
