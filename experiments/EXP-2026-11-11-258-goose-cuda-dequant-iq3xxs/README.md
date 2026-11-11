# EXP258: CUDA IQ3_XXS dequantization in Goose

`dequantize_iq3_xxs` (`dequantize.cuh:324-343`) and its row wrapper, the first
3-bit grid path after the IQ2 family EXP255–257.

## Verdict

**OFF_PATH** — bit-exact IQ3_XXS dequantization to F32, but the fixed 32K/128
Apex profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_IQ3_XXS)`. No
performance claim, shipped DSO unchanged.

## Scope transferred

- device: 32-thread decomposition `il = tid/8`, `ib = tid%8`; the two grid
  indices `q3[2*il]` and `q3[2*il+1]` from `qs + 8*ib`; the packed gas words at
  `qs + QK_K/4 + 4*ib`; `aux32 = gas[0] | gas[1] << 16`; the exponent
  `aux32 >> 28`; the per-`il` sign index; `d = x.d * (0.5 + exponent) * 0.5`;
  destination `i*QK_K + 32*ib + 8*il`; four bytes from each grid word and the
  conditional signs.
- host: `nb = k/QK_K`, grid `(nb,1,1)`, block 32, `k % QK_K != 0` rejected.

`iq3xxs_grid` (256 `uint32_t`) and `ksigns_iq2xs` are imported as explicit
device constant primitives (data, not code).

Layout used: `block_iq3_xxs` is 98 bytes — `ggml_half d` at 0,
`uint8_t qs[96]` at 2. `QK_K = 256`.

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
| `q3` stride `8*ib` -> `4*ib` | 4 |
| gas offset `66+4*ib` -> `66+2*ib` | 4 |
| exponent shift 28 -> 27 | 4 |
| second grid group `j+4` -> `j+8` | 8 |
| sign index shift `7*il` -> `6*il` | 4 |

## Not integrated

Same rule as EXP245–257: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

`iq3_s`, `iq1_s`, `iq1_m` stay native, as do the non-F32 destinations and the
MMQ/MUL_MAT families. MMQ remains blocked on the emitter address typing recorded
in EXP180.
