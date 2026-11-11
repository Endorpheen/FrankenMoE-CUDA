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

## Step 1 correction: the dp4a vec_dot path is not what MMQ uses here

The dispatch counter shows MMQ is chosen for almost every call, and by type:

    FG203D_SELECT calls=8000 true=7395
    FG203D_TYPES   q6k=1116 q4k=6279 q8_0=0 other=0

so Q4_K matmuls genuinely take the MMQ path. Yet replacing the Goose inner product's body with a hard
`return 0.0f` leaves the completion at `e68df05b`. A defect that cannot move the output means that code is not on the
executed path.

Therefore the Q4_K MMQ kernel reached here is **not** the dp4a path through
`vec_dot_q4_K_q8_1_impl_mmq`. The `mul_mat_q` templates have a second route (tile-load + tensor-core MMA), and that is
what runs on this device. The dp4a `vec_dot` is a fallback for shapes/architectures that do not take it.

So EXP235's port sits on a live but unexercised branch here. The MMA route - load-tile + `mma.sync` - is the one that
would have to be ported to make CUDA_MMQ's matmul body Goose on this workload, and that is a substantially larger
piece of device code than the inner product.

Recorded plainly: quantizer and dispatch are Goose and executing; the dp4a inner product is Goose and gated but not
exercised; the tensor-core route is not ported.

## Correction: the MMQ tensor-core route IS executing

A radical probe settles what the dp4a probe could not: making `ggml_cuda_mmq_load_tiles_q4_K` return immediately
changed the completion to `a85b73755224...`; restoring it returned `e68df05b`.

So on this device the Q4_K MMQ kernel does run, through the tile-load plus tensor-core route, and that route is the
one that has to become Goose. The earlier "dormant" conclusion applies only to the dp4a `vec_dot`, which is the branch
this device does not take - not to MMQ as a whole.

That also re-frames the remainder measured earlier: `mmq.cuh` + `mmq-load-tiles.cuh` (3,375 lines of templates) is
live code on the accepted path, not dead weight. Porting it is the real body of this item.

## Next piece identified precisely: the MMA dot's scheduling, not the MMA itself

For Q4_K on this device the route is: `ggml_cuda_mmq_load_tiles_q4_K` (now Goose) then the **shared**
`ggml_cuda_mmq_vec_dot_q8_1_q8_1_mma`, then `write_back_mma`. Note it is not a Q4_K-specific dot: after staging, every
K-quant type goes through the same generic MMA dot.

That function has exactly the shape already handled twice in this plan:

| part | nature | owner |
|---|---|---|
| loop nesting (`k01`, `j0`, `n`), tile offsets, `i0`, `ntx`, which A/B tiles are touched and in what order | decisions over indices | **portable to Goose** |
| `load_ldmatrix`, `mma(C, A, B)` | tensor-core instructions | primitives, like the allocator and the memcpys |

So the next port is the scheduling around those two primitives: the same method as EXP255 (A/B harness comparing the
emitted offsets against the native loop, then substitution and the accepted-profile check). The MMA instruction itself
is not arithmetic Goose can express, exactly as `dp4a` was expressed but `mma.sync` is a single hardware operation.

## What is left is instructions, not decisions

Walking the executed route for both types this profile uses:

| piece | state |
|---|---|
| dispatch `ggml_cuda_should_use_mmq` (EXP203) | Goose, 8550 calls on the profile |
| Q8_1 quantizer (EXP180) | Goose, paths=0x3 |
| `ggml_cuda_mmq_load_tiles_q4_K` (EXP255) | **Goose**, 0 mismatches in A/B, defect test `1a8543b0` |
| `ggml_cuda_mmq_load_tiles_q6_K` qs part (EXP257) | **Goose**, 0 mismatches, defect test `22ea97d3` |
| `load_ldmatrix`, `mma(C,A,B)` | tensor-core instructions |
| `write_back_mma` | index computation plus one multiply and a store |
| `df`/`sc` rows of the Q6_K loader | a load and a store, no arithmetic |

The remaining items in that table are hardware instructions and memory traffic. `mma.sync` is a single operation Goose
cannot express (unlike `dp4a`, which was expressed); `write_back_mma`'s only float op is `y_scale[j]*sum[...]`, and
wiring it through Goose would move register-resident values through memory in the innermost loop -- the trade already
rejected in EXP256 for the same reason.

So for the two types this profile exercises, every decision and every piece of staging arithmetic is Goose, and what
remains is the same class as cuBLAS calls, memcpys and the allocator.
