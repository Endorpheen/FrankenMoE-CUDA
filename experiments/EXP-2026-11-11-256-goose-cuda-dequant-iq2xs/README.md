# EXP256: CUDA IQ2_XS dequantization in Goose

`dequantize_iq2_xs` (`dequantize.cuh:299-315`) and its row wrapper, the second
grid-table IQ path after EXP255 (IQ2_XXS).

## Verdict

**OFF_PATH** — bit-exact IQ2_XS dequantization to F32, but the fixed 32K/128
Apex profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_IQ2_XS)`. No
performance claim, shipped DSO unchanged.

## Scope transferred

- device: 32-thread decomposition `il = tid/8`, `ib = tid%8`; the per-thread
  `q2` word at byte `2 + 8*ib + 2*il`; the 9-bit grid index `q2 & 511`; the
  7-bit sign index `q2 >> 9`; the per-sub-block scale `scales[ib]` shifted by
  `4*(il/2)`; `d = x.d * (0.5 + scale) * 0.25`; destination
  `i*QK_K + 32*ib + 8*il`; per-element grid byte extraction and the conditional
  sign. Eight values per thread.
- host: `nb = k/QK_K`, grid `(nb,1,1)`, block 32, `k % QK_K != 0` rejected.

The shared tables `iq2xs_grid` and `ksigns_iq2xs` are imported as explicit
device constant primitives (data, not code); every index and arithmetic
operation is Goose.

Layout used: `block_iq2_xs` is 74 bytes — `ggml_half d` at 0,
`uint16_t qs[32]` at 2, `uint8_t scales[8]` at 66. `QK_K = 256`.

## Gate

| check | result |
|---|---|
| `gpu-fixture` Goose vs native, 4 cases (1/4/64/256 blocks) | 12 checks / 0 failures |
| values compared bit-for-bit | 83 200 |
| every destination element written (canary) | 4 of 4 |
| `host-fixture` plan and rejections | 20 checks / 0 failures |

No defect was found on the first build.

## Mutations

| mutation | failures |
|---|---:|
| `q2` stride `8*ib` -> `4*ib` | 4 |
| destination `8*il` -> `4*il` | 8 |
| `scales` index `ib` -> `2*ib` | 4 |
| scale shift `4*(il/2)` -> `2*(il/2)` | 4 |
| grid mask 511 -> 255 | 4 |

## Not integrated

Same rule as EXP245–255: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

`iq2_s`, `iq3_xxs`, `iq3_s`, `iq1_s`, `iq1_m` stay native, as do the non-F32
destinations and the MMQ/MUL_MAT families. MMQ remains blocked on the emitter
address typing recorded in EXP180.
