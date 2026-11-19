# EXP-2026-11-19-1350: the buffer form measured on the call sites that matter, before converting it

## Why measure first

The piece **cache** was converted with equality proven over all 248 320 tokens (EXP-2026-11-19-1348). The buffer form
`token_to_piece(token, buf, length, lstrip, special)` is a different call: it is what emits each token to the client,
it passes `special = false` for ordinary tokens, and it applies `lstrip`. None of those combinations is exercised by
the cache build, so the earlier equality says nothing about them.

## The measurement

A hook at the public `llama_token_to_piece` compares the door's bytes with the native ones on the exact flags each call
uses, and counts:

```
FG_TK_BUF_PROGRESS calls=9217 disagreements=0
```

9217 calls over three 512-token generations of the acceptance workload, and zero disagreements - including the
`special = false` path that returns zero for special tokens before the piece is written, and lstrip values above zero.

The response hash in that run was `c2ff42cc1219` and decode 62.19-62.85, so the measurement itself costs nothing
measurable.

## What is not converted yet, and the one subtlety

The conversion is the next step, and it has one thing to get right before it is safe: the native function returns
**`-needed`** when the caller's buffer is too small, and the door returns a byte count. A conversion that ignored that
would truncate long pieces silently instead of reporting the size - a wrong answer that no equality check on
successful calls would catch.

So the conversion keeps the native call for the negative case and serves from the door only when it can produce the
full piece. That is written down here rather than discovered later.

## Where this leaves P7-07

| piece | state |
|---|---|
| cache | served by the port |
| end of generation | served by the port |
| buffer form | **measured equal on 9217 calls**, not yet converted |
| sequence form | not measured |
