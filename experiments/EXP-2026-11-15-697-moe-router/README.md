# EXP-2026-11-15-697 — the MoE router decision, in Goose

Base: `9460f9a1`. No model run, no GPU.

## 1. Result

```
trial 0: ids_differ=0  weights_mismatch=0  sum_x1000=2500.0
trial 1: ids_differ=0  weights_mismatch=0  sum_x1000=2500.000000660648
dominant_first=173
FG_MOE_ROUTER PASS   n_expert=256  n_used=8
```

`goose/cpu/goose-moe-router.goose` decides the selection half of `build_moe_ffn`
(`llama-graph.cpp:1941-2100`): softmax over 256 experts, top-k of 8 with the lower id
winning a tie, gather by id, optional normalise, then the scale.

## 2. Why the reference is a different algorithm

`reference_top_k` in the harness repeatedly scans the full probability vector for the
largest value not yet taken. The module's `moe_top_k` does the same thing through a
linear-pass selection. If both agreed because they shared code, the check would prove
nothing - so the harness re-derives the answer from the probabilities instead of calling
back into the module.

Two input shapes are used on purpose:

- **trial 0**, smooth logits `((i*37 % 101) - 50)/8`: many distinct probabilities.
- **trial 1**, all zeros with `l[173] = 24.0`: one dominant expert.

The second exists because the first one alone does not catch a wrong expectation on
normalisation. That is precisely what happened: see below.

## 3. A wrong expectation, not a wrong module

The first run reported `weights_mismatch=8` on trial 0 and `0` on trial 1. The module
was right and the harness was wrong: after normalisation the weight is

```
probs[id] / sum(selected probs) * scale
```

not `probs[id] * scale`. In the dominant trial the selected sum is almost 1, so the
incorrect formula passes there and fails only on flat logits.

This is worth recording because it is the opposite failure mode from EXP-688b: there the
gate passed while the layer was wrong; here a correct module failed a wrong check. Both
come from the same place - an expectation that was not derived carefully enough - and the
cure is the same, which is to state what the expected value is and why, before running.

## 4. What the harness had to work around

Three language-level points, none about the model:

1. A dash is not allowed in an import path, so `goose-moe-router.goose` enters a program
   only through composition; the harness is concatenated after the module.
2. Arrays must be fully formed at declaration: `var a: f32[N];` followed by per-index
   assignment is rejected as possibly unassigned. `default<f32[N]>()` works.
3. `print` does not format. It emits its argument, so labelled metrics are printed as
   separate lines rather than a formatted row.

## 5. On accumulate width

`moe_softmax` and `moe_weights` accumulate in f64; ggml's softmax accumulates in f32. The
weights therefore agree to f32 rounding, not bit for bit - and the ids agree exactly,
since they compare values already rounded into the same f32 probabilities. That is stated
before any comparison against ggml, so the next step does not chase a difference that is
supposed to exist.

## 6. The shared-expert branch and the combine

`moe_shared_gate`, `moe_ffn_par`, `moe_shared_apply` and `moe_combine` carry the rest of
`qwen35moe.cpp:530-548`. They are one-liners each, and they are exactly the decisions
that are easy to get subtly wrong while every node stays correct:

- the sigmoid belongs on the **shared** gate, not on the router;
- the silu belongs on the **gate** half of the PAR pair, not on the up half;
- the two branches are **added**, not averaged and not replaced.

The check is property-based rather than a second copy of the same formula, because
recomputing sigmoid the same way would only prove the code agrees with itself:

```
range_bad=0            sigmoid strictly inside (0,1)
mono_bad=0             sigmoid monotonic in its input
up_doubles=0           doubling `up` doubles the result exactly
gate_not_linear=63     doubling `gate` does NOT double the result (silu is not linear)
add_bad=0              combine is a plain add
comm_bad=0             combine is commutative
avg_bad=63             combine is not an average
failures=0   FG_MOE_FFN PASS
```

`gate_not_linear=63` is the check that would fail if silu had been applied to the wrong
half: with silu on `up`, doubling `gate` would double the result and the count would be 0.

### Three expectations wrong again, module right again

The first run of this harness reported FAIL three times, and each time the harness was
wrong:

1. `silu(2x) != 2*silu(x)` for **all** 64 elements - but silu is locally linear for
   large positive x, so three elements coincide.
2. `combine != average` for **all** 64 - but a sum equals its average wherever the
   branches are zero, so one element coincides.
3. the counter named `gate_not_linear` was incrementing on the **equal** case, so the
   verdict read the property backwards.

That is the third card in a row where the model was right and the check was wrong
(EXP-688b was the reverse). The pattern is consistent enough to state as a rule: when a
check fails, first ask whether the expected value was derived correctly, and write down
why that value is expected before running.

## 7. What is still missing

The expert matmuls (`mul_mat_id` over the selected ids, weighted sum), the shared expert
branch with its own sigmoid gate, and the 42-layer stack. The arithmetic of the experts
is already migrated in `goose/cpu/goose-quantized-moe/`.
