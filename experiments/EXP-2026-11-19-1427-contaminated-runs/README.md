# EXP-2026-11-19-1427: the numbers fell, and the reason is not my code

## What was observed

Six paired runs of the current build, nothing changed between them:

```
18 measurements: median decode 61.60, min 56.72, max 63.75   (median prefill 2816.7)
```

against 62.85 at checkpoint EXP1412. A 2 % drop with a 7-token spread - wide enough to invite a search for a cause, and
narrow enough to be dismissed as noise by someone in a hurry.

## The first thing I suspected, and what happened to it

`plan()` called `getenv("FG_SLRU_DIAG")` **once per invocation** - 61 441 times per run - which is exactly the kind of
mistake that shows up as a small percentage. Caching it in a static was the fix, and the measurement afterwards was:

```
median decode 60.84   (min 58.21, max 62.46)
```

**Lower**, not higher. The suspicion was wrong, and the change did not make things better - which is what the numbers said,
so they were believed rather than explained away.

## The actual cause, found by looking at the machine

```
13409  igorek-llama-server --embeddings ... --port 8022 ...
nvidia-smi: 2846 MiB used, 33 % utilization, 50 C
load average: 3.70
```

A **foreign service** is using the same GPU. It is not mine to stop - rule 30 forbids touching `igorek-llama-server` - and
its presence means every measurement in this session's last hours shared the device with it.

## What that invalidates, and what it does not

Invalidated: any comparison between the recent runs and the earlier checkpoints, including my own two attempts above. The
spread 56.72-63.75 is the signature of a shared device, not of a code change.

Not invalidated: everything measured **with the hash** in the same run - `c2ff42cc1219` held through all six runs, so
correctness and tokenisation are untouched. Nor the traffic measurements of EXP-1423/1424: those compared **bytes and
hit rates**, which are counters, not timings, and the affine fit came from their ratio.

## What a valid measurement requires, and what I will do

The protocol in the repository already says it: one server at a time, port identified by pid, and no measurement while
something else holds the device. With `igorek-llama-server` running, this machine cannot satisfy that for a GPU benchmark.
So:

- no checkpoint is created from these runs - a checkpoint records numbers, and these numbers describe the sharing, not the
  build;
- the `getenv` caching stays, because it is correct regardless, with the note that it moved nothing measurable;
- when the device is free, three runs will be taken and compared against EXP1412 in the same session.

## The lesson, which is the repository's own rule 29

A verification run that overlaps another proves nothing, and a failed run is not evidence about the code. Here the overlap
was not a leftover of mine but a service I must not stop - which is precisely why the rule says to **check and report**
rather than to explain a number away.
