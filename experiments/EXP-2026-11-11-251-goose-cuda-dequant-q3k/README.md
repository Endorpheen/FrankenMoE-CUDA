# EXP251: CUDA Q3_K dequantization in Goose

`dequantize_q3_K` (`dequantize.cuh:147-186`), the fifth dequantisation path after
EXP182 (Q4_K/F32), EXP188 (Q4_0/F16), EXP248 (Q6_K/F32), EXP249 (Q5_K/F32) and
EXP250 (Q2_K/F32).

## Verdict

**OFF_PATH** — bit-exact Q3_K dequantization to F32, but the fixed 32K/128 Apex
profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_Q3_K)`. No performance
claim, shipped DSO unchanged.

## Scope transferred

`dequantize_q3_K<dst_t=float>` and the row contract in
`dequantize_row_q3_K_cuda`:

- device: 64-thread decomposition `r = tid/4`, `t = r/2`, `is0 = r%2`,
  `l0 = 16*is0 + 4*(tid%4)`, `n = t/4`, `j = t-4*n`; the high-bit mask
  `m = 1 << (4*n+j)`; the scale index `is = 8*n + 2*j + is0` with all four
  source branches; the 2-bit quant field shifted by `2*j`; the subtraction of 4
  when the high-bit mask is clear; `dl = d*(us-32)`; destination
  `i*QK_K + 128*n + 32*j + l` for four consecutive `l`. Extern symbols are only
  byte loads, one half unpack and a float store.
- host: `nb = k/QK_K`, grid `(nb,1,1)`, block 64, tail rejected.

Layout used: `block_q3_K` is 110 bytes — `uint8_t hmask[32]` at 0,
`uint8_t qs[64]` at 32, `uint8_t scales[12]` at 96, `ggml_half d` at 108.
`QK_K = 256`.

The reconstructed scale value stays in 0..63 in every branch, so the native
`int8_t` cast on it is the identity and the Goose i64 path agrees exactly.

## Gate

The oracle is the real `dequantize_q3_K` template compiled from the read-only
source tree into the same fixture.

| check | result |
|---|---|
| `gpu-fixture` Goose vs native, 4 cases (1/4/64/256 blocks) | 12 checks / 0 failures |
| values compared bit-for-bit | 83 200 |
| every destination element written (canary) | 4 of 4 |
| `host-fixture` plan and rejections | 22 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| `d` offset 108 -> 106 | 4 |
| first-branch high bits `is+8` -> `is+4` | 4 |
| shift `2*j` -> `j` | 4 |
| destination `32*j` -> `16*j` | 8 |
| scale bias 32 -> 31 | 4 |

No defect was found in this module on the first build; the layout was read from
the struct and the branch set from the source before writing.

## Not integrated

Same rule as EXP245–250: emitted and gated, deliberately outside the candidate
link, because a witness for this route reads zero in the fixed profile.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

The IQ family (`iq1_s`, `iq1_m`, `iq2_xxs`, `iq2_xs`, `iq2_s`, `iq3_xxs`,
`iq3_s`, `iq4_nl`, `iq4_xs`) and MXFP4 stay native, as do the non-F32
destinations and the MMQ/MUL_MAT families. MMQ remains blocked on the emitter
address typing recorded in EXP180.
