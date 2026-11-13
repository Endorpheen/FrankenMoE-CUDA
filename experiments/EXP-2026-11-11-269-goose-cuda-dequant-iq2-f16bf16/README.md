# EXP269: CUDA IQ2_XXS and IQ2_XS dequantization to half and bfloat16 in Goose

`dequantize_iq2_xxs<dst_t>` and `dequantize_iq2_xs<dst_t>` for `half` and
`nv_bfloat16`. EXP255/EXP256 covered the F32 destinations.

## Verdict

**OFF_PATH** — bit-exact against all four native instantiations; the fixed
32K/128 Apex profile never selects these types through the F16/BF16 convertors.
No performance claim, shipped DSO unchanged.

## Scope transferred

The F32 arithmetic per destination: IQ2_XXS's q2 word layout, aux8 grid index,
aux32 exponent and `7*il` sign index; IQ2_XS's per-thread q2 word, 9-bit grid
index, 7-bit sign index and `scales[ib]` sub-block scale. The shared tables
`iq2xxs_grid`, `iq2xs_grid`, `ksigns_iq2xs` are explicit device constant
primitives. Only the store differs.

Layouts: `block_iq2_xxs` 66 bytes (`d` 0, `qs[32]` u16 at 2);
`block_iq2_xs` 74 bytes (`d` 0, `qs[32]` u16 at 2, `scales[8]` at 66).

## Gate

| check | result |
|---|---|
| IQ2_XXS F16 / BF16, 4 cases (1/4/64/256 blocks) | 83 200 values each |
| IQ2_XS F16 / BF16, 4 cases | 83 200 values each |
| total values compared | 332 800 |
| failures / launch failures | 0 / 0 |

## Mutations

| mutation | failures |
|---|---:|
| XXS scale factor 0.25 -> 0.5 (F16 body) | 4 |
| XXS sign index `7*il` -> `6*il` (F16 body) | 4 |
| XS grid mask 511 -> 255 (F16 body) | 4 |
| BF16 store uses the half conversion (shared helper) | 8 |
| XS scales index `ib` -> `2*ib` (F16 body) | 4 |

No defect was found on the first build.

## Not integrated

Same rule as EXP245–268: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

F16/BF16 for IQ2_S, IQ3_XXS, IQ3_S, IQ1_S, IQ1_M and MXFP4; the activation
quantizer and MMQ stay blocked on the EXP180 emitter address typing.
