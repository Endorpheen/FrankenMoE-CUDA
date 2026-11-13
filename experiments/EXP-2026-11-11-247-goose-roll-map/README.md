# EXP247: ROLL index mapping in Goose — gated candidate, not integrated. P3-03 closed.

`roll_f32_cuda` (`roll.cu:5-45`), the last P3-03 item without a Goose implementation.

## What it preserves

    i0 = idx % ne00;  i1 = (idx / ne00) % ne01;  i2 = (idx / (ne00*ne01)) % ne02;  ...
    dk = wrap_index(ik - sk, nek)
    wrap_index(v, ne) = v < 0 ? v + ne : (v >= ne ? v - ne : v)

Two things stated rather than assumed:

1. **The `%` and `/` here are on provably non-negative operands** — `idx` is a thread index and the divisors are
   extents. Goose's Euclidean modulo and C's truncating one agree for non-negative operands, so this module is
   clean under the rule in `MODULO_SEMANTICS.md`. Checked against the source, not hoped for.
2. **`wrap_index` corrects by a single extent.** It is not a general modulo: it is only correct while
   `|sk| < nek`. The guard asserts that bound, so a caller violating it fails loudly instead of silently
   producing an index outside the tensor — which is what a "simplifying" rewrite to `%` would do, by accepting
   every shift and reading the wrong element.

## Gate

    45277 checks, 0 failures

6 named cases (identity, positive shift, negative shift, maximum legal shift `+3`, minimum legal shift `-3`, and
1×1×1×1) plus 2000 random geometries with **every index of every geometry** verified, not a sample.

| mutation | failures |
|---|---:|
| single-extent correction doubled (`v + ne + ne`) | 22892 |
| `i1` divisor uses `ne02` instead of `ne01` | 20864 |
| shift sign flipped (`i1 + s1` instead of `i1 - s1`) | 22494 |

## Not integrated

ROLL does not execute in the fixed 32K/128 Apex profile — recorded in the plan and the census. Same rule as
EXP245 and EXP246: implementing and gating an unreachable kernel is a deliverable; putting it into the shipped
DSO is a liability, because the witness that proves a route works is exactly the thing that reads zero when it
does not. Module `source/rm.goose`, bridge `source/bridge.c`, prefix `fg247r_`.

## P3-03 is now closed

CONCAT (EXP175), GET_ROWS (EXP176), CPY (EXP177) are live and integrated. SET (EXP245), PAD (EXP246) and ROLL
(EXP247) are implemented and gated but deliberately not shipped. Every item in the task has a Goose
implementation; three of them are waiting for a profile that reaches them.
