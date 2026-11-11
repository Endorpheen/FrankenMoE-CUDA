# CUDA_MMQ: state and remaining MMA work

## Done and executing

| part | state |
|---|---|
| Q8_1 quantizer (EXP180, direct + scatter) | in the backend, opt-out, `paths=0x3`, executes on the accepted profile |
| dispatch policy `ggml_cuda_should_use_mmq` (EXP203) | Goose, native kept as `*_native`, `calls=8550` on the accepted profile |
| both, with the rest of the integration | rebuild from scratch with `build_integrated.sh`; completion stays `e68df05b` |

## Remaining: the MMA/TensorCore matmul body

Ready-made material already exists and does not need inventing:

| piece | experiment | evidence |
|---|---|---|
| `vec_dot_q4_K_q8_1_impl_mmq` (the 25-line inner product, 30.3% of kernel time) | EXP235 | 30000 checks / 0 failures, mutations caught, `dp4a` ported as real arithmetic with correct sign extension |
| typed input address for the Q8_1 quantizer | EXP208 | parser fix verified, 1 remaining `_Alignof` guard at the first foreign call |
| `ne2` pinning (400 -> 164 SASS instructions) | EXP222 | every Apex MMQ call has `ne2 == 1`, so the fast path alone is valid |

The integration shape is the same as EXP186's: emit the device code with the pinned compiler, place it inside the
MMQ translation unit so `nvcc` inlines it (the EXP235 A/B file already proves that boundary is a plain inlined
call, not a launch), and keep the native body behind `*_native` for rollback.

Constraints recorded from those experiments: the device emission must be compiled inside the same TU
(`__device__ __forceinline__`), the `gg_` helper names need this module's prefix, and `half2 -> float2` conversions
plus the tile loop stay native.

## Order

1. EXP235 `vec_dot_q4_K_q8_1_impl_mmq` into `vecdotq.cuh`, with the harness comparing against the native body at
   kernel level before touching the profile.
2. EXP222 `ne2` fast path for the Q8_1 quantizer shapes used here.
3. Re-run the accepted profile and require `e68df05b` with the MMQ witness non-zero.

## Step 1 done: vec_dot Q4_K is Goose, and the profile does not exercise it

The EXP235 device body is now inlined inside `vecdotq.cuh` (its own `fg235v_` namespace, primitives supplied
locally) and `vec_dot_q4_K_q8_1_impl_mmq` calls it. The backend builds and the accepted profile completes with
`e68df05b`, equal probabilities and a successful restore.

Execution check, and this is the honest part: scaling the inner product's return by 0.5 inside the Goose body did
**not** change the completion. So on this profile the MMQ matmul body is never reached - consistent with `--cpu-moe`
sending the expert matmuls to the CPU and the dense matmuls going to cuBLAS. The quantizer and the dispatch decision
do run (paths=0x3, calls=8550); the MMA arithmetic is integrated but dormant for this workload.

Consequence: CUDA_MMQ cannot be closed as "the MMA path executes on the profile", because it does not. What is true
and verifiable: the quantizer and dispatch are Goose and executing, the inner product is Goose with its 30000-check
gate, and the whole thing rebuilds from scratch without changing the completion.
