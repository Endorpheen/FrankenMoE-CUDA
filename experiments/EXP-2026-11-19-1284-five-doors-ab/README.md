# EXP-2026-11-19-1284: five sampler doors through the public API - four identical, one open

```
penalties: identical
typical:   identical
mirostat:  identical
adaptive:  identical
xtc:       OPEN, 60 lines differ (membership, same size)
```

Each sampler is constructed with the public init, driven with `llama_sampler_apply`, and the resulting array is
compared between the door-enabled and door-disabled runs - values, order and size. The whole input array is
**sorted by logit and marked `sorted`**, which is what the default chain provides because top-k runs before these
samplers; the first version of this harness fed an unsorted array and produced differences that were an artefact
of that, not of the ports.

`penalties` carries accepted history through `llama_sampler_accept`, and `mirostat`/`adaptive` are applied four
times in a row because their state advances across draws - a single application would compare only the first step.

## XTC: the open one, stated precisely

Round 0 is representative: both paths return **28** candidates, but not the same 28. The native keeps the two most
probable tokens (`358` and `140`); the door drops them and keeps two from the tail. XTC's design **excludes** the
likely tokens above a threshold, so dropping them is what the sampler is for - which means the question is why the
native path keeps them.

That needs both implementations read side by side, and it is not a question to answer by choosing the side that
looks more like a port. 28 of the 60 rounds differ.

## Why this matters beyond XTC

The accepted XTC evidence (720/720) compared the port with a **restatement of the native formula inside the
harness**. DRY had the same shape of evidence and turned out to hide a real defect (EXP-2026-11-19-1281). The
lesson generalises: a harness that reimplements the reference cannot see a mistake shared by the port and the
harness, and only the shipped code driven by both switches can.

XTC is not on the acceptance path (the fixed profile sets no XTC parameters), so this is dormant there - and
recorded, not deferred silently.
