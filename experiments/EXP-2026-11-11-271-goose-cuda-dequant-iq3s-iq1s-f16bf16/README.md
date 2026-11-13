# EXP271: CUDA IQ3_S and IQ1_S dequantization to half and bfloat16 in Goose

`dequantize_iq3_s<dst_t>` and `dequantize_iq1_s<dst_t>` for `half` and
`nv_bfloat16`. EXP259/EXP260 covered the F32 destinations.

## Verdict

**OFF_PATH** — bit-exact against all four native instantiations; the fixed
32K/128 Apex profile never selects these types through the F16/BF16 convertors.
No performance claim, shipped DSO unchanged.

## Scope transferred

The F32 arithmetic per destination: IQ3_S's two 9-bit grid indices, per-32 scale
`1+2*((scales[ib/2]>>4*(ib%2))&0xf)`, sign byte and two 4-value groups; IQ1_S's
11-bit grid index, delta from `qh` bit 15, scale `2*((qh>>12)&7)+1` and the
nibble split. `iq3s_grid` and `iq1s_grid_gpu` are explicit device constant
imports. Only the store differs.

Layouts: `block_iq3_s` 110 bytes (`d` 0, `qs[64]` 2, `qh[8]` 66, `signs[32]` 74,
`scales[4]` 106); `block_iq1_s` 50 bytes (`d` 0, `qs[32]` 2, `qh[8]` u16 at 34).

## Gate

| check | result |
|---|---|
| IQ3_S F16 / BF16, 4 cases (1/4/64/256 blocks) | 83 200 values each |
| IQ1_S F16 / BF16, 4 cases | 83 200 values each |
| total values compared | 332 800 |
| failures / launch failures | 0 / 0 |

## Mutations

| mutation | failures |
|---|---:|
| IQ3_S grid1 qh shift `8-2*il` -> `6-2*il` (F16 body) | 4 |
| IQ3_S scale shift `4*(ib%2)` -> `2*(ib%2)` (F16 body) | 4 |
| IQ1_S delta bit `0x8000` -> `0x4000` (F16 body) | 4 |
| IQ1_S grid index `3*il` -> `2*il` (F16 body) | 4 |
| BF16 store uses the half conversion (IQ1_S body) | 4 |

No defect was found on the first build.

## Not integrated

Same rule as EXP245–270: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

F16/BF16 for IQ1_M and MXFP4; the activation quantizer and MMQ stay blocked on
the EXP180 emitter address typing.
