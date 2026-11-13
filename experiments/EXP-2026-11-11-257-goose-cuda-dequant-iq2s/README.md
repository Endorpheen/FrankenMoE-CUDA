# EXP257: CUDA IQ2_S dequantization in Goose

`dequantize_iq2_s` (`dequantize.cuh:314-326`) and its row wrapper, the third
grid-table IQ path after EXP255 (IQ2_XXS) and EXP256 (IQ2_XS).

## Verdict

**OFF_PATH** — bit-exact IQ2_S dequantization to F32, but the fixed 32K/128
Apex profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_IQ2_S)`. No
performance claim, shipped DSO unchanged.

## Scope transferred

- device: 32-thread decomposition `il = tid/8`, `ib = tid%8`; the 10-bit grid
  index `qs[4*ib+il] | ((qh[ib] << (8-2*il)) & 0x300)`; the per-sub-block scale
  `scales[ib] >> 4*(il/2)`; `d = x.d * (0.5 + scale) * 0.25`; the sign byte at
  `qs[32 + 4*ib + il]`; destination `i*QK_K + 32*ib + 8*il`; grid byte
  extraction and the conditional sign.
- host: `nb = k/QK_K`, grid `(nb,1,1)`, block 32, `k % QK_K != 0` rejected.

Tables `iq2s_grid` (1024 entries) and `ksigns_iq2xs` are imported as explicit
device constant primitives (data, not code).

Layout used: `block_iq2_s` is 82 bytes — `ggml_half d` at 0, `uint8_t qs[64]`
at 2, `uint8_t qh[8]` at 66, `uint8_t scales[8]` at 74. `QK_K = 256`.

## Gate

| check | result |
|---|---|
| `gpu-fixture` Goose vs native, 4 cases (1/4/64/256 blocks) | 12 checks / 0 failures |
| values compared bit-for-bit | 83 200 |
| every destination element written (canary) | 4 of 4 |
| `host-fixture` plan and rejections | 20 checks / 0 failures |

No defect was found in the module on the first build. One script defect was
caught before the first build: a blanket `256 -> 257` rename also changed the
`sha256` calls, the `QK_K = 256` literals in the host module and the 256-block
test case. The scripts were regenerated with targeted prefix/path replacements
only; the gate above is from the corrected files.

## Mutations

| mutation | failures |
|---|---:|
| grid byte uses `4*ib` only (drops `il`) | 4 |
| `qh` shift `8-2*il` -> `6-2*il` | 4 |
| sign byte offset `+32` -> `+31` | 4 |
| scale shift `4*(il/2)` -> `2*(il/2)` | 4 |
| destination `8*il` -> `4*il` | 8 |

## Not integrated

Same rule as EXP245–256: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

`iq3_xxs`, `iq3_s`, `iq1_s`, `iq1_m` stay native, as do the non-F32
destinations and the MMQ/MUL_MAT families. MMQ remains blocked on the emitter
address typing recorded in EXP180.
