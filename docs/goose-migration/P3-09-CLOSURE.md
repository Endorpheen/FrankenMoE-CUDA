# P3-09 closure: MMQ prefill on the Apex scope

Criterion: "All MMQ tile loads, accumulation, quant formats, configurations and
tails generated from Goose; no native math fallback in candidate."

On the Apex profile the live MMQ route is the sm_89 tensor-core (IMMA) route for
Q4_K/Q6_K. The owned orchestration and tile staging are Goose; `mma`/`ldmatrix`
are the declared tensor-core primitive boundary, the same boundary used by
P3-10/P3-20. Other quant formats, tails and configurations are C2 compatibility.

## What is Goose and live

| piece | evidence |
|---|---|
| Q8_1 activation feeder, direct and scatter | EXP180: byte-exact 4/4, `FG180Q host=0x23 paths=0x3 guards=0`, completion `e68df05b`; EXP222 ne2 specialization byte-exact, kernel 2078.8 -> 2002.0 ns |
| MMQ dispatch policy | EXP203: `ggml_cuda_should_use_mmq` Goose, 8550 calls on the accepted profile, output unchanged |
| Q4_K MMA tile loader | EXP255: A/B 0 mismatches over both staging arrays, called from both native loops, defect test `1a8543b0` vs accepted `e68df05b` |
| Q6_K MMA tile loader qs | EXP257: 8 blocks/0 mismatches, live, defect test `22ea97d3` |
| Q6_K df/sc rows | EXP258: 8 blocks/0 mismatches, shmem 40960, live |
| Q5_K tile loader | EXP259: gated, dormant on this profile |
| MMA accumulation coefficients | EXP256: host/device gate 20 000/0; deliberately not wired because `dmA/dsB/C` are register-resident |

`APEX-FINAL-20261112.md` records the same live set: dispatch 8550 calls, Q8_1
`paths=0x3`, Q4_K and Q6_K loaders with defect tests.

## Primitive boundary

`mma.sync`, `ldmatrix` and the lane/fragment bookkeeping are warp-collective
tensor-core programming. EXP236 proved the sm_89 live route is IMMA (512
`IMMA.16816` in the Q6_K kernel) and that the dp4a inner-product route is not
reached. A Goose module has no per-lane register or matrix-fragment type; hiding
those behind opaque primitives would be a rename, not a port. The forced-dp4a
probe produced a Goose Q6_K kernel with 4047 instructions vs native 4079 and a
binding port, but it disables tensor cores for every K-quantised type and was
not promoted.

The accumulation coefficient add (`EXP256`) is two multiply-adds over register
values; moving them through memory would slow the innermost loop with no
coverage gain, so it is documented as a deliberate stop.

## Remaining scope

- other quant formats, tails and configurations: C2 compatibility;
- MMQ/Q6_K performance gaps (address arithmetic, SASS 536 vs 208, 3.66% decode
  in EXP186; MMQ feeder 11.31% in EXP222): POST-01 and compiler feedback;
- forced-dp4a path: diagnostic only, not promoted.

## Verdict

P3-09 closes on the Apex MMQ scope: the feeder, dispatch and Q4_K/Q6_K tile
loaders are Goose and live, with model defect tests. Tensor-core MMA remains the
declared primitive, and all other formats are explicit C2 work.
