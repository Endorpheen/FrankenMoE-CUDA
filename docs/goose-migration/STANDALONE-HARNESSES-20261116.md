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
