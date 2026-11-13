# EXP272: CUDA IQ1_M and MXFP4 dequantization to half and bfloat16 in Goose

`dequantize_iq1_m<dst_t>` and `dequantize_mxfp4<dst_t>` for `half` and
`nv_bfloat16`. EXP261/EXP254 covered the F32 destinations. This completes the
F32/F16/BF16 matrix for every templated dequantizer in `dequantize.cuh`.

## Verdict

**OFF_PATH** — bit-exact against all four native instantiations; the fixed
32K/128 Apex profile never selects these types through the F16/BF16 convertors.
No performance claim, shipped DSO unchanged.

## Scope transferred

The F32 arithmetic per destination: IQ1_M's `ib16` sub-block, half scale pattern
from four uint16 words, 3-bit scale, delta bit, 11-bit grid index and nibble
split; MXFP4's manual E8M0 bit path, 8-blocks-per-CTA block index, nibble split
and `d*kv*0.5`. `iq1s_grid_gpu` is an explicit device constant import; the two
bitcast conversions (half bits→float, u32→float) are explicit primitives. Only
the store differs.

Layouts: `block_iq1_m` 56 bytes (`qs[32]` 0, `qh[16]` 32, `scales[8]` 48, no `d`
field); `block_mxfp4` 17 bytes (`e` 0, `qs[16]` 1).

## Gate

| check | result |
|---|---|
| IQ1_M F16 / BF16, 4 cases (1/4/64/256 blocks) | 83 200 values each |
| MXFP4 F16 / BF16, 4 cases | 83 200 values each |
| total values compared | 332 800 |
| failures / launch failures | 0 / 0 |

## Defect in my own work, found by the gate

The first build reported 6 failures in MXFP4 F16, all signed-zero (`0000` vs
`8000`). Cause: the fixture allocated one 17-byte block per CTA, while
`dequantize_mxfp4` addresses `base + i*8` blocks — one CTA always consumes 8
blocks = 256 elements. The out-of-range reads produced the zero-sign noise. This
is the same fixture mistake as EXP254; the corrected size is `nblocks*8*17`,
which is now written explicitly at the call site. The module needed no change.

## Mutations

| mutation | failures |
|---|---:|
| IQ1_M half pattern `s3` mask `0xf000` -> `0x0f00` | 4 |
| IQ1_M qh index drops `il/2` (F16 body) | 4 |
| MXFP4 E8M0 shift 23 -> 22 (F16 body) | 8 |
| MXFP4 drops the 0.5 factor (F16 body) | 4 |
| BF16 store uses the half conversion (IQ1_M body) | 4 |

## Not integrated

Same rule as EXP245–271: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

The activation quantizer (`quantize_mmq_q8_1`, blocked on the EXP180 emitter
address typing), the shared `dequantize_block_cuda` launch template and the
MMQ/MUL_MAT families. All templated dequantizers now have F32, F16 and BF16.
