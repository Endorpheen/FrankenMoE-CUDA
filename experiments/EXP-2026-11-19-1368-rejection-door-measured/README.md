# EXP-2026-11-19-1368: the rejection door, measured for the first time

## Why it had no evidence

`sp_reject_accept` is wired into the native route at `common/sampling.cpp` with a cross-check on every draft position,
but that route needs `spec_draft_q` - a draft **distribution** - which only a draft model produces. The ngram drafters
fill none, so on this machine the live comparison cannot run. Grepping the cards for it returns nothing: the port was
written, wired, and never measured.

## What was measured instead

The door is a pure function of three doubles, and the library exports it, so no model and no drafter are needed. The
harness calls `fg_sp_reject_accept` over a grid and compares it with the native rule written out from the source:

```
accept = q_x > 0 && (p_x >= q_x || u < p_x / q_x)
```

Eleven values of p_x and q_x and eight of u, including the edges that decide the rule: `q_x = 0` and `q_x < 0` (the
divide the native code guards), `p_x == q_x` (the short-circuit that avoids it), and infinity and NaN on both sides,
where the order of comparisons, not the arithmetic, decides the answer.

```
rejection door: calls=880 disagreements=0
```

## What this does and does not establish

It establishes the **decision**: given the same three numbers, the port and the native rule agree on 880 combinations,
including every guarded edge. It does not establish that the port is **used** on the live path - that still needs a
draft model, and the requirement is a second GGUF with a compatible vocabulary.

## Reproduction

```
bash experiments/EXP-2026-11-19-1368-rejection-door-measured/verify.sh
```

No model load, two seconds, exit code 0 when the door agrees.
