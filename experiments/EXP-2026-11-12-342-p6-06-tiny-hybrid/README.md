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

## The gate exists and is green; its sensitivity is not yet proven

`check_fixture.py` runs our line against the tiny model with `FG_TRACE_GRAPH=1` and asserts five things: the trace was produced, the
graph has more than a hundred nodes, layer 0 contributes exactly one `conv_output_silu`, layer 0 has `cache_r_l0` nodes, and layer 1 has
none of them.

    checks=5 failures=0

Two attempts to prove the check is sensitive both **failed to load**, so neither counts as a mutation:

- three layers with two recurrent ones (107 tensors) -- the replicated names and the `recurrent_layers` array disagree, and the loader
  rejects the model before any graph exists;
- two layers with `recurrent_layers = [false, false]` -- the loader then wants `attn_q/k/v` on layer 0 where the original model has
  `attn_qkv`, so the tensor set no longer matches.

The lesson is the same as elsewhere in this session: the model's tensor names are coupled to its recurrent flags, so a mutation of the
flag alone produces an unloadable model rather than a changed graph. A valid mutant has to be generated with the matching tensor set --
for example a two-layer all-attention model with `attn_q/k/v` on both layers -- and that is the next step. Until then the green result
above shows the fixture works, not that the check can fail.

## Sensitivity attempt: the all-attention mutant, and how far it got

The first two mutants were unloadable, so the next attempt builds a proper one: the same two-layer model with
`recurrent_layers = [false, false]`, no `ssm_*` tensors, and `attn_qkv` on both layers -- because the account above learned that this
architecture uses the fused `attn_qkv` on every layer regardless of the recurrent flag.

Driving it against the loader settled several details the hybrid model never needed:

- a non-recurrent layer expects `attn_qkv` with width 96 (three projections) instead of 64;
- it also expects `attn_output.weight`, and per-layer `attn_q_norm` / `attn_k_norm` of width 16;
- the final count is 55, and the current builder produces 52.

The `create_tensor` diff names the rest precisely:

    blk.0.attn_k.bias, blk.0.attn_q.bias, blk.0.attn_qkv.bias, blk.0.attn_v.bias,
    blk.0.attn_output.scale, blk.0.attn_output.input_scale

So the mutant is two steps from loadable: add those six names, drop whatever is extra, and the gate can be run against it. That is
where this pass stops -- the mutant is not yet a mutation, and the gate's sensitivity remains unproven until it loads.

## The all-attention mutant loads past every name check and then aborts in a Goose buffer module

The remaining names were settled the same way -- width 96 for the fused `attn_qkv`, `attn_qkv.bias` of 96, `attn_output.scale` and
`.input_scale` of 1, `attn_q_norm`/`attn_k_norm` of 16 -- and three names that are not used on a non-recurrent layer were dropped
(`attn_k/q/v.bias` and `attn_gate*`). The tensor count now matches at 55.

The model then passes all name and shape checks and aborts later, in a Goose module rather than in the loader:

    ggml_abort <- fg162_fail <- fg162_gg_function_1

so `fg162` (the buffer module) rejects something about this all-attention model during setup. That is a different and more interesting
result than an unloadable file: it is the first time the synthetic model has exercised a Goose validation that the Apex model does not.

It is **not** a sensitivity proof yet, because the gate cannot run against a server that never starts. But it is a concrete lead: the
all-attention configuration reaches a Goose check that the hybrid configuration does not, and that check is worth reading on its own
merits.

## Where this leaves the item

- Fixture (hybrid): loads, serves, and the gate asserts five properties -- `checks=5 failures=0`.
- Mutant (all-attention): tensor set accepted (55/55), then aborted in `fg162`.
- Gate sensitivity: still unproven, now with a named reason and a named module.

## The mutant is rejected by the native line too -- so it is a bad model, not a divergence

The obvious check before blaming `fg162`: does flint load the same file? It does not.

    native: GGML_ASSERT(buffer) failed at ggml/src/ggml-backend.cpp:345
    ours:   ggml_abort <- fg162_fail <- fg162_gg_function_1

Both lines refuse the model, at different assertion sites with different messages. So this is not a Goose divergence -- my synthetic
all-attention model is malformed in a way that the buffer allocation rejects: the tensor set passes name and shape checks, but the
allocation derived from it is not valid.

That is worth stating plainly because the previous note called the abort "the first time the synthetic model exercises a Goose check the
Apex model does not". It does exercise a Goose check, but the same file also fails natively, so nothing about the port is implicated.

It also adds a small parity observation consistent with P4-03: the same invalid input is rejected by both lines, with different text
(native names the failing assert and file position, ours names a module and a code).

## Consequence for the sensitivity proof

The gate still has no proven mutation. A valid mutant needs a *correctly formed* all-attention model, and the generator is not there
yet: the tensor list and shapes now pass, but the resulting model does not allocate. The next step is to find why the allocation fails
-- likely a size or per-layer consistency rule -- rather than to look for a port defect, since both lines agree on the rejection.

## Second mutant (both layers recurrent): same native rejection

The all-attention mutant was rejected by both lines, so a second one was built the other way: both layers recurrent (`recurrent_layers =
[true, true]`), layer 1 given the `ssm_*` set copied from layer 0, `attn_qkv` at width 64, and the six tensors a recurrent layer does not
use dropped. Names and shapes now match the loader's expectations (73 tensors), and the result is the same native abort:

    GGML_ASSERT(buffer) failed at ggml/src/ggml-backend.cpp:345

So both synthetic mutants fail allocation on the native line, in the same place, which means the deficiency is in my generator rather than
in the port. The hybrid model -- built the same way but from a name list the loader itself produced on its first successful run -- loads
and serves.

## What that leaves

The gate's sensitivity remains unproven, and the reason is now precise: I cannot yet produce a second *valid* model to run it against.
Both attempts die at `ggml_backend_buffer_get_usage(NULL)`, which means some tensor ends up without a buffer; the likely culprits are the
`.scale`/`.input_scale` placeholders, which are plain F32 of shape 1 in our generator and may not satisfy the loader's expectations once
the layer mix changes.

That is a generator problem with a clear next step -- compare the tensor metadata of the working hybrid model against the failing mutant,
property by property -- rather than a port question.

## The generator cannot yet produce a second valid model, and that is the item's boundary

After the last name fixes the two-recurrent mutant matches the loader's expectations exactly -- 75 created, 75 present, no duplicates in
either file -- and it still aborts at `GGML_ASSERT(buffer)` in the native line. The working hybrid has 65 tensors and the same structure,
so the difference is not name count, not duplicates, and not the `.scale` placeholders by themselves.

At this point the honest statement is that my generator produces exactly one valid model: the one whose name list the loader itself
printed on its first successful run. Every deviation from that list -- all-attention, both-recurrent -- ends in the same native
allocation assert, in both lines.

## What P6-06 has and what it lacks

Has: the recurrent/hybrid builders are enumerated, and the tiny hybrid fixture exists with a gate that asserts five graph properties
(`checks=5 failures=0`), including per-layer attribution of `cache_r_l*` and `conv_output_silu`.

Lacks: a proven mutation for that gate, and fixtures for the other kept architectures (RWKV, Mamba-style) -- which would need the same
kind of generator work, since no small models exist in the tree.

Closing the item requires either a scope decision (is "the kept set" just the architecture the accepted profile uses?) or a generator
that can emit a second valid model. Both are recorded; neither is assumed.
