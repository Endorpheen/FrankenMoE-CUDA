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
