# EXP268: CUDA IQ4_NL and IQ4_XS dequantization to half and bfloat16 in Goose

`dequantize_iq4_nl<dst_t>` and `dequantize_iq4_xs<dst_t>` for `half` and
`nv_bfloat16`. EXP252/EXP253 covered the F32 destinations; this extends both to
the two non-F32 destinations in one module with four entries.

## Verdict

**OFF_PATH** — bit-exact against all four native instantiations; the fixed
32K/128 Apex profile never selects these types through the F16/BF16
convertors. No performance claim, shipped DSO unchanged.

## Scope transferred

- IQ4_NL: the row wrapper's nb32 guard index, source block `i*8+ib`, base
  `i*QK_K + 32*ib + 4*il`, nibble split and `d*kvalues_iq4nl[q]`.
- IQ4_XS: the per-32 scale from `scales_l`/`scales_h`, `d = x.d*(scale-32)`,
  base with `16*ib + 4*il` in `qs`, nibble split and the table.
- The 16-entry `kvalues_iq4nl` table is an explicit Goose constant function.
- Only the store differs per destination (`__float2half` / `__float2bfloat16`).

Layouts: `block_iq4_nl` 18 bytes (`d` 0, `qs[16]` 2); `block_iq4_xs` 136 bytes
(`d` 0, `scales_h` u16 2, `scales_l[4]` 4, `qs[128]` 8).

## Gate

| check | result |
|---|---|
| IQ4_NL F16 / BF16, 5 cases (8/32/512/2048 blocks and a 7-block tail) | 83 424 values each |
| IQ4_XS F16 / BF16, 4 cases (1/4/64/256 blocks) | 83 200 values each |
| total values compared | 333 248 |
| failures / launch failures | 0 / 0 |

## Mutations

| mutation | failures |
|---|---:|
| IQ4_NL destination `4*il` -> `2*il` (F16 body) | 5 |
| IQ4_NL destination uses the guard index `ibg` (F16 body) | 3 |
| IQ4_XS scale bias 32 -> 31 (F16 body) | 4 |
| BF16 store uses the half conversion (NL body) | 5 |
| high nibble reads the low nibble (F16 body) | 5 |

The second mutation is the exact defect EXP252 found in the F32 module
(guard-index vs block-index conflation); the gate still catches it in the
destination variant.

## Not integrated

Same rule as EXP245–267: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

F16/BF16 for IQ2_XXS, IQ2_XS, IQ2_S, IQ3_XXS, IQ3_S, IQ1_S, IQ1_M and MXFP4;
the activation quantizer and MMQ stay blocked on the EXP180 emitter address
typing.
