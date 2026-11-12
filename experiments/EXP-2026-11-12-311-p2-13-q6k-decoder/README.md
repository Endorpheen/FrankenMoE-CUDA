# EXP311: Q6_K decode in Goose, byte-exact, with the fork's real layout

Second type of the Q4_K_M pair, chosen by the same rule as EXP310: the executed `dequantize_row_q6_K` in `base-quants.o` is
scalar (zero vector instructions), so a faithful port is possible, and Q6_K is on the Apex path.

## The layout is not the upstream one

An assumption cost one iteration here and is recorded so it does not repeat. This fork orders `block_q6_K` as
`ql[128], qh[64], scales[16], d` -- offsets 0, 128, 192, 208 -- verified with `offsetof`, not read from memory. The upstream
order puts `d` first. The first port attempt used the upstream order and failed 7329 of 7424 checks, which is exactly what the
gate is for.

For completeness, `block_q4_K` in this fork *does* match the familiar order (`d` 0, `dmin` 2, `scales` 4, `qs` 16, size 144),
so EXP310 needed no correction.

## Gate

    EXP311_Q6K checks=7424 failures=0

Same shape as EXP310: every output float compared by bit pattern against the real `dequantize_row_q6_K` from `libggml-base`,
sizes 256/512/2560/4096, last super-block zeroed.

Mutation check: adding one to the second group's scale makes 1717 of 7424 checks diverge; the mutation was reverted and the gate
returned to zero failures before this commit.

## Live status

Not wired into the type-trait table, so no live witness is claimed. A preload probe on `dequantize_row_q4_K` recorded zero calls,
but `libggml-base` is linked `-Bsymbolic`, so the probe is blind to internal calls and that result says nothing either way. The
witness has to come from wiring, which is the next step for both decoders.
