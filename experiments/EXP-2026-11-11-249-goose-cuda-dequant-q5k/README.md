# EXP249: CUDA Q5_K dequantization in Goose

`dequantize_q5_K` (`dequantize.cuh:214-243`), the third dequantisation path after
EXP182 (Q4_K/F32), EXP188 (Q4_0/F16) and EXP248 (Q6_K/F32). It shares its
scale/min helper with Q4_K but has its own 5-bit layout and high-bit plane.

## Verdict

**OFF_PATH** — bit-exact Q5_K dequantization to F32, but the fixed 32K/128 Apex
profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_Q5_K)`; the Q4_K_M model has
no Q5_K tensors and MMQ bypasses the row selector. No performance claim, shipped
DSO unchanged.

## Scope transferred

`dequantize_q5_K<dst_t=float>` and the row contract in
`dequantize_row_q5_K_cuda`:

- device: 64-thread decomposition `il = tid/16`, `ir = tid%16`, `is = 2*il`;
  destination base `64*il + 2*ir`; `dall`/`dmin` from the packed half2;
  `get_scale_min_k4` for both scale/min pairs; the 4-bit low nibble plus the
  conditionally added 16 from the `qh` high-bit plane (`hm = 1 << 2*il`, then
  `hm <<= 1` for the high nibbles); affine `d*q - m`. Four values per thread.
- host: `nb = k/QK_K`, grid `(nb,1,1)`, block 64, tail rejected.

Extern symbols are only byte loads, two half unpacks and a float store.

Layout used: `block_q5_K` is 176 bytes — `ggml_half2 dm` at 0 (`d` at 0,
`dmin` at 2), `uint8_t scales[12]` at 4, `uint8_t qh[32]` at 16,
`uint8_t qs[128]` at 48. `QK_K = 256`.

## Gate

The oracle is the real `dequantize_q5_K` template compiled from the read-only
source tree into the same fixture; both arms run on the GPU over random raw block
bytes, so every 5-bit pattern, every 6-bit scale/min pair and the whole high-bit
plane are exercised.

| check | result |
|---|---|
| `gpu-fixture` Goose vs native, 4 cases (1/4/64/256 blocks) | 12 checks / 0 failures |
| values compared bit-for-bit | 83 200 |
| every destination element written (canary) | 4 of 4 |
| `host-fixture` plan and rejections | 22 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| second-half mask `hm1` -> `hm0` | 3 |
| `qs` offset 48 -> 49 | 3 |
| destination `2*ir` -> `ir` | 8 |
| `a1` reads `ql0` instead of `ql1` | 3 |
| `d2` reuses scale `is` instead of `is+1` | 3 |

## Not integrated

Same rule as EXP245/246/247/248: emitted and gated, deliberately outside the
candidate link, because a witness that would prove this route reads zero in the
fixed profile.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

Q2_K, Q3_K, the IQ family and MXFP4 stay native, as do the non-F32 destinations
of Q4_K/Q5_K/Q6_K and the MMQ/MUL_MAT families themselves. MMQ remains blocked on
the emitter address typing recorded in EXP180.
