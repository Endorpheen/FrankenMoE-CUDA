# CPU math closure: Apex execution scope

The agreed Apex plan is explicit: Apex-35B-V1.0-Q4_K_M with its fixed profile.
Other models, formats and unused llama.cpp variants are out of scope for this
plan. This note applies that scope to the CPU math requirements.

## Measured path

`docs/goose-migration/apex-cpu-math-inventory.md` recorded first-call detours on
the real CPU-profile server:

| kernel | called on Apex |
|---|---|
| `ggml_vec_dot_q4_K_q8_K` | yes, n = 2048 |
| `ggml_vec_dot_q6_K_q8_K` | yes, n = 2048 |
| `q8_0`, `q2_K`, `q3_K`, `q5_K`, `f32`, `f16`, `bf16` | never |

The model's 753 tensors are 379 Q4_K, 310 F32 (norms/biases) and 64 Q6_K; the 64
Q6_K tensors are one kernel, not 64 kernels. APEX-FINAL records both dots as
bit-exact: Q4_K 160/160 and Q6_K 240/240, with 7/7 and 6/6 mutations, the real
CPU run identical to native (`d13cf4cf`), and a defect test proving the result
reaches the model.

## Closed on the Apex scope

| requirement | Apex closure |
|---|---|
| P1-06 every active common CPU dot/type-trait family | Q4_K and Q6_K are the only called families and are accepted; other K/IQ/TQ/FP4/legacy families are C2/P7-04 |
| P2-02 copy/conversion/set | not on the Apex CPU MoE path; EXP143 subset preserved, full contracts C2 |
| P2-03 scalar/vector/conversions | same; the two dot kernels use their own quantized path and are accepted |
| P2-04 dense matmul/out-product | not called on the Apex CPU path; C2 |
| P2-05 reductions/scans/sort | not called on the Apex CPU path; C2 |
| P2-06 normalization/gradients | F32 norms are not the quantized expert dot; C2 |
| P2-07 softmax/masks/RoPE | attention is GPU-side; C2 |
| P2-08 gather/scatter/transforms | not called on the Apex CPU MoE path; C2 |
| P2-10 recurrent/SSM/gated | Apex recurrence is GPU-side; CPU variants C2 |
| P2-13 quantization/serialization/type traits | Q4_K/Q6_K load/type traits are in use and covered; quantization encoders for other types are P7-04/C2 |

The EXP143 subset evidence is preserved in the ledger and is not relabelled as
universal CPU-backend coverage.

## What is not claimed

- No claim that the general CPU backend is fully Goose. The other operator
  families and quantized types are compatibility work for C2/P10-10.
- No claim that CPU-only inference across model families is tested; that is
  P2-17, outside the Apex execution scope.
- P4-03 and P6-02 remain blocked by the base-library reproducibility/scheduler
  issue and are not affected by this scope.

## Verdict

The CPU-math requirements close on the Apex execution scope: the two kernels the
product calls are Goose, bit-exact and live; everything else is explicitly
deferred. This is a scope decision, not a claim that the universal CPU backend
is finished.
