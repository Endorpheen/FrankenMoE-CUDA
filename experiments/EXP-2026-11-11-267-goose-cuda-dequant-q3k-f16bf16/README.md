# EXP267: CUDA Q3_K dequantization to half and bfloat16 in Goose

`dequantize_q3_K<dst_t>` for `half` and `nv_bfloat16`. EXP251 covered the F32
destination.

## Verdict

**OFF_PATH** — bit-exact against both native instantiations; the fixed 32K/128
Apex profile never selects Q3_K through these convertors. No performance claim,
shipped DSO unchanged.

## Scope transferred

EXP251's arithmetic per destination: 64-thread (r/t/is0/l0/n/j), high-bit mask
`1 << (4n+j)`, scale index `8n+2j+is0` with its four source branches, the `2*j`
field shift, the conditional `-4`, and `dl = d*(us-32)`. Only the store differs.

Layout: `block_q3_K` is 110 bytes — `hmask[32]` at 0, `qs[64]` at 32,
`scales[12]` at 96, `ggml_half d` at 108.

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
| `d` offset 108 -> 106 | 4 |
| first-branch high bits `is+8` -> `is+4` | 8 |
| shift `2*j` -> `j` (F16 body) | 4 |
| BF16 store uses the half conversion | 4 |
| scale bias 32 -> 31 | 4 |

No defect was found on the first build.

## Not integrated

Same rule as EXP245–266: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

F16/BF16 for the IQ family (IQ4_NL, IQ4_XS, IQ2_XXS, IQ2_XS, IQ2_S, IQ3_XXS,
IQ3_S, IQ1_S, IQ1_M) and MXFP4; the activation quantizer and MMQ stay blocked on
the EXP180 emitter address typing.
