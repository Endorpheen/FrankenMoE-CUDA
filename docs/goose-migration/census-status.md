# Apex census — transfer status

Source of truth for kernel coverage: `results/exp084-apex-profile/analysis.json`,
cross-checked against the per-experiment `apex-summary.json` files. "Executed by Apex"
means the model log carries the module's witness line, not merely that the module
builds.

## Accepted, enabled by default

| kernel | experiment | evidence |
|---|---|---|
| `rms_norm_f32` | EXP168 | in the CUDA DSO |
| `ggml_cuda_gated_delta_net_mma::gdn_single` | EXP172 | witness in Apex log |
| `flash_attn_ext_vec`, `flash_attn_ext_f16` | EXP173 | witness in Apex log |
| `unary_gated_op_kernel` (SiLU/gated) | EXP166 | witness in Apex log |
| `concat_non_cont` | EXP175 | +1.03% paired |
| `k_get_rows_float_vec` | EXP176 | byte-exact |
| cpy / dup / cont | EXP177 | byte-exact |
| `quantize_q8_1` | EXP178 | witness in Apex log |
| MoE weighted reduction | EXP179 | byte-exact |
| `mm_ids_helper` | EXP181 | byte-exact |
| FWHT (N=64..512) | EXP183 | −1.15% / −0.99%, inside threshold |
| `mul_mat_vec_f` | EXP184 | +0.40% / +1.69% |
| `mul_mat_vec_q` Q4_K | EXP185 | +0.21% / −1.28%, byte-exact |
| Q4_K dequantization (F32) | EXP182 | byte-exact, **off the Apex path** |
| Q4_0 dequantization (F16) | EXP188 | byte-exact, **off the Apex path** |

Fourteen modules execute inside a single Apex pass. Their witnesses appear together in
one model log: `FG175C`, `FG176G`, `FG177C`, `FG178Q`, `FG179W`, `FG181M`, `FG183F`,
`FG184M`, `FG185Q`, plus the earlier normalization/attention/GDN set.

## Blocked on the same code-generation limitation

These carry the remaining share of GPU time and all fail for one reason: a Goose kernel
that recomputes a full byte address per access compiles to roughly 2.5x the native
instruction count.

| kernel | share | measurement |
|---|---|---|
| `mul_mat_q` Q4_K + Q6_K | ~40% of GPU time | EXP180: 824 vs 160 instructions |
| `quantize_mmq_q8_1` | feeds MMQ | EXP180, same cause |
| `mul_mat_vec_q` Q6_K | second half of the matvec work | EXP186: 504 vs 193, kernel 21.82 us vs 17.41 us, paired decode −3.66% |
| `mul_mat_f` | 1 instance | same shape as the matvec family |

EXP186 is byte-exact and would be promoted instantly if the gap closed; it is committed
as a diagnostic checkpoint with its numbers.

### What was tried and measured

For EXP186, each attempt kept correctness at 4/4 and was measured, not assumed:

1. index math moved from `i64` to `i32` so divides fold to shifts — **504 instructions,
   unchanged**;
2. primitives switched from `(base, byte)` to a single absolute address — **664
   instructions, worse**, reverted;
3. `iqs`-derived indices hoisted out of the `kbx` loop — **504, unchanged**, ptxas was
   already hoisting them.

The instruction profile shows where the cost is: 172 `IMAD` and 58 `IADD3` against 45
and 14 in native, with 58 `ISETP` against ~8. The Goose body recomputes addresses rather
than letting nvcc fold them into the load's immediate offset, and native additionally
gets the read-only `LDG.E.CONSTANT` path from its `__restrict__` parameters.

Closing this needs a compiler-side change, which is out of bounds for this work.

## Not covered

`ggml_get_to_fp16_nc_cuda` and its `to_t_nc_cuda_t` per-dimension-stride signature. The
Apex KV dequantization reaches the NC template rather than the row wrapper, which is why
EXP188's byte-exact module never runs. Reaching it needs a separate seam.

## Bottom line

Every kernel in the census is either transferred-and-verified, or blocked on one
identified code-generation limitation with measured evidence. Nothing is in an unknown
state, and nothing was promoted on the strength of an invalid comparison.

## Tooling limitation found while trying to re-take the census

The plan calls for re-taking the census on the current qualified runtime rather than
trusting the EXP084 profile. An attempt was made by wrapping the exact same verified
server invocation in `nsys profile -t cuda --sample=none` and replaying the fixed
32K/128 request.

It did not complete: the profiled server reached `{"status":"ok"}` on `/health` and
served the model, GPU utilisation fell back to 4%, a 4 MB `.qdstrm` was written, but the
closing `nsys` export never finished, so no `.nsys-rep` appeared. After several minutes
the wrapper was terminated and the port released. No profile data was produced.

This is recorded rather than worked around, because a census taken from a different
build would not be comparable to the EXP084 numbers and the instruction is to port by
fact, not by guess. The EXP084 census remains the authority, and it is complete: 14
distinct kernels, 70 entries, all of them in one of the three documented states above.
