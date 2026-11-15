# Prefill and decode — what is established, and where I stopped

Base: `6a8bae1c`. No passing gate is claimed for this card. It records the substrate facts
that cost six iterations to establish, so the next attempt starts from them instead of
rediscovering them.

## The facts, each read from the source and confirmed by an abort

These are not opinions; every one terminated a run.

1. **The token axis is dim 2 while rope runs.** `ggml_rope_ext` asserts
   `a->ne[2] == pos->ne[0]` (ggml.c:4227). Positions are per token, so tokens must be on
   dim 2 when rope is applied.
2. **The token axis is dim 1 for flash attention.** `ggml_flash_attn_ext` first asserts
   `ggml_can_mul_mat(k, q)` (ggml.c:5456), which requires the lengths on dim 0 and the
   token counts on dim 1.
3. **llama.cpp resolves 1 and 2 with a permute between them** (llama-graph.cpp:2558-2560):
   `[HD, n_head, n_tok, B]` → rope → `permute(0,2,1,3)` → `[HD, n_tok, n_head, B]` →
   flash attention. Doing it in any other order aborts at one of the two asserts.
4. **The mask must be F16, contiguous, 4-D, shaped `[n_kv, n_tokens, 1, n_seq]`.** An F32
   mask is refused by the assert at ggml.c:5450; a 2-D mask has the wrong `ne[2]`/`ne[3]`.
   llama-graph.cpp:41 builds it exactly this way and says why in a comment.
5. **The projection output is `n_embd` wide, not the head width.** `wo` maps `HD` back to
   `K`, so a per-token row of the result is `K` floats.

## RESOLVED — EXP-2026-11-15-706

The 27-of-32 discrepancy was f32 rounding between flash-attention tilings, not a wiring
failure. Rebuilt as ONE shared chain with the arms differing only in `n_q` and mask width:

```
tokens=4 bit_diff=27 max_abs=4.77e-07 rel=1.14e-07 wrong_pos_max=3.33 sensitive=1 PASS
```

The bound (rel < 1e-5) is stated before the run, and a sensitivity control runs the same
decode with the wrong rope position, which differs seven orders of magnitude more. That
control is what makes the comparison meaningful.

The section below is kept as the record of the failed attempt and of what it cost.

## The failure that stopped the first attempt

With 1-5 all satisfied, a 4-token prefill and a decode step over the same k/v still
disagree in 27 of 32 output elements. Both arms run, both produce finite values, the mask
reaches the kernel, and `rows_identical=0` (so the causal mask is doing something).

The remaining candidate is the accumulation path: a masked flash-attention call and an
unmasked one select different kernels (`ggml_compute_forward_flash_attn_ext_f16_one_chunk`
versus the tiled path), and they accumulate in a different order. Passing a same-width
all-zero mask to both arms - which I did - does not guarantee the same path, because the
kernel selection also depends on `GGML_PREC_*` and the tensor types, not only on mask
presence.

That hypothesis is untested. Six iterations were enough to establish the contract and not
enough to isolate this, and continuing to vary one parameter per run is not a good use of
time.

## Where the next attempt should start

Do not rebuild the two arms. Instead:

1. Run the **same** 4-token prefill twice: once with the causal mask, once with an all-zero
   mask of the same shape. If they differ, the mask is working and the comparison target is
   the masked prefill's last row - which is what a decode step with a full cache computes.
2. Run that last row through flash attention as a **1-token query over the 4 cached
   positions**, reusing the identical tensor construction rather than a second hand-written
   chain. Two hand-written chains differing anywhere will produce this exact symptom.
3. Only when those agree, compare against llama.cpp's own layer on the same weights - which
   is a differential run that does not need a model.

The mistake to avoid, stated so it is not repeated: I built the decode arm as a second,
independently written chain. A prefill/decode comparison is only meaningful if both arms
share the graph and differ in the token count, so the difference under test is the token
axis and nothing else.
