# EXP-2026-11-19-1394: the encoder serves, and the acceptance triple could never have shown it

## The serving path

`llama_tokenize` now returns the port's ids when the port answered and its answer is equal to the native one, behind
`FG_TK_BPE_SERVE` for its own A/B, and behind the pre-type guard established in EXP-2026-11-19-1392 - so another family
never reaches the port at all. `parse_special` is plumbed in, because with it false the native tokenizer treats special
markers as text and the port must do the same.

## First finding: the acceptance request carries token ids

An acceptance run with the encoder serving gave the same hash as the baseline - and that is **not evidence about the
encoder**. The request is

```
{"prompt": [5439, 425, 411, 2393, ...], ...}
```

a fixed token sequence, so the server never calls the tokenizer on the acceptance path. The hash is stable because the
encoder is not exercised there, not because it agrees. Reading it as a pass would have been the "two equally unexercised
builds" mistake this project has already paid for twice.

## The measurement that does show it

Same text prompt, same seed, temperature 0, one request each - the answer must be byte-identical:

| run | answer sha256 (first 16) | length |
|---|---|---|
| native encoder | `deb812577837a6f7` | 662 |
| ported encoder, serving | `deb812577837a6f7` | 662 |

and the served run's log confirms the port answered (`FG_TK_BPE_SERVED from_port=1`), so the equality is between two
actually different code paths, not between one path taken twice.

## What this establishes

On a text prompt through the server, with chat templates off and 128 generated tokens, the ported encoder and the native
one produce the same tokens and therefore the same text.

## What it does not

A 32K text prompt, chat templates on, and a multi-turn conversation are not covered here, and neither is the
`--no-bos` path for a vocabulary that actually adds BOS. The acceptance profile cannot cover any of them until a request
with a **text** prompt is part of it - which is the honest gap this card exposes rather than hides.

## State

| item | state |
|---|---|
| port serves `llama_tokenize` | yes, behind `FG_TK_BPE_SERVE` |
| pre-type guard | in place, refuses other families |
| equality on a text request | identical answer hash, served counter confirmed |
| acceptance-level proof | **not available** - the acceptance request has no text |
