# EXP-2026-11-15-706 — prefill and decode agree to f32 rounding, on one shared chain

Base: `d1b608d1`. No model run, no GPU.

## 1. Result

```
FG_PREFILL_DECODE tokens=4 bit_diff=27 max_abs=4.77e-07 rel=1.14e-07
                  wrong_pos_max=3.32657275 sensitive=1 PASS
```

## 2. What went wrong last time, and what the number means

The previous attempt reported 27 differing elements out of 32 and I read that as a wiring
failure. It was not. The same 27 elements differ here, and the largest difference is
4.8e-07 at a relative magnitude of 1.1e-07: that is f32 rounding between two flash-attention
tilings, not a wrong graph.

Three things made the difference visible as rounding rather than suspicious:

1. **One chain, not two.** The k/v tensor is built once; both arms call the same `chain()`
   and differ in two numbers - `n_q` and the mask width. The earlier version had a second
   hand-written chain, so 27 or 32 differing elements said nothing about prefill.
2. **The bound is stated before the run.** Flash attention over 4 queries and over 1 query
   select different internal tilings and accumulate the products in a different order. That
   is a real difference, so the check bounds it (rel < 1e-5) instead of demanding
   bit-equality that cannot exist.
3. **A sensitivity control.** The same decode is run with the WRONG rope position. It
   differs from the prefill by 3.33 absolute - seven orders of magnitude more than the
   rounding bound. So the comparison can see the thing it is supposed to see; a check that
   passed both the right and the wrong position would prove nothing.

`sensitive=1` is the load-bearing part. `rel=1.1e-07` alone would also be produced by a
comparison that ignores its inputs.

## 3. The contract this confirms

Recorded in `docs/goose-migration/PREFILL-DECODE.md` and now exercised end to end:

| step | requirement | why |
|---|---|---|
| rope | tokens on dim 2 | `a->ne[2] == pos->ne[0]` (ggml.c:4227) |
| permute | `(0,2,1,3)` between rope and attention | llama-graph.cpp:2558-2560 |
| flash attn | tokens on dim 1 | `can_mul_mat(k, q)` (ggml.c:5456) |
| mask | F16, contiguous, 4-D, `[n_kv, n_q, 1, 1]` | ggml.c:5450, llama-graph.cpp:41 |
| projection | output is `n_embd` wide | `wo` maps HD back to K |

The mask for the decode arm is literally the last row of the prefill's causal mask, so the
two arms see identical key/value and differ only in how many queries are asked at once.

## 4. What this does not prove

That a full decode loop produces the reference token sequence. It proves that processing
tokens in a batch and processing them one at a time give the same answer to f32 precision -
which is the property a decode loop depends on.

## 5. Cost

Gate compiles only. No unit rebuild.
