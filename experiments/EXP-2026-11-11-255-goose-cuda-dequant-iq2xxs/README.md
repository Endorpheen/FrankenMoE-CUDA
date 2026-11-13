# EXP255: CUDA IQ2_XXS dequantization in Goose

`dequantize_iq2_xxs` (`dequantize.cuh:272-297`) and its row wrapper, the first
grid-table IQ path after the lattice-free EXP252/253/254 modules.

## Verdict

**OFF_PATH** — bit-exact IQ2_XXS dequantization to F32, but the fixed 32K/128
Apex profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_IQ2_XXS)`. No
performance claim, shipped DSO unchanged.

## Scope transferred

- device: 32-thread decomposition `il = tid/8`, `ib = tid%8`; the `q2` word
  layout (`qs + 4*ib` in `uint16_t` units, byte offset `2 + 8*ib`); the `aux8`
  grid index at `q2 + il`; `aux32 = q2[2] | q2[3] << 16`; the exponent
  `aux32 >> 28`; the per-`il` sign index `(aux32 >> 7*il) & 127`; destination
  `i*QK_K + 32*ib + 8*il`; per-element grid byte extraction and the conditional
  sign. Eight values per thread.
- host: `nb = k/QK_K`, grid `(nb,1,1)`, block 32, `k % QK_K != 0` rejected.

The two shared tables are **data, not code**: `iq2xxs_grid` and `ksigns_iq2xs`
are read through explicit constant primitives (`grid(i)`, `ksign(i)`), the same
category as `ld_u8`. Every index computation, every arithmetic operation and
every branch is Goose; no native routine is called.

Layout used: `block_iq2_xxs` is 66 bytes — `ggml_half d` at 0,
`uint16_t qs[32]` at 2. `QK_K = 256`.

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
| sign index shift `7*il` -> `6*il` | 4 |
| scale factor 0.25 -> 0.5 | 4 |
| sign ignored | 4 |

## Not integrated

Same rule as EXP245–254: emitted and gated, deliberately outside the candidate
link, because a witness for this route reads zero in the fixed profile.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

`iq2_xs`, `iq2_s`, `iq3_xxs`, `iq3_s`, `iq1_s`, `iq1_m` stay native, as do the
non-F32 destinations and the MMQ/MUL_MAT families. MMQ remains blocked on the
emitter address typing recorded in EXP180.
