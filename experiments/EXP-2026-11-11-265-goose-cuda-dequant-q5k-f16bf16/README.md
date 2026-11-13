# EXP265: CUDA Q5_K dequantization to half and bfloat16 in Goose

`dequantize_q5_K<dst_t>` for `half` and `nv_bfloat16`. EXP249 covered the F32
destination.

## Verdict

**OFF_PATH** — bit-exact against both native instantiations; the fixed 32K/128
Apex profile never selects Q5_K through these convertors. No performance claim,
shipped DSO unchanged.

## Scope transferred

The F32 module's arithmetic per destination: 64-thread (il = tid/16,
ir = tid%16, is = 2*il), base `i*QK_K + 64*il + 2*ir`, packed dm scale/min,
both 6-bit pairs, the qh high-bit plane (`hm = 1<<2*il`, then `hm<<1`), and
`d*q - m`. Only the store differs (`__float2half` / `__float2bfloat16`).

Layout: `block_q5_K` is 176 bytes — `ggml_half2 dm` at 0 (`d` at 0, `dmin` at
2), `uint8_t scales[12]` at 4, `uint8_t qh[32]` at 16, `uint8_t qs[128]` at 48.

## Gate

| check | result |
|---|---|
| F16 destination, 4 cases (1/4/64/256 blocks) | 83 200 values bit-identical |
| BF16 destination, 4 cases | 83 200 values bit-identical |
| total values compared | 166 400 |
| failures / launch failures | 0 / 0 |

## Mutations

| mutation | failures |
|---|---:|
| `d2` reuses scale `is` (F16 body) | 4 |
| `qs` offset 48 -> 49 | 4 |
| high-half offset `+32` -> `+16` (F16 body) | 4 |
| BF16 store uses the half conversion | 4 |
| high-bit contribution 16 -> 15 (shared helper) | 8 |

No defect was found on the first build.

## Not integrated

Same rule as EXP245–264: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

F16/BF16 for Q2_K, Q3_K, the IQ family and MXFP4; the activation quantizer and
MMQ stay blocked on the EXP180 emitter address typing.
