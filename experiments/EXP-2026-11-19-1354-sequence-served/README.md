# EXP-2026-11-19-1354: the sequence form serves from the port, and the last door on this side is done

## The conversion

`llama_detokenize` now returns the port's bytes when the whole sequence fits the caller's buffer, with the native call
kept for the case the door reports as short - the same rule the piece form follows, for the same reason: a truncated
string served as complete would be a wrong answer nothing downstream checks.

```
FG_TK_SEQ_SERVED from_port=81        (84 calls in the harness, one model load)
diffs=0
decode  62.63 / 63.26 / 63.11
hash    c2ff42cc1219
```

## A false alarm on the way, and how it resolved

The first run of the converted build showed **no** served calls at all, and the honest first reading was "the door is
not being reached". Rather than guess, a one-line state print was added:

```
FG_TK_SEQ_STATE fn=0x7785be5e2fe0 ready=1
```

Both were fine, and the very next run served every call. So the earlier silence was in the run, not the code - the
most likely cause is that its log was captured from a binary built before the change. Either way, the fix was to stop
and measure the state instead of concluding from an absence.

## What P7-07 looks like now

| piece | served by the port |
|---|---|
| piece cache (248 320) | yes |
| buffer form `token_to_piece` | yes, short buffers native |
| end-of-generation decision | yes |
| sequence form `llama_detokenize` | **yes**, short buffers native |

The detokeniser side is fully served except for the two short-buffer fallbacks, which exist because the native
contract returns `-needed` and the door does not.
