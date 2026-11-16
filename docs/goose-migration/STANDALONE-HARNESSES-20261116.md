# 23 standalone harnesses, two of them run, one failing

Found while looking for the next integration step: `integration/goose-standalone/tests/` holds 23 harnesses
and `build-standalone-runtime.sh` builds exactly two - `graph_exec` and `tokenizer_decisions`, the ones that
need no model. The other 21 were executed by nothing, so whether they passed was unknown.

## What running them showed

`tools/verify-standalone-apex.sh` now builds and runs the ones that take the Apex GGUF:

```
ok   stack_dispatch   FG_STACK_DISPATCH n=41 attn=10 recr=30 mtp=1 first_attn=3 last_attn=39 pos_bad=0
                      shifted_differs=20 bad=0 PASS
ok   prefill_decode   FG_PREFILL_DECODE tokens=4 bit_diff=27 max_abs=4.76837158e-07 rel=1.14e-07
                      wrong_pos_max=3.32657275 sensitive=1 PASS
FAIL layer_real       no PASS line
```

**`stack_dispatch` is a real result that had never been shown**: the 41-layer plan is consumed by a loop that
selects a builder per kind, and 10 attention / 30 recurrent / 1 MTP come out with `bad=0` and
`pos_bad=0` - the positions of the attention layers are right, not just their count. That is the dispatch
half of "full 41-layer graph: dispatch yes / execution no", and it passes on the real file.

**`prefill_decode` is the other**: the same chain with only `n_q` and the mask width differing between the
two arms, `rel=1.14e-07` and `sensitive=1`. Prefill and decode agree to f32 precision and the test proves it
is sensitive to the difference rather than trivially equal.

## The failure is in the test, not the runtime

```
GGML_ASSERT(ggml_can_mul_mat(a, b)) failed   at ggml_mul_mat(gc, k, q)
```

`layer_real.c` builds q/k/v with the full `n_embd` projection and then multiplies `k` by `q` expecting both
at `n_embd`. This model is **GQA**: `n_head 16`, `n_head_kv 2`, `head_dim 128`, so `n_embd_k_gqa = 256`
against `n_embd = 2048`. The assertion is correct to fire; the harness was written for a shape this model
does not have.

It is kept in the list rather than removed, so the failure stays visible. Fixing it means reshaping q/k/v per
head before the score matmul - a test fix, and it should be done with its own evidence, not silently.

## Why this is not in the build check

Each run reads a 21.7 GB file and takes tens of seconds. Rule 18 puts that in "heavyweight, needs a reason",
not in a build. The script is the reason to run it.

## Consequence

The two passing harnesses **raise the standalone's verified surface**: the 41-layer dispatch and the
prefill/decode chain are now executed on the real weights, not only on fixtures. Neither was in any report
before this, because nothing ran them.

## Full run: 19 pass, 1 informational, 1 test defect

All 21 model-dependent and model-free harnesses were built and run. The results, which nothing had produced
before:

**Fast, no model (9/9 pass)**

```
qg_split      head_dim=256 n_head=16 elements=4096 q_differs=0 gate_differs=0 PASS
gdn_ops       skip_when_equal=1 PASS
gdn_recur     tokens=2 state_elems=24576 state_differs=0 state_nonzero=10120 PASS
moe_block     n_expert=8 n_used=2 ids_match=1 router=PASS
layer_orch    differs_ab=0 differs_wrong_postnorm=32 differs_wrong_input=32 distinguishable=1 PASS
kv_cache      tokens=3 head_dim=8 n_ctx=8 bad=0 PASS
state_slot    checks=22 bad=0 PASS
state_life    tokens=6 rebuilt=6 lost=0 stray=0 PASS
state_reuse   split_at=2 differs=0 distinguishable=1 PASS
```

**Reading the GGUF (10 pass, 1 informational)**

```
apex_shape      cases=24 mismatches=0 PASS
q35_shape       checked=32 mismatch=0 absent=0 PASS
gdn_plan        n=41 attn=10 recr=30 mtp=1 bad=0 PASS
gdn_planfile    layers=41 mismatch=0 missing_both=0 mtp=1 PASS
stack_dispatch  n=41 attn=10 recr=30 mtp=1 pos_bad=0 bad=0 PASS
apex_layer      nodes=15 shape_ok=1 nonfinite=0 nondet=0 wiring_differs=0 vs_ggml=0 PASS
apex_graph      nodes=5 N=8192 first=0.134777 want=0.134777 rel=8.07e-08 PASS
gdn_layer       head_k=128 n_k=16 head_v=128 n_v=32 bad_nodes=0 PASS
gdn_layer_full  same, bad=0 PASS
apex_plan       (informational - the source contains no PASS string; it reports the plan)
```

`apex_graph` is the strongest number here: **`first=0.134777 want=0.134777 rel=8.07e-08`** - a real numeric
match on real weights through a 5-node graph. `apex_layer` adds `nondet=0` and `wiring_differs=0` over 15
nodes, and `vs_ggml=0` means it agrees with ggml's own result.

**One failure, and it is the test**

```
layer_real   FAIL   GGML_ASSERT(ggml_can_mul_mat(a, b)) at ggml_mul_mat(gc, k, q)
```

Written for MHA; Apex is GQA (`n_head 16`, `n_head_kv 2`, `head_dim 128` -> `n_embd_k_gqa 256` against
`n_embd 2048`). ggml's assertion is correct to fire. Kept in the list so it stays visible.

## What this changes

The status line said "full 41-layer graph: dispatch yes / execution no". **That is still true for the full
stack** - a whole 41-layer pass has not been run. But it was understating what does execute: a real layer's
15-node attention chain, a 5-node graph to `rel=8e-08`, the 30-layer GDN kernels, the MoE block, the KV and
state machinery, end to end on the 21.7 GB file. All of it was built, all of it passes, and none of it was
being run by anything.

The gap is no longer "nothing executes" but "the layers have not been composed into one pass".
