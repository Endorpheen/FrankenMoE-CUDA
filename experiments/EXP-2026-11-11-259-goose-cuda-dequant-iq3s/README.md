# EXP259: CUDA IQ3_S dequantization in Goose

`dequantize_iq3_s` (`dequantize.cuh:345-363`) and its row wrapper, the second
3-bit grid path after EXP258 (IQ3_XXS).

## Verdict

**OFF_PATH** — bit-exact IQ3_S dequantization to F32, but the fixed 32K/128
Apex profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_IQ3_S)`. No
performance claim, shipped DSO unchanged.

## Scope transferred

- device: 32-thread decomposition `il = tid/8`, `ib = tid%8`; the two 9-bit grid
  indices `qs[2*il] | ((qh[ib] << (8-2*il)) & 256)` and
  `qs[2*il+1] | ((qh[ib] << (7-2*il)) & 256)`; the per-32 scale
  `1 + 2*((scales[ib/2] >> 4*(ib%2)) & 0xf)`; `d = x.d * scale`; the sign byte
  at `signs[4*ib+il]`; destination `i*QK_K + 32*ib + 8*il`; four bytes from each
  grid word and the conditional signs.
- host: `nb = k/QK_K`, grid `(nb,1,1)`, block 32, `k % QK_K != 0` rejected.

`iq3s_grid` (512 `uint32_t`) is imported as an explicit device constant
primitive (data, not code). `kmask_iq2xs[j]` is `1 << j` and is computed here.

Layout used: `block_iq3_s` is 110 bytes — `ggml_half d` at 0, `uint8_t qs[64]`
at 2, `uint8_t qh[8]` at 66, `uint8_t signs[32]` at 74, `uint8_t scales[4]`
at 106. `QK_K = 256`.

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
| `qs` stride `8*ib` -> `4*ib` | 4 |
| grid1 `qh` shift `8-2*il` -> `6-2*il` | 4 |
| grid2 `qh` shift `7-2*il` -> `5-2*il` | 4 |
| sign byte offset 74 -> 73 | 4 |
| scale shift `4*(ib%2)` -> `2*(ib%2)` | 4 |

## Not integrated

Same rule as EXP245–258: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

`iq1_s` and `iq1_m` stay native, as do the non-F32 destinations and the
MMQ/MUL_MAT families. MMQ remains blocked on the emitter address typing recorded
in EXP180.
