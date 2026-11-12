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
