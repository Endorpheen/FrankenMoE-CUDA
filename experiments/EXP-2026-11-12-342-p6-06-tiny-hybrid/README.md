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

## Round three: names, not shapes, are now the obstacle

The generator writes 43 tensors and the GGUF reader confirms 43 unique names, but the loader reports `expected 43, got 36` -- it created
only thirty-six, and it emitted no "unknown tensor" or "unused tensor" warning to say which seven it discarded. Shape errors are gone;
what remains is a name mismatch between our set and the architecture's expected set.

That makes the iterative method the wrong tool for this last step: the loader tells us the *count*, not the names, so each round only
confirms that seven names are still wrong. The systematic fix is to derive the exact list from the architecture's tensor mapping in
`llama-arch.cpp` / the model's tensor-creation code for `qwen35moe`, and generate against that list instead of guessing names and letting
the loader count them.

Also worth recording: a duplicate guard was added to `T()` during this pass, and it found **no** duplicates in the current generator --
the earlier "43 written, 36 unique" confusion came from the loader's count, not from the file.

## Honest status

The file loads past metadata, past dtype and shape checks, and fails only on the final count. It is not yet a fixture: no token has been
produced from it, and the seven name mismatches are known to exist but not yet identified.

## Round four: the loader's own create_tensor log is the authoritative name list

Running with `-v` makes the loader print one `create_tensor: loading tensor <name>` line per tensor it intends to create. That is the
expected set, and comparing it against our GGUF is decisive:

- **in our GGUF but never created** (seven): `blk.0.attn_k_norm.weight`, `blk.0.attn_q_norm.weight`, `blk.0.ssm_d`,
  `blk.1.attn_gate.weight`, `blk.1.attn_k.weight`, `blk.1.attn_q.weight`, `blk.1.attn_v.weight`.
- **created but absent from our GGUF**: the whole `.scale` / `.input_scale` family -- `blk.*.attn_qkv.scale`,
  `blk.*.ffn_gate_exps.input_scale` and so on.

Two conclusions follow. First, the architecture expects `attn_qkv` for every layer, not separate q/k/v in a non-recurrent layer, so my
"one recurrent, one full-attention" split does not match how this arch is built. Second, it expects microscaling metadata tensors for
every quantizable weight, which our F32-only generator never provides.

The `expected 43, got 36` count is still not fully explained by these sets (the created list is longer than 43), and I am recording that
as an open discrepancy rather than inventing an explanation for it.

## Next step

Generate exactly the names from the `create_tensor` list, provide the `.scale`/`.input_scale` tensors, and let the shape driver settle the
rest. The list is captured in this experiment, so the next pass starts from it rather than from another guess round.

## The tiny hybrid loads, serves, and traces -- the fixture exists now

After the name list was taken from the loader's own `create_tensor` log (80 entries), 15 names it no longer asked for were dropped
(separate `ffn_gate_exps`/`ffn_up_exps` and the attention biases), the shape driver settled every remaining dimension, and one runtime
assertion had to be fixed: `rope.dimension_sections` cannot be all zeros, so it is `[6, 6, 4, 0]` for a 16-wide head.

The model now loads and serves on **both** lines:

    native: listening, completion answered
    ours:   listening, completion answered

and with the traced `libllama` from EXP340 it produces a **190-node** graph whose hybrid path is visible and attributable:

    cache_r_l0 (reshaped) / (view) ...   six nodes carrying the recurrent layer's cache
    conv_output_silu                      per recurrent layer

That is the fixture P6-06 asked for: small enough to assert on node by node, and small enough to read.

## Remaining to close P6-06

Turn the observations into assertions: for this model, layer 0 must contribute its `cache_r_l*` and conv nodes and layer 1 must not, and
the same must hold on both lines. Generation returns an empty string because zero weights make greedy decoding pick token 0, so the
fixture's value is the graph, not the text -- and that is what the item asks for.
