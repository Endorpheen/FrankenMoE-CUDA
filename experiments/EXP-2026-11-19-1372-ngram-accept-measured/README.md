# EXP-2026-11-19-1372: the ngram-accept door measured - 630 cases, zero disagreements

## What it decides

When an ngram draft is accepted, the native path tracks a low-acceptance streak: a draft whose acceptance fraction is
below 0.25 increments it, a streak of five resets the ngram modulation entirely, and any accept at or above 0.25 clears
it. The door answers the same thing as a pair - the new streak and a reset flag - so the host can apply it.

## The measurement

`fg_sp_ngram_accept` was the second door with no call site anywhere (EXP-2026-11-19-1370). It takes three integers and
two output slots, so it was called directly and compared with the native rule written out from the `accept()` path:

```
ngram-accept door: calls=630 disagreements=0
```

The grid covers: `n_draft_last <= 0` (where the native path does nothing, and the door must pass the streak through
unchanged), the 0.25 boundary from both sides, streak values that cross five, and negative inputs on the way in.

## The one place the port could have differed

The native compares a **double division** against 0.25; the port compares `4 * n_accepted < n_draft_last`. Those are
not the same computation, and the grid is chosen so the difference would show: `n_accepted = 1, n_draft_last = 4` is
exactly 0.25, where the division says "not below" and the multiplication says the same only if written strictly. It is,
and the case is in the grid rather than assumed.

## State of the two unwired doors

| door | evidence | wired |
|---|---|---|
| `fg_sp_compatible` | 180 cases, 0 disagreements (EXP-1371) | no - its route needs a draft model, and the text half is native |
| `fg_sp_ngram_accept` | 630 cases, 0 disagreements | no - the host path uses `n_low` inline |

Neither is dead code by rule 28's standard: both are correct and both correspond to a native decision that a future
draft-model path will need. They are recorded as measured-and-unwired rather than as gaps.

## Reproduction

```
bash experiments/EXP-2026-11-19-1372-ngram-accept-measured/verify.sh
```
