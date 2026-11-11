# EXP390: special-token partition decisions -- integrated, unreachable on this model

## Verdict

**GATED, INTEGRATED, NOT VALIDATED.** No live witness; the point is not counted as closed.

## Transferred

| op | rule |
|---|---|
| 0 | control and unknown tokens are ignored when special parsing is off |
| 1 | the left reminder is trimmed only for LSTRIP tokens and only while non-empty |
| 2 | the same on the right |

## Gate

| check | result |
|---|---|
| 33 draws over parse_special × attribute, strip bits × reminder lengths −2..4 | 33 checks / 0 failures |
| mutations | **5/5 caught** |

## Why no witness

`tokenizer_st_partition` runs on every non-empty tokenize, but its work is the loop over
`cache_special_tokens`. `FG390S_VOCAB_PARTITION` never fires, and the run's log carries no special-token
cache line at all, so the cache is empty for this model and the loop body never executes.

That is a property of the model/request on this profile — a raw completion prompt — not of the port. A
chat-template request, or a model that declares special tokens, would exercise it; neither is part of the
agreed profile, and I did not invent one.
