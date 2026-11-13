# EXP256: MMA-dot accumulation arithmetic -- gated, then deliberately not wired in

The shared MMA dot's only float arithmetic outside `mma()` is:

    sum[...] += dmA.x*dsB.x*C.x[l];
    sum[...] += dmA.y*dsB.y;

## Gate (kept)

    FG256S_GATE cases=20000 failures=0       (host form, bit-exact vs float*float)
    device form emitted                     (fg256d_gg_function_1: acc, acc2)

## Why it is not wired in

`dmA`, `dsB` and `C.x[l]` live in **registers** inside the MMA loop. A Goose module cannot hold registers across
calls, so the values would have to travel through memory -- four extra loads and two stores per accumulated element,
inside the innermost loop of the hot kernel, to replace **two multiply-adds**.

That is not a portability gain; it is a slowdown with a correctness risk and no new coverage: the surrounding
`mma()` and `load_ldmatrix` remain primitives either way, so the decision boundary of this loop does not move.

Recorded as a deliberate stop with the reasoning, not as a silent omission. If this becomes worth doing, it needs a
Goose-side notion of register-resident values first -- otherwise it is the same kind of "mobile memory round-trip"
that EXP410 already rejected once for being slower with no benefit.
