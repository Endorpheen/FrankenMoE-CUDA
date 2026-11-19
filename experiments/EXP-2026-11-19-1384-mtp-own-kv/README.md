# EXP-2026-11-19-1384: MTP on Apex - it runs now, it costs 470 MiB, and it is slower

## First: the arithmetic in the earlier part of this card was wrong

It predicted ~6 GiB for the head's private KV at the accepted context. The measured delta between a control and an MTP
run, same context, same request, is **470 MiB**:

```
vram-ctrl   decode 79.86 tok/s   vram 9722 MiB    rss 20766060 kB
vram-mtp    decode 60.40 tok/s   vram 10192 MiB   rss 21376152 kB
```

So the private KV is about 12 KiB per token, not 23.6, and the arithmetic was off by roughly a factor of two per token
and 13x in the conclusion. It is replaced here by the measurement, which is the only thing that was ever going to settle
it. Extrapolated linearly, the head would need about 3 GiB at 262 144 - still more than fits beside the accepted profile
on a 12 GiB card, but a number rather than a guess.

## Second: MTP works on this machine at all

Before this card the head could not start: its context inherited the target's `n_ubatch 4096` and failed to allocate a
2368 MiB compute buffer. With only the head's `n_ubatch` sized to its work (512), it starts, generates 512 tokens, and
produces a normal answer. That is the first working MTP configuration in this project.

## Third: it is slower, and wider drafts are much slower

Same request, same context, only `--spec-draft-n-max` varied:

| draft width | decode tok/s | vram MiB |
|---|---|---|
| none (control) | **79.86** | 9722 |
| 3 (default) | 60.40 | 10192 |
| 8 | 31.29 | 10809 |
| 16 | 18.68 | 11215 |

Monotonic degradation with width is the signature of **low acceptance**: each extra drafted token costs a verification
pass and is mostly thrown away. A head that were accepting well would improve up to some width and then flatten, not
halve with every doubling.

## What this settles and what it does not

Settled: on Apex with this request, MTP is not a speedup lever, and neither is n-gram drafting (about -10 % at the
accepted profile). The record stands at 62.79 / 62.73 and nothing here challenges it.

Not settled: whether the head's **acceptance** is low because of the mechanism's implementation (the private context, the
verification path) or because this model's nextn head is simply weak on this workload. The measurement that would tell
them apart is the per-position acceptance statistics - `n_accepted_per_pos`, which P6-11 already lists as unfinished -
and that is the next concrete step, not another configuration sweep.

## What was not done, deliberately

No further change to the profile, no attempt to make MTP "fit" by shrinking the target context, cache or batch. The
accepted profile measured itself: 79.86 tok/s at a 40960 context with a short prompt is an exploration, not a record.

## State for P6-11

| item | state |
|---|---|
| synthetic acceptance on the real model | measured, 0 disagreements (EXP-1366) |
| rejection acceptance | measured over 880 combinations (EXP-1368) |
| MTP runs inside the profile | **no** - needs ~3 GiB the card does not have at 262144 |
| MTP as a speedup | **no** at any width tried; degradation is monotonic |
| per-position acceptance statistics | not collected - the next step |
