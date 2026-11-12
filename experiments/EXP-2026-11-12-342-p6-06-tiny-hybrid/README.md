# EXP342: a minimal hybrid GGUF for the P6-06 fixture -- in progress

P6-06 needs a hybrid model small enough to assert on node by node. None exists in the tree, so this experiment builds one: a
`qwen35moe` with two layers, the first recurrent and the second full-attention, `n_embd` 32, four experts with two used, vocabulary 64.

## Method: let the loader dictate the tensor list

Rather than guess the full tensor set from the architecture code, the generator is iterated against the real loader. Each run reports the
next problem, which is a fact rather than a guess:

| run | loader said |
|---|---|
| 1 | `token_embd.weight` wrong shape -- GGUF stores dimensions reversed relative to the numpy array |
| 2 | `output.weight` wrong shape -- its logical size is `(n_embd, n_vocab)`, like the embedding, not the transpose |
| 3 | `blk.0.post_attention_norm.weight` not found |
| 4 | `blk.0.ssm_conv1d.weight` not found |

So four constraints are settled and the discovery is at the recurrent layer's SSM tensors. The file is 95 KB and loads far enough to
reach tensor checking, which is what makes the loop fast.

## Not finished, and what remains

The recurrent layer's `ssm_*` set, then whatever the attention layer and the output path require, then a run that actually produces tokens.
Only after that does the model become the fixture P6-06 asks for -- and the node-by-node assertions are the point, not the file itself.

The generator is committed as it stands so the loop can be resumed from the last discovered constraint instead of from the start.

## Round two: the loop is automated, and where it stalls

`iterate.py` now drives the cycle by itself: run the loader, parse `tensor 'X' has wrong shape; expected a, b, ...`, rewrite the
generator's line for X (mapping `blk.N.suffix` to `pre+"suffix"`), regenerate, repeat.

It settles the SSM shapes quickly -- `ssm_conv1d.weight` (4, 64), `ssm_dt.bias` (4,) -- and then stalls on
`ffn_gate_exps.weight`, expecting (32, 16, 4). The stall is a limitation of the driver, not of the model: that tensor is written in the
generator inside a loop over two names (`for e in ("ffn_gate_exps.weight", "ffn_up_exps.weight")`), so the rewriter cannot find a
single line to fix and keeps reporting the same expectation.

Two failures of my own are also fixed in this pass and worth recording: an earlier insertion branch had added duplicate
explicitly-named tensors next to the `pre+` versions, so the file carried two definitions of the same tensor and the last write won; and
the first version of the driver matched only literal names, so it never touched any per-layer tensor.

## Next step

Rewrite the generator to use one explicit `T(pre+"...")` line per tensor -- no loops over names -- and let `iterate.py` finish the
remaining shapes. That is mechanical and bounded: each round is a few seconds because the file is 95 KB.
