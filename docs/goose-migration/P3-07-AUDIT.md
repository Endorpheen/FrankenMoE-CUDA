# P3-07 audit: dequantization complete, MMQ activation layouts partial

Criterion: "Every supported type and packed block path mapped to Goose with
byte/ULP fixtures and malformed-size rejection."

This audit maps every path in `dequantize.cuh` and `quantize.cu` to its
evidence. It closes the dequantization half and records the exact remaining
activation-quantizer layouts. P3-07 stays IN_PROGRESS because the criterion says
every supported packed block path, and three activation layouts are still
native.

## Dequantization: complete

`dequantize.cuh` has 22 functions. Every one has a Goose implementation, a
bit-exact differential against the native template, a mutation gate and a
recorded destination matrix.

### Legacy 32-element callbacks (output `float2`, destination-independent)

| function | evidence |
|---|---|
| `dequantize_q1_0` | EXP262 |
| `dequantize_q2_0` | EXP262 |
| `dequantize_q4_0` | EXP262, plus EXP188 for the `__half` block path |
| `dequantize_q4_1` | EXP262 |
| `dequantize_q5_0` | EXP262 |
| `dequantize_q5_1` | EXP262 |
| `dequantize_q8_0` | EXP262 |

EXP262 gate: 1 408 pairs / 2 816 float values, 0 failures, 5/5 mutations
(512/224/112/112/59). Three own defects found and fixed before commit: unaligned
`uint32` read of `qh`, sign extension of `q8_0`, and an invisible device failure
counter.

The `q4_0` V-cache dequantization actually executed by Apex Flash Attention is
`dequantize_V_q4_0` in `fattn-common.cuh`, transferred by EXP173; the live
`FG175FA_CUDA host=0xf` witness covers it. EXP188's row-selector seam is
bit-exact but not the path Apex takes, and that is recorded rather than counted
twice.

### K-quants (templated destination)

| function | F32 | half | bfloat16 |
|---|---|---|---|
| `dequantize_q2_K` | EXP250 | EXP266 | EXP266 |
| `dequantize_q3_K` | EXP251 | EXP267 | EXP267 |
| `dequantize_q4_K` | EXP182 | EXP263 | EXP263 |
| `dequantize_q5_K` | EXP249 | EXP265 | EXP265 |
| `dequantize_q6_K` | EXP248 | EXP264 | EXP264 |

Each gate is 83 200 values bit-identical per destination (166 400 for the two
16-bit destinations), 0 failures/0 launches, 5/5 mutations.

### IQ and MX families (templated destination)

| function | F32 | half/bf16 |
|---|---|---|
| `dequantize_iq4_nl` | EXP252 | EXP268 |
| `dequantize_iq4_xs` | EXP253 | EXP268 |
| `dequantize_iq2_xxs` | EXP255 | EXP269 |
| `dequantize_iq2_xs` | EXP256 | EXP269 |
| `dequantize_iq2_s` | EXP257 | EXP270 |
| `dequantize_iq3_xxs` | EXP258 | EXP270 |
| `dequantize_iq3_s` | EXP259 | EXP271 |
| `dequantize_iq1_s` | EXP260 | EXP271 |
| `dequantize_iq1_m` | EXP261 | EXP272 |
| `dequantize_mxfp4` | EXP254 | EXP272 |

The grid/sign tables are explicit device constant imports (data, not code);
every index and arithmetic operation is Goose. All modules are OFF_PATH and not
linked, because the Q4_K_M Apex profile reaches MMQ rather than the row
selector. They are deliverables with adoption recipes, not live routes.

## Activation quantization: DS4 complete, D4/D2S6/FP4 open

| path | state | evidence |
|---|---|---|
| `quantize_q8_1` / `quantize_row_q8_1_cuda` non-MMQ, `ids == nullptr` | Goose | EXP178: 5/5 block arrays byte-identical, memcheck 0, live `host=0x1 paths=0x1` |
| `quantize_mmq_q8_1` DS4, direct | Goose | EXP180: 4/4 arrays byte-identical, live `paths=0x3` |
| `quantize_mmq_q8_1` DS4, scatter | Goose | EXP180 |
| `quantize_mmq_q8_1` D4 | **native** | EXP180/EXP208 explicitly refuse it; host records layout 0 |
| `quantize_mmq_q8_1` D2S6 | **native** | EXP180/EXP208 explicitly refuse it; layout 2 |
| `quantize_mmq_mxfp4` direct/scatter | **native** | `quantize.cu` FP4 kernels, not ported |
| `quantize_mmq_nvfp4` direct/scatter | **native** | `quantize.cu` FP4 kernels, not ported |

`quantize_row_q8_1_cuda` asserts `!ids`, so the non-MMQ `ids` branch is not a
reachable supported path. The MMQ `n_expert_used` scatter form for DS4 is
ported; the D4/D2S6 scatter forms are part of the same missing layouts.

Apex Q4_K/Q6_K uses DS4 for its MMQ activation quantization, so the executed
profile is covered. The criterion is broader than the profile: it says every
supported packed block path. D4, D2S6 and the FP4 quantizers are supported by
the source and remain unimplemented in Goose.

## Verdict

- Dequantization clause: **complete**, 22/22 functions, three destinations for
  every templated function.
- Activation quantization clause: **partial**. DS4 direct+scatter is Goose;
  D4, D2S6, MXFP4 and NVFP4 direct/scatter are native.
- P3-07 stays IN_PROGRESS. Next implementation slice: D4 and D2S6
  `quantize_mmq_q8_1` layouts, then the FP4 quantizers. Do not mark the item
  DONE on the Apex profile alone.
