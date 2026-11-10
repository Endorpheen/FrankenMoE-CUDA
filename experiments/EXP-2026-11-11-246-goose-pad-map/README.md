# EXP246: PAD index mapping in Goose — and a language-semantics trap that affects every future port

## The finding first

**Goose's `%` is Euclidean; C's `%` truncates toward zero.** For a negative dividend the two disagree in sign:

| expression | C | Goose |
|---|---:|---:|
| `-5 % 10` | `-5` | `5` |

Transcribing `wrap_around(coord, size) = (coord + size) % size` directly from `pad.cu` therefore diverges
whenever `coord + size < 0`. The first gate run caught it: **30 of 10234 checks**, with the module returning
`+5` where the C reference returns `-5`.

The fix is to emulate the C rule explicitly rather than trust the operator:

    fn cmod(a, m) { if a < 0 { return -((-a) % m); } a % m }     // sign follows the dividend
    fn wrap_around(coord, size) { cmod(coord + size, size) }

This matters beyond PAD. **Every ported expression containing `%` or `/` on a value that can be negative has to
be re-examined**, because both operators differ the same way. Nothing in a byte-comparison gate flags it until
an input actually produces a negative operand, which is why it survived every earlier stage: the eight routes
committed before this one deal in indices that happen to be non-negative. A required follow-up is a sweep of
the existing `*.goose` sources for `%` and `/` on signed values.

## What the kernel does

`pad_f32` (pad.cu:13-84) maps each destination element either to a source element or to zero:

    !circular: inside = (ik >= lpk && ik < nek - rpk) for all k
               inside -> src_idx = i03*s03 + i02*s02 + i01*s01 + i00*s00,  i0k = ik - lpk
               else   -> dst = 0
    circular : always copy, i0k = wrap_around(ik - lpk, nek - lpk - rpk)

## Gate

    10234 checks, 0 failures

5 named cases (inside, further inside, right pad, circular wrap, negative-coordinate wrap) plus random cases
across both modes with independent extents and pads.

The 30-failure run above is the mutation this gate exists for, and it came from the natural transcription, not
from a contrived change.

## Not integrated

PAD does not execute in the fixed 32K/128 Apex profile, same rule as EXP245: implement and gate, do not put an
unreachable route into the shipped DSO. Module `source/pm.goose`, bridge `source/bridge.c`, prefix `fg246p_`.
