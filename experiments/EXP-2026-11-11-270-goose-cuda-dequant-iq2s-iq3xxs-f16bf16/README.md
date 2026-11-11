# EXP270: CUDA IQ2_S and IQ3_XXS dequantization to half and bfloat16 in Goose

`dequantize_iq2_s<dst_t>` and `dequantize_iq3_xxs<dst_t>` for `half` and
`nv_bfloat16`. EXP257/EXP258 covered the F32 destinations.

## Verdict

**OFF_PATH** — bit-exact against all four native instantiations; the fixed
32K/128 Apex profile never selects these types through the F16/BF16 convertors.
No performance claim, shipped DSO unchanged.

## Scope transferred

The F32 arithmetic per destination: IQ2_S's 10-bit grid index, per-sub-block
scale, sign byte and 8-value loop; IQ3_XXS's two grid indices, packed gas words,
exponent, per-`il` sign index and two 4-value groups. `iq2s_grid`,
`iq3xxs_grid` and `ksigns_iq2xs` are explicit device constant imports. Only the
store differs.

Layouts: `block_iq2_s` 82 bytes (`d` 0, `qs[64]` 2, `qh[8]` 66, `scales[8]` 74);
`block_iq3_xxs` 98 bytes (`d` 0, `qs[96]` 2).

## Gate

| check | result |
|---|---|
| IQ2_S F16 / BF16, 4 cases (1/4/64/256 blocks) | 83 200 values each |
| IQ3_XXS F16 / BF16, 4 cases | 83 200 values each |
| total values compared | 332 800 |
| failures / launch failures | 0 / 0 |

## Mutations

| mutation | failures |
|---|---:|
| IQ2_S grid byte drops `il` (F16 body) | 4 |
| IQ2_S qh shift `8-2*il` -> `6-2*il` (F16 body) | 4 |
| IQ3_XXS exponent 28 -> 27 (F16 body) | 4 |
| IQ3_XXS second group `+4` -> `+8` (F16 body) | 4 |
| BF16 store uses the half conversion (IQ3_XXS body) | 4 |

No defect was found on the first build.

## Not integrated

Same rule as EXP245–269: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

F16/BF16 for IQ3_S, IQ1_S, IQ1_M and MXFP4; the activation quantizer and MMQ
stay blocked on the EXP180 emitter address typing.
