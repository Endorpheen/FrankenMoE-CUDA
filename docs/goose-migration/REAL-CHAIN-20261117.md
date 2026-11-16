# Real values chained through the Goose builder

`real_values` proved one real tensor through one node. Composition needs nodes in sequence carrying the
intermediate - PASS-COMPOSITION named exactly this ("nothing holds x across iterations").

```
FG_REAL_CHAIN wq[2048x4096 q4_K] -> wo[4096x256 q4_K] nodes=2 out=256
              builder_vs_ggml=0.000e+00 nonfinite=0 nonzero=256 PASS
```

Two real q4_K tensors from `blk.3` - `attn_q.weight` then `attn_output.weight` - with the first node's output
feeding the second, both nodes emitted by `gb_common`. The reference is the same two steps with raw ggml over
the same bytes: **`builder_vs_ggml = 0` exactly**, so the builder chains as faithfully as it emits a single
node.

## What this does and does not establish

**Establishes:** the Goose builder composes. Two real quantised tensors, one feeding the other, produce bits
identical to the equivalent raw-ggml chain. That was the open question about item 1 of the composition plan -
whether the loop would have a working primitive to loop over.

**Does not establish:** that the chain is a layer. The RMS norm, the QG split, rope, the attention over k/v
and the MoE block are all absent. This is a chaining test, not an attention test, and a full pass needs each
of those wired between the nodes that already pass their own gates.

## Sizing note, recorded because it decided the shapes

The q projection emits 8192 columns (`2 * HD * H`) while the o projection consumes 4096 (`HD * H`), so the
middle of the chain is 4096 wide. Taking 4096 rows of `wq` gives a chain whose shapes actually connect; taking
all 8192 would have failed the matmul, and the failure would have looked like a builder defect.

## Attempt to add rms_norm through the builder: blocked on the builder's MUL mode

The next node a layer needs is the norm: `rms_norm(x, eps)` then multiply by `attn_norm.weight`. The RMS norm
part works through the builder - `mode 4` with `GGML_OP_RMS_NORM` and the eps in the seventh argument, which
is how `apex_layer` calls it. The **scale by the norm weight** does not: the chain was built as
`rms_norm -> mul(nw) -> wq -> wo` and two attempts both failed.

| attempt | result |
|---|---|
| builder does `rms_norm` only, raw chain does `rms_norm -> mul(nw)` | `builder_vs_ggml=3.416e-01` - **my bug**: the two chains were not the same computation |
| builder does `rms_norm -> gb(11, ..., GGML_OP_MUL) -> ...` | **abort** - mode 11 with `MUL` is not supported |

So `gb_common`'s mode for an elementwise multiply is not the mode it uses for matmul with a different op word,
and nothing in the harnesses says what it is. `apex_layer` never multiplies by a norm weight at all - its
weights are synthetic, so it had no reason to.

**That is the next thing to establish, and it is a lookup rather than an experiment**: find the mode
`gb_common` uses for `GGML_OP_MUL` in the generated unit, then the norm step joins the chain like the two
matmuls did.

The harness is back at its committed state (`builder_vs_ggml=0`, PASS). Reverting rather than leaving a
half-working chain: a chain whose two arms compute different things is worse than no norm step, because it
reports a difference and the difference is mine.
