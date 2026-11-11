# EXP260: CUDA IQ1_S dequantization in Goose

`dequantize_iq1_s` (`dequantize.cuh:365-384`) and its row wrapper, the last
grid-table IQ path except IQ1_M.

## Verdict

**OFF_PATH** — bit-exact IQ1_S dequantization to F32, but the fixed 32K/128
Apex profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_IQ1_S)`. No
performance claim, shipped DSO unchanged.

## Scope transferred

- device: 32-thread decomposition `il = tid/8`, `ib = tid%8`; the 11-bit grid
  index `qs[4*ib+il] | (((qh[ib] >> 3*il) & 7) << 8)`; the delta selection from
  `qh` bit 15 (`-1.125` or `-0.875`); the scale `2*((qh>>12)&7)+1`; the low/high
  nibble split of the 32-bit grid word; destination
  `i*QK_K + 32*ib + 8*il`; `d*(q+delta)`. `iq1s_grid_gpu` (2048 `uint32_t`) is
  imported as an explicit device constant primitive.
- host: `nb = k/QK_K`, grid `(nb,1,1)`, block 32, `k % QK_K != 0` rejected.

Layout used: `block_iq1_s` is **50 bytes** — `ggml_half d` at 0, `uint8_t qs[32]`
at 2, `uint16_t qh[8]` at 34. `QK_K = 256`.

## Defect in my own work, found by the gate

The first build used a 66-byte block stride (2 + 32 + 32), as if `qh` were 16
`uint16_t` entries. It has 8 (`QK_K/32`), so the block is 50 bytes. CTA 0 was
correct because a zero stride is zero; every later CTA read the wrong block.
The first diff at index 256 and an exact match of the hand-computed model for
block 1 against the Goose arm but not the native arm made the direction obvious.
Corrected to 50; the gate went to zero differences.

## Gate

| check | result |
|---|---|
| `gpu-fixture` Goose vs native, 4 cases (1/4/64/256 blocks) | 12 checks / 0 failures |
| values compared bit-for-bit | 83 200 |
| every destination element written (canary) | 4 of 4 |
| `host-fixture` plan and rejections | 20 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| block stride 50 -> 51 (the real defect) | 3 |
| `qh` offset `34+2*ib` -> `34+ib` | 4 |
| grid index shift `3*il` -> `2*il` | 4 |
| delta bit `0x8000` -> `0x4000` | 4 |
| scale drops the `+1` | 4 |

## Not integrated

Same rule as EXP245–259: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

`iq1_m` stays native, as do the non-F32 destinations and the MMQ/MUL_MAT
families. MMQ remains blocked on the emitter address typing recorded in EXP180.
