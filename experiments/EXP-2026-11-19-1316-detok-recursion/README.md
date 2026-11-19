# EXP-2026-11-19-1316: my detokenizer hook crashed the server, and the cause is instructive

## What happened

The second attempt at wiring the detokenizer cross-check hooked `token_to_piece(token, buf, ...)` and called
`token_to_piece_for_cache(token, true)` to obtain the native piece for comparison. The server died while loading the
model:

```
goose runtime error: native call stack overflow
```

`token_to_piece_for_cache` is implemented in terms of `token_to_piece`, so the hook called itself. The failure is
mine, not the port's, and it is the most severe of this session's instrument errors because it broke the runtime
rather than producing a wrong number.

## What was done immediately

`llama-vocab.cpp` restored from the committed revision, rebuilt, and the runtime re-verified before anything else was
concluded:

```
pre-flight   accepted components 11 of 11
decode       62.40 / 62.83 / 62.85
prefill      2693.5 / 2942.7 / 2948.4
hash         c2ff42cc1219
```

The build is healthy. Its library hash differs from the EXP1298 checkpoint because the unit now also composes the
detokenizer closure (EXP-2026-11-19-1314), which is a recorded, accepted composition change.

## The correct design, now clear

Compare inside `token_to_piece_for_cache` itself - the one place that produces the native piece - and call only the
door from there. No path may call a piece function to obtain the reference, because the reference is what that
function computes.

## The rule, in its sharpest form yet

Three instrument errors in this thread so far produced wrong numbers. This one took the server down. The check that
catches it is not a review: it is starting the thing afterwards. Every change to a hot path is verified by a run
whose verdict is a hash, not by the fact that it compiled.

## Status

The detokenizer doors are composed and unused. The cross-check is not wired, the crash is recorded with its cause,
and the next attempt starts from `token_to_piece_for_cache` with no re-entry.
