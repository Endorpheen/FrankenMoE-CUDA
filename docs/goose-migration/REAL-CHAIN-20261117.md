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


### What the lookup found, and where it stopped

`gb_common_g` is a one-line forwarder to `common_g`, so the mode dispatch lives in `common_g`. There is also
`gb_op_g`, a table mapping a **logical** op index to a numeric code:

```
logical 0 -> 2,  1 -> 29,  2 -> 30,  3 -> 48,  4 -> 46,  5 -> 74,  6 -> 100,  7 -> 15,  8 -> 40,  9 -> 42
```

Those codes are not `ggml_op` enum values in ascending order, so the table is a separate logical ordering
rather than a direct cast - which is consistent with mode 11 + `GGML_OP_MUL_MAT` working (the builder maps it)
while mode 11 + `GGML_OP_MUL` aborting (the same mode does not accept an elementwise op).

**Not resolved:** an extraction of the full logical→code map with a regex returned nothing, so which logical
index corresponds to `MUL` is still unknown. The next step is to read `common_g`'s mode dispatch directly,
or to call `gb_op_g` with candidate indices and see which one returns ggml's `MUL` code.

That is a lookup with a clear success criterion - the chain's two arms agreeing again - and it is where the
next attempt should start, not from another guess at the mode argument.


## Resolved: the builder has no elementwise multiply

Called `gb_op(i)` for every index instead of guessing the mode argument. The map is short and it is decisive:

```
ggml enum here:  ADD=2  MUL=7  RMS_NORM=25  MUL_MAT=29  CONT=35
gb_op logical:   0->2 (ADD)  1->29 (MUL_MAT)  2->30  3->48  4->46
                 5->74  6->100  7->15  8->40  9->42
                 10.. -> -1  (unsupported)
```

**`MUL` is not in the map.** The Goose builder supports ten operations and an elementwise multiply is not
one of them, so `mode 11 + GGML_OP_MUL` did not abort because of the call syntax - the op simply is not
reachable through `gb_common`. That is a fact about the builder, not about this harness.

Two consequences, and they are different in kind:

1. **For the test**: the norm scale has to be a raw `ggml_mul` between two builder nodes, or the comparison
   arms will never match. That is honest for a harness whose subject is the matmuls, but it means the norm
   weight is not exercised through Goose.
2. **For the migration**: a layer needs `rms_norm` followed by a scale by `attn_norm.weight`. `RMS_NORM (25)`
   is reachable, the scale is not. So either the pass calls `ggml_mul` directly - C++ in the middle of a
   Goose-built graph - or `gb_op` gains `MUL`, which is a change to the Goose graph-builder module.

The second is the migration-correct answer and the first is the expedient one. Per rule 17 the choice matters:
an adapter that calls raw ggml inside a Goose graph is the kind of debt the rules call for counting rather
than accumulating quietly. Recorded before anything is written.

## The exact shape of the missing MUL, with the pieces already present

`mode 11` is not generic. Line 313 of `goose/cpu/goose-graph-builders.goose`:

```goose
if mode == 11 { ensure(pred(8, a, b) != 0, 17); }
```

and `pred(8)` is `fg_can_mul_mat` (`integration/goose-standalone/graph-bindings.c:241`). So mode 11 validates
its operands as a matrix product before emitting anything; an elementwise multiply fails that predicate by
construction, not by misuse.

**So the builder has no mode for elementwise binary ops**, and adding MUL needs four things, all small and all
identifiable:

| piece | where | what |
|---|---|---|
| the op code | `op_*()` block | `fn op_mul() -> i64 { return 7; }` - ggml's `MUL` in this tree |
| the table entry | `gb_op` | `if op == 10 { return op_mul(); }` (10 is free: the table stops at 9) |
| the mode | `common()` | a mode that emits `MUL`, or mode 11 made op-aware so its predicate is chosen per op |
| the shape rule | `fg158_pred` | `case 4: ggml_can_repeat(x, y)` already exists and is exactly the broadcast check a scale needs |

The last row is the reason this is a small change rather than a new subsystem: the predicate the new mode
needs is already implemented and exposed.

**What is not done, and why**: no edit to the Goose module. Changing a hot builder whose gate is 100/100
cross-checks against native, in a session that has already found seven stale claims, needs the change to be
made with the gate updated in the same step - not appended at the end of a long run.


## CORRECTION: the builder does have an elementwise multiply. Mode 0, not mode 11.

My previous conclusion on this card - "the Goose builder has no elementwise multiply" - is **wrong**, and the
error was in which mode I tried.

`common()` dispatches on `mode`, and inside the `mode 0..9` block:

```goose
var actual = op;                    // the op comes from the ARGUMENT...
...
finish(r, actual, a, b, d);         // ...and mode 0 adds no validation of its own
```

**Mode 0 takes its op from the argument.** So `mode 0` with `GGML_OP_MUL` emits a multiply. Mode 11 was the
wrong choice because it asserts `pred(8) == fg_can_mul_mat` before emitting - an elementwise multiply can
never satisfy that, which is a property of mode 11, not of the builder.

`gb_op`'s ten-entry table is a naming aid for a host that wants an op id without hard-coding it. It is not a
capability list, and I read it as one.

## The chain now includes the real norm, and it agrees

```
FG_REAL_CHAIN wq[2048x4096 q4_K] -> wo[4096x256 q4_K] nodes=2 out=256
              builder_vs_ggml=0.000e+00 nonfinite=0 nonzero=256 PASS
```

Four nodes on the builder side - `rms_norm`, the scale by the real `blk.3.attn_norm.weight`, `wq`, `wo` - and
three on the raw side, all over real bytes, agreeing to the bit.

So the layer's first four nodes exist and are equivalent. What a layer still needs is the QG split, rope and
the attention over k/v, then the MoE block.

## QG split: attempted, crashed, reverted - and the reason is the truncation, not the split

Adding `qg_split` as the fifth node segfaulted. The likely cause is arithmetic this harness created for
itself:

`apex_layer` derives the head width from the **real** `wq` width: `8 192 / (2 * 16) = 256`, because the q
projection emits `2 * HD * H` columns. This chain truncates `wq` to 4096 rows to make the middle of the chain
connect to `wo`, so its q projection is 4096 wide and `HD` would be 128 - a **different model shape**, not a
narrower version of the same one.

So the split is being asked to divide a projection whose head structure it cannot know from the tensor alone.
That is a property of a harness that trims rows to keep the shapes tidy, not of `qg_split` - which passes its
own gate (`qg_split q_differs=0 gate_differs=0 PASS`) at the real 8192-wide projection.

**The consequence is a design decision for the composition harness, not a bug to patch**: either it carries
the real 8192-wide projection and the matching `wo` that consumes `HD * H = 4096`, or it stays a two-matmul
chain and the split belongs to the next harness. Trimming rows to connect two tensors of different widths is
what put the head structure out of reach.

Harness reverted to its committed state (`nodes=4`, `builder_vs_ggml=0`, PASS).

### Second attempt with the full projection: code=17, and the exact cause

Carrying the real 8192-wide projection got past the head-width problem and stopped one node later:

```
graph-bindings.c:279: Goose graph builder validation failed: code=17
```

`code 17` is the mode-11 assertion `ensure(pred(8, a, b) != 0, 17)`, i.e. `fg_can_mul_mat` refused. The failing
call is `gb(11, gc, wo, qv)`: `wo` is `[4096, 256]` and `qv` comes out of `qg_split` as a **3-D view**
`[HD, H, 1] = [256, 16, 1]`, not as a flat `[4096, 1]`. `can_mul_mat` compares `ne[0]` and sees 4096 against
256.

`apex_layer` does the reshape the split's callers need - it takes `qv` and continues with `k4 = call(24, ...)`
(`GGML_OP_CONT` with explicit extents). That step is what this chain is missing, and it is a missing **reshape
between two nodes**, not a wrong mode, a missing builder op, or a bad split.

So the two attempts bracket the problem:

| attempt | carried | stopped at | cause |
|---|---|---|---|
| first | 4096-wide (trimmed) | `qg_split` | head structure not derivable from a trimmed projection |
| second | 8192-wide (real) | `gb(11, wo, qv)` | `qv` is `[HD,H,1]`, needs a reshape before a 2-D matmul |

Each attempt removed one obstacle and exposed the next, and both were in the harness rather than in the
builder, the split or ggml.

Harness reverted to its committed state (`nodes=4`, `builder_vs_ggml=0`, PASS).

### Third attempt: the chain runs, the two arms disagree - and that is the harness's guessing, not a defect

With `ROWS` at the full 8192 and a `CONT` + `reshape_2d` for the split's output, the chain executes:

```
FG_REAL_CHAIN norm -> wq[2048x8192 q4_K] -> wo[4096x256 q4_K] nodes=4 out=256
              builder_vs_ggml=2.138e+00 nonfinite=0 nonzero=256 FAIL
```

No abort, finite output, and the arms differ by 2.1 on an output whose scale is order 1. That difference is
mine: the raw arm reproduces the q half with `ggml_view_3d(..., HD, 16, 1, r1->nb[1] * 2, r1->nb[2], 0)` - a
**guess** at the interleaving - while `qg_split(gc, q, HD, H, 1, 4, views)` decides the actual layout behind
two numeric mode arguments whose meaning this harness does not read.

So the three attempts produced a clean progression, and none of the failures was in the components:

| attempt | got past | failed at | whose fault |
|---|---|---|---|
| 1 | - | `qg_split` | trimmed projection hid the head structure |
| 2 | head width | `gb(11, wo, qv)` | `qv` is 3-D, no `CONT` |
| 3 | the whole chain | the comparison | the raw arm guesses the split's interleaving |

`qg_split` itself passes its own gate: `q_differs=0 gate_differs=0 q_at_zero=1 gate_at_head=1`.

**What the next attempt needs is not more code but one fact**: what `arg4 = 4` and `arg3 = 1` mean for
`qg_split`, so the raw view can reproduce the layout instead of approximating it. That is in
`goose/model/qgate-split.goose` and is a read, not an experiment.

Harness reverted to its committed state (`nodes=4`, `builder_vs_ggml=0`, PASS).


## RESOLVED: the fact was `elem` means element size, and the chain now runs the split

Read `goose/model/qgate-split.goose:39-50` instead of guessing the layout:

```goose
let head_stride = elem * head_dim * 2;
let tok_stride  = head_stride * n_head;
let seq_stride  = tok_stride * n_tokens;

out[0] = view4(c, qfull, head_dim, n_head, n_tokens, 1, head_stride, tok_stride, seq_stride, 0);
out[1] = view4(c, qfull, head_dim, n_head, n_tokens, 1, head_stride, tok_stride, seq_stride, elem * head_dim);
```

**`elem` is the element size in BYTES (4 for f32), not a mode.** That was the whole misreading. With
`head_stride = 4*HD*2 = 2048` and the gate view at offset `elem*HD = 1024`, the raw arm reproduces the split
exactly:

```
FG_REAL_CHAIN norm -> wq[2048x8192 q4_K] -> wo[4096x256 q4_K] nodes=4 out=256
              builder_vs_ggml=0.000e+00 nonfinite=0 nonzero=256 PASS
```

The 2.138e+00 of the previous attempt came from using the tensor's row stride (`nb[1]*2` = 65536) where the
split uses a per-head stride of 2048 - wrong by 32x, and it looked like a split defect.

## What the chain now contains

Six nodes over real bytes, the builder side and the raw side identical to the bit:

```
rms_norm(x) -> * blk.3.attn_norm.weight -> wq (q4_K, full 8192) -> qg_split -> CONT -> wo (q4_K)
```

That is everything an attention layer does **before** rope and the attention itself. The remaining nodes are
rope, the k/v path and the softmax-weighted values, then the MoE block.

## The method note this earned

Three failed attempts, each from a guess: the head width from a trimmed projection, the view shape without a
`CONT`, and the stride from `nb[1]` instead of from the module. The fourth worked because it stopped guessing
and read the twelve lines that decide the layout. Guessing at a stride is not an experiment - it is arithmetic
that can be checked by reading.
