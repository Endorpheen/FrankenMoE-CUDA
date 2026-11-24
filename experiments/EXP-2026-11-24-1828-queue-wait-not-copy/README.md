# EXP-2026-11-24-1828 — The 4.9 s of plan copies was GPU queue wait, not host work

Status: **MEASUREMENT, correcting EXP-1824's attribution.** Baseline `37ac0f1d`
unchanged.

## The split

Before each synchronising plan copy the null stream was drained with a spin on
`cuEventQuery` and timed separately from the copy itself
(`GOOSE_SPLIT_TRACE=1`, one 32768-token request):

```
Goose plan split: 2660 copies, queue wait 5.954 s, copy 0.011 s, spins 75056701
```

- **queue wait 5.954 s**: the host is spinning until the GPU has drained the work
  already queued, that is waiting for the device;
- **copy 0.011 s**: the transfer itself, 2660 copies of about 0.4 MB.

So the segment I had called "4.9 s of host work in synchronising copies" is
almost entirely **waiting for the GPU**, which also explains why every attempt to
make it cheaper led nowhere: the bytes are not the cost, the queue is.

## The corrected request budget

32868-token request, 12.4 s wall:

| part | time | relation |
| --- | ---: | --- |
| GPU busy (kernels + memcpys, union) | 5.73 s | measured in EXP-1818's trace |
| host waiting for the GPU inside the copies | 5.95 s | same intervals as the GPU busy time |
| host code while the GPU is idle | ~6.4 s | 12.4 - 5.95 |
| copy time itself | 0.011 s | negligible |

The request is therefore **strictly alternating**: about 6.4 s of host work with
an idle GPU, then about 6.0 s of GPU work with a spinning host. There is no
overlap at all, and the two sum to the wall time.

## What that changes

The lever is not the copy and not its submission: it is the **serialisation
between host planning and GPU execution**. Seventeen hundred words of earlier
analysis said "host-bound"; the corrected statement is "alternating, host phase
6.4 s and GPU phase 6.0 s, zero overlap". Perfect overlap would make the request
6.4 s (about 5100 tok/s); overlapping even a quarter of the GPU phase would close
the 1.13 s the 3500 target needs.

Why the pipeline attempts (EXP-1813) did not achieve it: each projection needs the
previous projection's device result on the host (the router output, read back in
165 DtoH calls), so the host cannot run ahead of the GPU without a deeper
double-buffered plan. That is the next thing to measure, and this card records the
budget it has to work against.

## Artefacts

`runs/split-070555` (the split counters), `runs/upt2-070355` (async copies 0.271 s),
`runs/ust-065953` (user/sys split), `runs/fullprof-000121` (GPU busy union 5.73 s).
