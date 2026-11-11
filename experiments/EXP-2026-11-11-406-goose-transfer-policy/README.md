# EXP406: transfer policy -- integrated, not executed on this request

## Verdict

**GATED, INTEGRATED, NOT VALIDATED.** Not counted.

## Transferred

| op | rule |
|---|---|
| 0 | a copy of more than zero bytes is issued asynchronously |
| 1 | which output is being copied: logits, embeddings or the next-token hidden state |
| 2 | for MoE expert copies, the distance to the end of the current expert |

## Gate

18 checks over copy sizes, output kinds and chunks | 0 failures
Mutations | **5/5 caught**

## Why no witness

`FG406S` never fires. The two call sites are in the block that extracts logits into `logits.data`, but
this server takes logits through the sampling path instead: with `n_probs = 5` the request never fills
`logits.data` on this context, so that block is skipped.

So the decision is correct and integrated, but this request shape does not reach it. A request that asks
for logits through this API would. Not counted.

## What this means for APEX.CUDA_CONTROL

The item's own mandatory routes (`FG195V`, `FG196P`) live in `libggml-cuda`, a different artefact that this
DSO does not build; they are live there. The remaining host/async lifecycle work therefore sits in that
artefact, and the transfer policy that could be moved inside `libllama` is not on this request's path.
Recorded as the item's boundary, not as progress.
