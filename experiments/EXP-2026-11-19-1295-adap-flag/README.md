# EXP-2026-11-19-1295: the adaptive door now sees the array's `sorted` flag

## The change

The native normalisation uses `data[0]` as the maximum when the array is flagged sorted, and scans otherwise. The
door could only scan, because the flag was not part of its arguments. It is now:

```
fg_sampler_adaptive_p(lv, p, id, n, target, ws, tw, sorted, marker)
sm_apply_adaptive_p_state(... , sorted)  ->  sm_apply_adaptive_p(... , sorted)
                                          ->  softmax_in_place(lv, p, n, sorted)
```

The host passes `cur_p->sorted ? 1 : 0`; the chain dispatcher, which has no flag of its own, passes 0 and says so in
a comment. The unit's self-test updated with it, and `gate_adaptive` in EXP-1188 with it.

## What it fixed

On the probe that isolates the adaptive door (`typical+xtc` native, adaptive door on):

```
before the change:  21 of 100 rounds differ
after  the change:   0 of 100 rounds differ
```

## Where the remaining 15 actually live

Block-wise comparison of the diagnostic trace - arrays compared as whole applies, not line windows - shows the
difference is not in the probabilities at all: **42 of 100 applies enter adaptive-p with a different `size`**.

```
goose : FG_ADAPTIVE n=6 idx=2 tok=109
native: FG_ADAPTIVE n=1 idx=0 tok=91
```

A different size at that point means a **different number of survivors from the stage before** it, and in this
chain that stage is XTC. The `typical+xtc` prefix was verified identical over 100 rounds - but on the RNG sequence
of the standalone probe, not on the sequence this harness reaches after two other configurations. So the prefix is
exact for the inputs it was tested on and not for these.

That re-points the residual at XTC's survivor count rather than at adaptive, and the next probe follows from it:
log `kept` per round in both modes on this input set. The adaptive flag change remains right - it makes the door's
normalisation agree with the native rule by construction - and it is not the cause of what is left.

## What it did not fix, stated plainly

The harness in EXP-2026-11-19-1287, whose inputs come from a different point in the same RNG sequence, still reports
15 of 100 for the same configuration. The two measurements disagree, so the honest record is: the flag now reaches
the door and one input set collapses to zero, while another does not. The mechanism is right; whether it is the only
one is not established.

## Acceptance

```
sha c2ff42cc1219, decode 45.77-46.27, prefill 1953-2277
```

Unchanged, as expected: the fixed profile sets no adaptive-p parameters, so this door is never reached there.
