# EXP253: CUDA IQ4_XS dequantization in Goose

`dequantize_iq4_xs` (`dequantize.cuh:425-438`) plus the exact launch wrapper,
the second IQ-family path after EXP252 (IQ4_NL).

## Verdict

**OFF_PATH** — bit-exact IQ4_XS dequantization to F32, but the fixed 32K/128
Apex profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_IQ4_XS)`. No
performance claim, shipped DSO unchanged.

## Scope transferred

- device: 32-thread decomposition `il = tid/8`, `ib = tid%8`; destination
  `i*QK_K + 32*ib + 4*il`; the per-32 scale
  `((scales_l[ib/2] >> 4*(ib%2)) & 0xF) | (((scales_h >> 2*ib) & 3) << 4)`;
  `d = x[i].d * (scale - 32)`; the nibble split; `d * kvalues_iq4nl[q]`.
- host: `nb = ceil(k/256)`, grid `(nb,1,1)`, block 32. A `k` that is not a
  multiple of `QK_K` is rejected: the native function has no tail guard and
  always processes whole 256-element blocks, so such a `k` would read past the
  last block and write past the destination.

The 16-entry `kvalues_iq4nl` table is reproduced as an explicit Goose constant
function, as in EXP252.

Layout used: `block_iq4_xs` is 136 bytes — `ggml_half d` at 0,
`uint16_t scales_h` at 2, `uint8_t scales_l[4]` at 4, `uint8_t qs[128]` at 8.

## Gate

| check | result |
|---|---|
| `gpu-fixture` Goose vs native, 4 cases (1/4/64/256 blocks) | 12 checks / 0 failures |
| values compared bit-for-bit | 83 200 |
| every destination element written (canary) | 4 of 4 |
| `host-fixture` plan and rejections | 20 checks / 0 failures |

No defect was found on the first build; the layout and the scale assembly were
read from the struct and the source before writing.

## Mutations

| mutation | failures |
|---|---:|
| `qs` offset 8 -> 9 | 4 |
| `qs` row offset `4*il` -> `2*il` | 4 |
| `scales_l` index `ib/2` -> `ib` | 4 |
| scale bias 32 -> 31 | 4 |
| `scales_h` shift `2*ib` -> `ib` | 4 |

## Not integrated

Same rule as EXP245–252: emitted and gated, deliberately outside the candidate
link, because a witness for this route reads zero in the fixed profile.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

`iq1_s`/`iq1_m`, `iq2_xxs`/`iq2_xs`/`iq2_s`, `iq3_xxs`/`iq3_s` and MXFP4 stay
native (they need the shared grid/sign tables), as do the non-F32 destinations
and the MMQ/MUL_MAT families. MMQ remains blocked on the emitter address typing
recorded in EXP180.
