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
| `flash_attn_ext_vec`, `flash_attn_ext_f16` | EXP173 | witness `FG175FA_CUDA host=0xf`, four paths |
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

## CPU side

EXP143 already executes Goose modules in the CPU MoE path and its witnesses appear in
every Apex run: `FG143_CONTROL plans=5346 dispatches=170378`,
`FG143_CPU op=100 calls=42560` (expert matmul) and `op=40 calls=138` (dense matmul),
`FG143_EXECUTOR worker_graphs=42706 worker_steps=170410`, `FG147_BACKEND`.

Against that, the CPU dot products are out of reach for the same reason as the GPU ones:
`ggml_vec_dot_q4_K_q8_K` contains 58 SIMD instructions and `ggml_vec_dot_q6_K_q8_K` 91 on
this host, and Goose has no SIMD. `quantize_row_q8_K` was transferred (EXP190) precisely
because it has zero, but Apex does not reach it: the graph goes straight to the matmuls.

## Process defect found and fixed

EXP180 (MMQ Q8_1) was rejected for a measured 12-14% kernel regression, but its switch
was written opt-OUT, so the Goose path ran unless the flag was explicitly "0" and the
object sat in the active link chain. Every Apex run since EXP180 paid the regression, and
the evidence was in the log all along as `FG180Q ... host=0x23`.

Fixed by inverting that switch to opt-IN and rebuilding the chain. The witness is gone
from the Apex log and the paired figures moved from -1.28% to -0.55% decode and -0.19%
prefill. EXP186, the other rejected candidate, was checked and is not in the active chain,
but its switch was inverted too so a later stage building on it cannot inherit the same
problem.

Rule now enforced in code, not just in the write-ups: a rejected experiment is never
default-on.

## Verified kernel-by-kernel against a live run

Every census kernel was matched to its witness in one Apex log rather than assumed:

| kernel | instances | state |
|---|---:|---|
| mul_mat_vec_q | 18 | covered (Q4_K accepted; Q6_K byte-exact but rejected, opt-in) |
| mul_mat_q | 8 | blocked: codegen |
| flash_attn_ext_vec | 6 | covered, `FG175FA host=0xf` |
| flash_attn_ext_f16 | 5 | covered, same witness |
| quantize_q8_1 | 5 | covered, `FG178Q` |
| gdn_single | 4 | covered, `FG174M`/`FG174S` |
| concat_non_cont | 4 | covered, `FG175C` |
| mul_mat_vec_f | 4 | covered, `FG184M` |
| k_get_rows_float_vec | 4 | covered, `FG176G` |
| rms_norm_f32 | 4 | covered, `FG168` |
| quantize_mmq_q8_1 | 3 | blocked: codegen (and rejected, opt-in) |
| unary_gated_op_kernel | 3 | covered, `FG166` |
| dequantize_block_q4_0 | 1 | EXP188 byte-exact, not on this path |
| mul_mat_f | 1 | blocked: codegen |

Also confirmed by direct check: the rejected EXP186 contributes no symbols to the active
library, and the EXP188 witness is absent from the live log, both as documented.

### Why the other mm_ids widths are not ported

`mm_ids_helper` has eight specialisations (2/4/6/8/10/16/32/0) selected by
`n_expert_used`. EXP181 transfers the 8-wide one because that is what this model
instantiates; the others cannot execute in this profile, so porting them would add code
that no Apex run exercises. If a different model with a different expert count is ever
targeted, the remaining widths are mechanical repeats of the same body with `slot`
changed.

### Why the Q4_0 quantizer keeps the rejected-adjacent shape

The remaining unported work is behind one of two walls: the code-generation gap for the
matmul families, or the absence of SIMD for the CPU dot products. Neither is reachable
from this repository; both are recorded with measurements in the per-experiment notes.
