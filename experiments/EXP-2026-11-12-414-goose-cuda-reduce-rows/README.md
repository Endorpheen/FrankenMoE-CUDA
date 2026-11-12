# EXP414: CUDA row reductions in Goose -- closes P3-02

P3-02 names four source areas: `common.cuh` (device math and warp/block
helpers), `convert.cu` (conversions), `reduce_rows.cuh` (row reductions) and
`mma.cuh` (tensor-core helpers). This experiment closes the reduction area.
The other three are mapped to their existing evidence in the criterion table
below.

## What was ported

Device (`source/device.goose`):

| native (`reduce_rows.cuh`) | Goose |
|---|---|
| eight per-thread accumulators, `temp[8]` / `sum_temp[8]` | eight scalar accumulators `a0..a7` |
| load guard `i < ncols`, otherwise zero | per-load branch |
| accumulator merge in index order | `s = ((a0+a1)+a2)+...` |
| `warp_reduce_sum`: `__shfl_xor_sync` offsets 16,8,4,2,1 | `warp_sum()` with five `shfl_xor_f32` calls in the same order |
| cross-warp staging in `shared_vals[32]` | typed stores/loads through a shared pointer |
| `__syncthreads` | `sync` primitive |
| `norm ? sum/ncols : sum` | explicit `norm` branch |
| strided row decode `row%ne1`, `(row/ne1)%ne2`, `row/(ne1*ne2)` | same expressions on non-negative operands |

Host (`source/host.goose`): the shared occupancy heuristic of `sumrows.cu` and
`mean.cu` -- 512 threads while `nrows/nsm < 2`, otherwise 32 threads for
`ncols < 1024` and 128 above that; grid is one block per row.

Only loads, stores, the shuffle, the barrier and the trap are extern
primitives. Every index and arithmetic operation is Goose.

## Result

| gate | result |
|---|---|
| GPU differential vs the actual native templates | **2 269 rows, 0 differences**, 0 launch failures |
| host plan | **16 checks, 0 failures** |
| compute-sanitizer memcheck | 0 errors |
| compute-sanitizer racecheck | 0 hazards |
| compute-sanitizer initcheck | 0 errors |
| compute-sanitizer synccheck | 0 errors |
| mutation gate | **5/5 caught** |

The fixture includes `reduce_rows.cuh` directly, so the oracle is the native
kernel rather than a transcription. Twelve cases span `ncols` 1..4096, both
block-size branches of the heuristic, sum and mean, and contiguous and strided
layouts with padded rows and planes. Three deterministic data patterns are
used: all ones, pseudo-random values, and a large-offset pattern, so a changed
association cannot hide behind a tolerant comparison -- the comparison is
`memcmp` on each float.

Mutations and failures:

| mutation | failures |
|---|---:|
| first accumulator add becomes subtract | 2 269 |
| first shuffle offset 16 becomes 8 | 2 269 |
| cross-warp shared read uses stride 8 instead of 4 | 1 064 |
| mean division becomes multiplication | 827 |
| strided row selector uses `ne1+1` | 820 |

The test harness itself had two defects, both caught before the result was
used: the Goose wrapper first passed `row = 0` instead of `blockIdx.x`, and the
host call site then tried to read `blockIdx.x` outside a kernel. Both were
fixture errors; the module was unchanged by the fixes.

## Integration

Not linked. `GGML_OP_SUM_ROWS` / `GGML_OP_MEAN` are not constructed by the
fixed Apex profile, and the accepted runtime reaches native code for them. The
module is therefore OFF_PATH under the EXP245/246/247 rule. Adoption is
mechanical: call `fg414dd_gg_function_1` with the row and shared pointer, and
the host plan with the packet described in `host.goose`.

## P3-02 criterion check

| clause | evidence |
|---|---|
| device math | EXP166 (activations/GLU/scale/clamp), EXP167 (binary/broadcast/fused) |
| conversions | EXP248-272: every templated dequantizer in `dequantize.cuh` to F32/F16/BF16, byte/bit exact, rounding and bf16 stores mutation-gated; EXP178/180 activation quantizers |
| warp/block helpers | this experiment (warp xor tree, cross-warp staging, block sum) plus EXP273's warp count reduction |
| generated device provenance | `emission.json` records the pinned compiler and the SHA-256 of both generated C files |
| rounding/half/bfloat/warp boundaries | EXP248-272 half/bf16 destinations, EXP166 xIELU 1-ULP FMA case, this experiment's association-sensitive rows |
| `mma.cuh` | tensor-core instructions (`mma`, `ldmatrix`) are the primitive boundary declared in `APEX-FINAL-20261112.md`, not owned arithmetic |

With those mappings, P3-02 is closed. The remaining CUDA work belongs to the
other P3 items (MMVQ/MMQ types, FA cache variants, source removal), not to this
criterion.

## Reproduce

    python3 emit_sources.py
    bash build_fixture.sh
    runtime/exp414/build/host-plan
    runtime/exp414/build/gpu-fixture
    python3 mutate.py
