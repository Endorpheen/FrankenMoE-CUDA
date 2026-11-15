# EXP-2026-11-15-698 — the MoE expert path wired end to end

Base: `23f3e7fd`. No model run, no GPU.

## 1. Result

```
FG_MOE_BLOCK n_embd=32 n_ff=16 n_expert=8 n_used=2 ids_match=1 out_n=32
             nonzero=1 wsum=2.500000 router=PASS
```

The Goose router now runs inside the composed unit: `goose-moe-router.goose` is in the
build set, so the 123-module unit exports `moe_route` and the gate links against it.

Scale is reduced (32 embedding width, 16 FF width, 8 experts, 2 active) so the tensors
stay small. The wiring is what is being checked, not the numbers.

## 2. What this proves and what it does not

**Proves:** the selection produced by the Goose router indexes the same experts ggml's own
`softmax` + `argsort_top_k` select (`ids_match=1`, compared id by id); the expert matrices
are applied in the right roles (`mul_mat_id` with `silu` on the gate half and a plain
multiply for up); the per-expert outputs are combined with the router weights
(`wsum=2.500000`, the scale); and the result has the embedding width (`out_n=32`), not
zero (`nonzero=1`).

**Does not prove:** that the quantised expert kernels give the same values as llama.cpp's.
The expert arithmetic is substrate and already migrated in
`goose/cpu/goose-quantized-moe/`; this gate uses plain F32 tensors to keep the wiring
question separate from the quantisation question.

## 3. The mistake this gate caught in itself

The first run reported `out_n=2` instead of 32. `ggml_sum_rows` sums over **dim 0**, and
the expert axis is **dim 1** - so summing directly gave `[1, n_used, 1]`, a two-element
tensor, rather than a sum over the selected experts. The fix is transpose, sum, reshape.

This one is worth noting because it would have passed a weaker check: the values were
non-zero and the weights had been applied; only the output **width** exposed that the sum
was over the wrong axis.

## 4. Cost

One arch-unit build (123 modules, 1079 symbols) because the router module did not exist in
any object. That is the justified case - a new module cannot be caught by the local gate.
The gate itself compiles in seconds.
