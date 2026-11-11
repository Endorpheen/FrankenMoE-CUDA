# EXP250: CUDA Q2_K dequantization in Goose

`dequantize_q2_K` (`dequantize.cuh:128-145`), the fourth dequantisation path after
EXP182 (Q4_K/F32), EXP188 (Q4_0/F16), EXP248 (Q6_K/F32) and EXP249 (Q5_K/F32).

## Verdict

**OFF_PATH** — bit-exact Q2_K dequantization to F32, but the fixed 32K/128 Apex
profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_Q2_K)`. No performance
claim, shipped DSO unchanged.

## Scope transferred

`dequantize_q2_K<dst_t=float>` and the row contract in
`dequantize_row_q2_K_cuda`:

- device: 64-thread decomposition `n = tid/32`, `l = tid - 32*n`,
  `is = 8*n + l/16`; destination base `i*QK_K + 128*n + l`; `dall`/`dmin` from
  the packed dm; one `qs` byte supplies all four 2-bit fields; each `scales`
  byte carries a 4-bit scale and a 4-bit min; affine
  `dall*sc*q - dmin*min`. Four values per thread.
- host: `nb = k/QK_K`, grid `(nb,1,1)`, block 64, tail rejected.

Extern symbols are only byte loads, two half unpacks and a float store.

Layout used: `block_q2_K` is 84 bytes — `uint8_t scales[16]` at 0,
`uint8_t qs[64]` at 16, `ggml_half2 dm` at 80 (`d` at 80, `dmin` at 82).
`QK_K = 256`.

## Gate

The oracle is the real `dequantize_q2_K` template compiled from the read-only
source tree into the same fixture.

| check | result |
|---|---|
| `gpu-fixture` Goose vs native, 4 cases (1/4/64/256 blocks) | 12 checks / 0 failures |
| values compared bit-for-bit | 83 200 |
| every destination element written (canary) | 4 of 4 |
| `host-fixture` plan and rejections | 22 checks / 0 failures |

## Defect in my own work, found by the gate

The first build differed at index 0 in all four cases. Cause: I carried the
Q5_K layout over mechanically and read `d`/`dmin` at block offsets 0/2. In
`block_q2_K` the dm pair is **at the end of the block**, offsets 80/82, after
`scales[16]` and `qs[64]`. The offsets were corrected and the gate went to zero
differences. This is exactly why the native oracle, not a hand-written formula,
is the reference.

## Mutations

| mutation | failures |
|---|---:|
| `dmin` offset 82 -> 80 (reuses `d`) | 4 |
| `qs` offset 16 -> 17 | 4 |
| second field shift 2 -> 1 | 4 |
| destination `128*n` -> `127*n` | 8 |
| last scale `is+6` -> `is+4` | 4 |

## Not integrated

Same rule as EXP245–249: emitted and gated, deliberately outside the candidate
link, because a witness for this route reads zero in the fixed profile.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

Q3_K, the IQ family and MXFP4 stay native, as do the non-F32 destinations of the
K-quants and the MMQ/MUL_MAT families. MMQ remains blocked on the emitter address
typing recorded in EXP180.
