# EXP261: CUDA IQ1_M dequantization in Goose

`dequantize_iq1_m` (`dequantize.cuh:386-405`) and its row wrapper. This closes
the super-block (QK_K = 256) part of `dequantize.cuh`: K-quants EXP182/248–251,
IQ-family EXP252–260 and MXFP4 EXP254 all have Goose implementations now.

## Verdict

**OFF_PATH** — bit-exact IQ1_M dequantization to F32, but the fixed 32K/128
Apex profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_IQ1_M)`. No
performance claim, shipped DSO unchanged.

## Scope transferred

- device: 32-thread decomposition `il = tid/8`, `ib = tid%8`; `ib16 = 2*ib + il/2`;
  the half scale pattern assembled from four `uint16` words
  `(s0>>12) | ((s1>>8)&0x00f0) | ((s2>>4)&0x0f00) | (s3&0xf000)`; the 3-bit
  scale `2*((sc[ib16/4] >> 3*(ib16%4)) & 7) + 1`; the delta bit
  `qh[2*ib+il/2] & (0x08 << 4*(il%2))` selecting `-1.125` or `-0.875`; the 11-bit
  grid index; the nibble split of the grid word; destination
  `i*QK_K + 32*ib + 8*il`; `d*(q+delta)`.
- host: `nb = k/QK_K`, grid `(nb,1,1)`, block 32, `k % QK_K != 0` rejected.

`iq1s_grid_gpu` is imported as an explicit device constant primitive; the half
bit-pattern conversion is one explicit device primitive.

Layout used: `block_iq1_m` is **56 bytes** — `uint8_t qs[32]` at 0,
`uint8_t qh[16]` at 32, `uint8_t scales[8]` at 48. There is **no `d` field** in
this block; the scale is packed inside the scales bytes.

## Gate

| check | result |
|---|---|
| `gpu-fixture` Goose vs native, 4 cases (1/4/64/256 blocks) | 12 checks / 0 failures |
| values compared bit-for-bit | 83 200 |
| every destination element written (canary) | 4 of 4 |
| `host-fixture` plan and rejections | 20 checks / 0 failures |

No defect was found on the first build. The 56-byte stride and the absent `d`
field were read from the struct before writing, after EXP260's stride mistake.

## Mutations

| mutation | failures |
|---|---:|
| block stride 56 -> 57 | 3 |
| `qh` index drops `il/2` | 4 |
| scale shift `3*(ib16%4)` -> `2*(ib16%4)` | 4 |
| half pattern `s3` mask `0xf000` -> `0x0f00` | 4 |
| destination `8*il` -> `4*il` | 8 |

## Not integrated

Same rule as EXP245–260: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

The 32-element legacy group in `dequantize.cuh` — `q1_0`, `q2_0`, the core
`q4_0`, `q4_1`, `q5_0`, `q5_1`, `q8_0` — stays native, as do the non-F32
destinations and the MMQ/MUL_MAT families. MMQ remains blocked on the emitter
address typing recorded in EXP180.
