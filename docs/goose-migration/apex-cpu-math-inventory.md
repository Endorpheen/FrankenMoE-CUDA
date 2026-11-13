# APEX CPU_MATH inventory for the agreed Apex path

Scope: Apex-35B-V1.0-Q4_K_M with its agreed configuration. Other models, formats and unused llama.cpp variants
are out of scope for this plan.

## Measured, not assumed

Entry detours on the real CPU-profile server process, with the witnesses printed on the first call of each
kernel (a destructor never runs under SIGINT, so an exit-time counter proves nothing):

| kernel | first-call witness |
|---|---|
| `ggml_vec_dot_q4_K_q8_K` | yes, n = 2048 |
| `ggml_vec_dot_q6_K_q8_K` | yes, n = 2048 |
| `q8_0`, `q2_K`, `q3_K`, `q5_K`, `f32`, `f16`, `bf16` | never called |

The model's tensor census (753 tensors: 379 Q4_K, 310 F32, 64 Q6_K) matches this: the 64 Q6_K tensors are one
kernel, not 64. The F32 tensors are norms and biases, not dot products.

## Final list

| kernel | state |
|---|---|
| Q4_K x Q8_K | **accepted** — gate 160/160, 7/7 mutations, real CPU run identical to native (`d13cf4cf`), execution proven by a defect test |
| Q6_K x Q8_K | **in progress** — module and gate exist, gate fails, offset term under debug |
| anything else | not required on this path; port only with proof of need |

Verification route for both: `libggml-cpu` built out-of-tree from the flint sources, relinked with the Goose
symbol, same profile run native and with the Goose library; identical content and sha required, plus a defect
test proving the kernel's output reaches the model.
