# EXP179: actual Apex CUDA MoE weighted reduction in Goose

Hypothesis: replace the CUDA MoE weighted-reduction operator with actual Goose for
the profile Apex runs, keeping every earlier module and the native implementation
behind one switch. No production or compiler-source changes. Current state:
qualified on the fixed Apex profile and preserved as a checkpoint.

## Verdict

**ACCEPTED** — byte-exact MoE weighted reduction on the Apex path; isolated pair +0.70% prefill / −0.62% decode.

This is a verdict on the transfer in the experimental runtime, which is the only thing
this work decides. Installation into the production working runtime is left to Igor and
Codex, which is why the state line below says "not promoted" for several stages: it means
"not installed", never "not accepted".

## Scope transferred

`ggml_cuda_op_moe_weighted_reduction` and `launch_moe_weighted_reduction` are
transferred whole. One GPU thread produces one output element:

```
first_row = token * n_expert_used
sum = (experts[first_row*n_embd + col] * first_scale) * weights[first_row]
for expert in 1..n_expert_used-1:
    row = first_row + expert
    sum += (experts[row*n_embd + col] * scale_for_row) * weights[row]
dst[token*n_embd + col] = sum
```

| Native | Goose |
|---|---|
| `moe_weighted_reduction_f32` body | `fg179w_reduce` in `device.goose` |
| assert set: all float, all contiguous, optional scale | `run()` in `host.goose` |
| extents `n_embd`, `n_expert_used`, `n_tokens` | `run()` |
| grid `(n_tokens, ceil(n_embd/256), 1)`, block 256 | `run()` |

Goose owns the token/column decode, the optional scale selection (the `1.0f`
literal path when `expert_scale` is absent), the row arithmetic, the multiply-add
order and the store. Extern symbols are only the raw float load and store. The
multiply order is preserved deliberately: native computes
`(experts * scale) * weights`, not `experts * (scale * weights)`.

## Defect in my own work, found and fixed

The bridge filled only the extents and flags in plan slot 1 and left the four
pointers in slot 0 unwritten, so the host rejected every call with validation
code 20. The pointers are now published to slot 0.

## The recorded deltas were measuring two components

The forward/reverse figures below (−0.855% / −0.295%) came from a runner inherited from
EXP174 that set `GGML_GOOSE_CUDA_SET_ROWS=0` in the native arm. That disabled SET_ROWS as
well as this stage's component, so the comparison covered two modules, not one. The same
inherited line is present in the runners for EXP176, 177, 178 and 181 — all five measured
MoE-adjacent stages at once instead of in isolation.

Re-measured with that line removed, so SET_ROWS stays in its default Goose state in both
arms and only this component switches:

| pair | native | goose | prefill | decode |
|---|---|---:|---:|---:|
| iso1 | 2607 / 27.09 | 2639 / 26.94 | +1.23% | −0.55% |
| iso2 | 2627 / 27.94 | 2631 / 27.75 | +0.17% | −0.69% |
| **median** | | | **+0.70%** | **−0.62%** |

The verdict does not change — both inside the 2% threshold — but the numbers do, and the
earlier pair had the wrong sign on prefill. SET_ROWS alone is worth about −0.09% prefill /
−0.52% decode (EXP174's own measurement), which is the same order as the effect being
measured, so it had to be removed rather than argued about.

## Local gate

| check | result |
|---|---|
| `gpu-fixture` Goose | 12 checks / 0 failures / 6 cases |
| `gpu-fixture` native rollback | 12 / 0 / 6 |
| Goose vs native destinations | identical, 0 differing floats |
| compute-sanitizer memcheck | `ERROR SUMMARY: 0 errors` |
| export surface vs EXP178 | exactly the two diagnostics added |

Cases cover `n_embd` 128/256/512/2048/4096, `n_expert_used` 2/4/8, one to sixteen
tokens, and both the scaled and the unscaled (`expert_scale == nullptr`) paths.

## On the independent reference

The host reference is a tolerance gate, not a byte gate, and that is deliberate:
native is compiled with the default `-fmad=true`, so its multiply-add chain can
differ from a strict host evaluation in the low bits. Measured relative error,
scaled by the operand magnitude rather than by the result, was at most 1.7e-7
across all cases.

A first attempt used a result-relative metric and reported up to 7.6e-4, which
looked like a defect. It was a metric artefact: an 8-term sum can land near zero,
where dividing by the result explodes despite a tiny absolute error. Scaling by
the operand magnitude removed it. Bit-exactness between the Goose route and the
native operator is established separately and directly: both were dumped and
compared float by float, with zero differing elements.

## Apex result

Two opposite-order pairs, only this operator toggled inside one DSO:

| Pair | Native pp/tg | Goose pp/tg | Δ prefill | Δ decode |
|---|---|---|---|---|
| forward | 2622.83 / 27.090 | 2619.78 / 27.121 | -0.116% | +0.113% |
| reverse | (same) | 2581.01 / 26.900 | -1.594% | -0.703% |
| mean | 2622.83 / 27.090 | 2600.39 / 27.010 | **-0.855%** | **-0.295%** |

All three runs return exactly the saved original C++ 128 token IDs, response text
and every returned top-5 probability (sha256 `e68df05b`). Both mean deltas are
inside the 2% tolerance, so this is a functional transfer with no confirmed
performance change.

Actual execution on the model is witnessed by
`FG179W_CUDA_MOE_WEIGHTED host=0x1, cuda_status=0, paths=0x1, guards=0`, with
`FG178Q_CUDA_Q8_1 host=0x1`, `FG177C_CUDA_CPY host=0x4` and
`FG176G_CUDA_GET_ROWS host=0x3` still active — four transferred operators running
together in one Apex pass.

## Rollback and reproduce

`GGML_GOOSE_CUDA_MOE_WEIGHTED=0` restores the private-renamed native operator. The
immutable EXP178 runtime remains the previous checkpoint. Reproduce with
`python3 emit_sources.py`, `python3 build_runtime.py`, `python3 test_local.py`,
then `FG_STAGE=exp179 FG174_MEASURE_MODE=goose` and `native-rollback`.
`FG179W_DUMP=<dir>` writes raw destinations for the float-by-float comparison.

## Remaining scope

Still native across P3: `mm_ids_helper`, the MMQ quantize layouts, the CUDA
dequantization module, SET/PAD/ROLL, the transposed CPY tile, quantized and
different-type copies, quantized GET_ROWS and GET_ROWS_BACK, and the MMQ/MUL_MAT
families themselves.
