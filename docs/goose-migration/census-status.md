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
| `concat_non_cont` | EXP175 | +0.26% / +0.76% (median of 3, isolated) |
| `k_get_rows_float_vec` | EXP176 | byte-exact |
| cpy / dup / cont | EXP177 | byte-exact |
| `quantize_q8_1` | EXP178 | witness in Apex log |
| MoE weighted reduction | EXP179 | byte-exact |
| `mm_ids_helper` | EXP181 | byte-exact |
| FWHT (N=64..512) | EXP183 | −1.15% / −0.99%, inside threshold |
| `mul_mat_vec_f` | EXP184 | +0.40% / +1.69% |
| `mul_mat_vec_q` Q4_K | EXP185 | −0.07% / −1.70% (median of 7), byte-exact |
| Q4_K dequantization (F32) | EXP182 | byte-exact, **off the Apex path** |
| Q4_0 dequantization (F16) | EXP188 | byte-exact, **off the Apex path** |

### Complete witness inventory of the active runtime (EXP196)

Re-taken from a single 32K/128 pass on `runtime/exp196/run` instead of being carried forward.
Forty witness lines appear together in one model log, covering every stage in the chain.
The earlier "fourteen modules" figure was stale.

    FG143_CONTROL plans=5346 task_queries=0 dispatches=170378 jobs=5346 worker_jobs=42706
    FG143_CPU op=40 calls=138 values=67377152
    FG143_EXECUTOR worker_graphs=42706 worker_steps=170410 worker_fusions=0
    FG147_BACKEND mode=0 calls=1
    FG148_QUANT kind=9 calls=456800 values=172640
    FG149_POOL mode=0 calls=2
    FG150_EXPERT scratch_calls=127680 enabled=1
    FG150_REGISTRY mode=0 calls=1
    FG155_GEOMETRY goose_modes=0x5fb4d7
    FG156_CONSTRUCTORS goose_modes=0x53ff
    FG157_CONTEXT_CORE goose_modes=0x570333d,live_context_heaps=0
    FG158_GRAPH_BUILDERS goose_modes=0x83f3e5f1b
    FG159_GRAPH_LIFECYCLE goose_modes=0x50dd225,live_graph_heaps=0
    FG160_SHARED caller=core modes=0x100000011410
    FG161_ALLOCATOR modes=0xb18070000c,live_allocator_heaps=0
    FG162_BUFFERS modes=0x100180dfbef,live_buffer_heaps=0
    FG163_STREAM modes=0xf3ffc4fa233e,live_staging_heaps=0
    FG164_SCHEDULER modes=0x1400079fc003bf,live_scheduler_heaps=0
    FG165_REGISTRY modes=0x110040000564,live_registry_heaps=0,retained_plugin_handles=0
    FG166_CUDA_ACTIVATIONS host=0x5012000400,cuda_status=0,m0=0x2000400,m1=0x0,m2=0x2000480,m3=0x0,m4=0x0,m5=0x0,m6=0x0,m7=0x1,m8=0x0,m9=0x0
    FG167_CUDA_BROADCAST host=0x4a,cuda_status=0,grid=0xa,unravel=0x0,repeat_back=0x0,types=0x1
    FG168_CUDA_NORM host=0x18,cuda_status=0,modes=0x18,vectors=0x2,fusions=0x5
    FG169T_CUDA_TOPK host=0x3,cuda_status=0,experts=0x100,flags=0x9
    FG170S_CUDA_SSM host=0x2,cuda_status=0,modes=0x7,sizes=0x2
    FG171R_CUDA_ROPE host=0x1,cuda_status=0,modes=0x8,variants=0x50
    FG174M_CUDA_GDN host=0x8,cuda_status=0,paths=0x4,variants=0x1
    FG174S_CUDA_GDN host=0x2,cuda_status=0,modes=0x1,sizes=0x8
    FG175C_CUDA_CONCAT host=0x100,cuda_status=0,paths=0x11,guards=0
    FG175FA_CUDA host=0xf,cuda_status=0,vector=0x7,features=0x3,prefill=0x1dc
    FG176G_CUDA_GET_ROWS host=0x3,cuda_status=0,paths=0x3,guards=0
    FG176R_CUDA_SET_ROWS host=0x8,cuda_status=0,paths=0x8,invalid_index_threads=0
    FG177C_CUDA_CPY host=0x4,cuda_status=0,paths=0x1,guards=0
    FG178Q_CUDA_Q8_1 host=0x1,cuda_status=0,paths=0x1,guards=0
    FG179W_CUDA_MOE_WEIGHTED host=0x1,cuda_status=0,paths=0x1,guards=0
    FG181M_CUDA_MM_IDS host=0x1,cuda_status=0,paths=0x1,guards=0
    FG183F_CUDA_FWHT host=0x1,cuda_status=0,paths=0x1,guards=0
    FG184M_CUDA_MMVF host=0x21,cuda_status=0,paths=0x1,guards=0
    FG185Q_CUDA_MMVQ_Q4K host=0x21,cuda_status=0,paths=0x1,guards=0
    FG195V_CUDA_VMM_POOL alloc=9526 free=9526 grow=5 native=0
    FG196P_CUDA_PINNED host_plan=3 host_fail=0 fallback=0 reg_plan=120 reg_fail=0 unreg=240 native=0

Every tag above is a counter incremented inside the Goose body, not a build-time marker, so
their presence together is the evidence that the whole chain executes in one process rather
than that each stage buildt in isolation.



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


## The q4_0 row-dequantization seam: witness was broken, the verdict was not

The EXP084 profile records the *row* kernel, not the NC template:

    void dequantize_block_q4_0<__half>(const void *, T1 *, int)   count=20   summed_ns=1915413

That is the three-argument form launched by `dequantize_row_q4_0_cuda`, the selector EXP188
ported, so the earlier note that "the NC template rather than the row wrapper" is reached
does not match this profile.

The first re-test showed nothing, but the absence was an artefact of the witness itself:
EXP188 reported from `std::atexit`, and a handler registered inside a shared library does not
run once that library is unloaded. EXP195 hit the identical defect and fixed it with
`__attribute__((destructor))`. EXP188 was rebuilt that way.

With a live witness the picture is unambiguous:

    FG188D_CUDA_DEQUANT_Q4_0 host=0x0,cuda_status=4,paths=0x0,guards=0

`host=0x0` is the counter incremented at entry to the Goose row function, so the selector was
never called; `cuda_status=4` is `cudaErrorInvalidValue` from reading device symbols that were
never written, which follows. The substitution itself is correct, checked by symbol table:
`T ggml_get_to_fp16_cuda(ggml_type)` is the Goose definition and the native body sits at
`t fg188d_native_ggml_get_to_fp16_cuda`.

So EXP188's verdict stands, now on positive evidence rather than a missing log line, and its
witness bug is fixed.

The 20 prefill instances in the EXP084 profile are accounted for by the profile's age, and
this is a consequence of the live witness rather than a guess. `dequantize_block_q4_0<__half>`
has exactly one launcher in the tree, `dequantize_row_q4_0_cuda`, which is the target returned
by `ggml_get_to_fp16_cuda(Q4_0)`. In the current build that selector's entry counter is zero,
so the kernel cannot have been launched by this chain; the profile predates EXP173, which
replaced the F16 attention path with the Goose vector kernel that dequantizes the V cache
inline as `dequantize_V_q4_0`. Re-taking the census on the current runtime is what would put a
number on that, and the earlier `nsys` attempt did not complete, as recorded below.

## Transfer/residency: no executing decision on this profile

P3-17 also asks for transfer and residency policy. Probed rather than assumed, on a full
32K/128 Apex pass:

    cudaMemcpyAsync_h2d=60077 cudaMemcpyAsync_d2h=10962
    cudaMemcpyAsync_d2d=0 cudaMemcpyPeerAsync=0 cudaMemcpy2DAsync=0

Every transfer is a plain host/device `cudaMemcpyAsync` on `cudaStreamPerThread` followed by
`cudaStreamSynchronize`, issued by `ggml_backend_cuda_buffer_set_tensor` / `get_tensor`. Those
two bodies are a device select, a memcpy with a fixed direction and stream, and a synchronize:
there is no device choice, no peer decision and no residency decision in them.

The one real decision in the transfer path, `ggml_backend_cuda_buffer_cpy_tensor` (same
physical device versus peer copy versus refusing under `GGML_CUDA_NO_PEER_COPY`), never runs:
a D2D copy count of zero and a peer count of zero say so. The 2D variants are likewise
unexercised.

So there is nothing to transfer here without moving zero computation into the hot path ~71000
times per pass. The measured statement is that the policy is not reached, not that it was
ported.

## Compute Sanitizer memcheck across the chain (P3-19, partial)

Every stage that ships a GPU fixture was run under `compute-sanitizer --tool memcheck
--error-exitcode 9`, each against its own runtime directory:

| stage | fixture checks | memcheck |
|---|---|---|
| EXP175 concat | 32 / 0 | 0 errors |
| EXP176 get_rows | 27 / 0 | 0 errors |
| EXP177 cpy | 18 / 0 | 0 errors |
| EXP178 q8_1 quantize | 10 / 0 | 0 errors |
| EXP179 MoE weighted | 12 / 0 | 0 errors |
| EXP181 mm_ids | 20 / 0 | 0 errors |
| EXP182 q4_K dequant | 12 / 0 | 0 errors |
| EXP183 FWHT | 15 / 0 | 0 errors |
| EXP184 mul_mat_vec_f | 10 / 0 | 0 errors |
| EXP185 mul_mat_vec_q Q4_K | 8 / 0 | 0 errors |
| EXP188 q4_0 dequant | 15 / 0 | 0 errors |
| EXP180 MMQ Q8_1 (opt-in, not in chain) | 8 / 0 | 0 errors |
| EXP186 mul_mat_vec_q Q6_K (opt-in, not in chain) | — | **10 errors** |

Everything in the active chain is clean.

The other three tools were run over the same fixtures:

    memcheck    13 stages, 0 errors each
    initcheck   14 stages, 0 errors each
    synccheck   14 stages, 0 errors each
    racecheck   9 stages, "RACECHECK SUMMARY: 0 hazards displayed (0 errors, 0 warnings)"

racecheck only prints a summary when it has something to say, so its absence was checked
against the log rather than read as a skip: the `COMPUTE-SANITIZER` banner and the fixture's
own `checks=N failures=0` line are both present in each run. Note on the last row: the out-of-bounds reads are
reported at `mul_mat_vec_q<(ggml_type)14, (int)1, (bool)0, (bool)1, (bool)0>` — the native
template the fixture runs as its reference arm, not a Goose kernel — so this is not evidence
against the Goose body. EXP186 is not in the active link. The fixture's native arm reading
past its buffer at block 4 was not investigated further because it belongs to an experiment
that is not promoted; it is recorded so the finding is not lost.

The fixtures need `LD_LIBRARY_PATH` pointing at the stage's runtime directory: under the
sanitizer the `fg160_fusion_enabled` reference from `libggml-cuda.so.0` does not resolve from
the binary's rpath alone, and the run aborts before the first instrumented call.

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

## Timing figures: what they are and are not

Every paired figure in this project comes from an Apex 32K/128 run compared against the
same build with one component switched. Three measurement defects were found and corrected
while auditing those numbers, and they change the figures without changing any verdict:

1. **Two components in one comparison.** Five runners (EXP176, 177, 178, 179, 181)
   inherited a line from EXP174 that set `GGML_GOOSE_CUDA_SET_ROWS=0` in the native arm,
   so the native side had reverted SET_ROWS as well. SET_ROWS alone is worth about −0.09%
   prefill / −0.52% decode, the same order as the effects being claimed. All five were
   re-measured in isolation.
2. **Contaminated runs.** EXP175's original pair ran at prefill 2198 / decode 18.99, where
   this profile returns 26xx / 27.x under normal conditions. The arms were mutually
   consistent so the delta looked plausible, but neither absolute value belonged to the
   configuration. Re-measured at normal throughput.
3. **Run order.** Whichever arm runs first pays a warm-up of roughly 1.3 tok/s on this
   machine. EXP176's first pairs showed a 3-4% "speed-up" that was entirely this artefact;
   the sign reversed when the order reversed. All stages are now measured in both orders.

The isolated medians:

| stage | component | prefill | decode | before correction |
|---|---|---|---:|---:|---|
| EXP175 | concat | +0.26% | +0.76% | +0.57% / +0.44% (contaminated) |
| EXP176 | get_rows | 0.00% | 0.00% | not previously isolated; first pairs showed +3-4% (order artefact) |
| EXP177 | cpy | −0.38% | −0.77% | −0.95% / +1.97% (two components, wrong sign) |
| EXP178 | q8_1 quantize | +0.29% | +0.61% | −0.46% / +2.96% (five-fold inflated) |
| EXP179 | MoE weighted | +0.70% | −0.62% | −0.86% / −0.30% (wrong prefill sign) |
| EXP181 | mm_ids | −0.46% | +1.79% | +1.44% / +0.48% (wrong prefill sign) |
| EXP183 | FWHT | — | −0.14% | −1.15% / −0.99% (from two valid pairs spanning 8.5 points) |
| EXP184 | mul_mat_vec_f | +0.40% | +1.69% | unchanged |
| EXP185 | mul_mat_vec_q Q4_K | −0.07% | −1.70% | −1.28% from a single pair |

Every stage remains inside the 2% threshold, so no verdict changes. What changes is that the
numbers now measure one component, on runs at normal throughput, in both orders.

Resolution is coarser than the effects: within-arm spread is 1-2 tok/s on decode, so a
figure like +0.61% cannot be distinguished from zero. Read the table as "no regression
detected" rather than as a ranking of components.
