# EXP-2026-11-24-1841 — One-layer-ahead expert loading: corrected arithmetic

Status: **CORRECTED, then re-scoped.** The original version of this card rejected the
lever on a wrong per-layer figure; the model parse has been redone with physical
quantised sizes and the conclusion changes from "impossible" to "possible with a
residency trade". Baseline `37ac0f1d` unchanged (prefill 3122.4, decode 78.225).

## Correction (read this first)

The first version of this card stated "a layer is 4096 MiB, the headroom is
2.56 MiB, the change needs a factor of 1600 more VRAM". That was wrong: the parse
used a type table that treated ggml type ids as bytes per element instead of
quantisation block sizes, which inflated the expert tensors to 144 GiB.

Redone with block sizes (`Q4_K` = 144 bytes per 256 elements, `Q6_K` = 210 per 256):

| quantity | corrected value |
| --- | ---: |
| model file | 20.22 GiB |
| sum of physical tensor bytes | 20.21 GiB (matches the file) |
| expert tensors | 18.65 GiB over 41 layers |
| **expert bytes per layer** | **498.0 MiB** (min 432, max 498) |
| cache 6290 MiB | holds **12.6 expert layers** |
| one extra layer | **498 MiB** |
| 13 layers would need | 6474 MiB, i.e. **184 MiB above the 6290 MiB limit** |

So the lever is not excluded by 1600×; it costs about **8 % of the cache's residency**
(12.6 layers become 11.6 when one layer's worth is dedicated to the prefetch buffer),
or 184 MiB of extra VRAM if the budget were raised.

Retracted: the 4096 MiB per layer, the 0.06 % headroom figure and the factor of 1600.
Retained: the engine-side measurement that the cache reports 6287.4 MiB used of
6290.0 MiB with 2.56 MiB free, which is why a reservation can only come from the
residency of the current layers, not from slack.

The rest of this card is the original text, kept for the record; its conclusion is
superseded by the section "Revised conclusion" at the end.

## What the next step needed

Reserve one extra expert layer next to the current one inside the 6290 MiB budget, so
the next layer's planes could be copied while the current layer computes (the 0.237 s
of overlap against 2.232 s of copies from EXP-1838 is the prize).

## The arithmetic, from the shipped file

Parsing the tensor table of `Apex-35B-V1.0-Q4_K_M.gguf` directly:

```
expert tensors total 144.00 GiB over 41 layers
per-layer expert bytes: min 3072.0 MiB, median 4096.0 MiB, max 4096.0 MiB
  blk.ffn_down_exps.weight: 63488.0 MiB
  blk.ffn_gate_exps.weight: 41984.0 MiB
  blk.ffn_up_exps.weight:   41984.0 MiB
```

and from the running engine at the same time (`runs/resv-083116/props.json`):

```
cache used 6287.4 MiB of 6290.0 MiB -> headroom 2.56 MiB
```

So one layer of expert planes is **4096 MiB** while the free space in the cache is
**2.56 MiB**, that is **0.06 %** of what the reservation needs. The cache holds
**1.54 layers** in total, so there is not even room for the current layer twice, let
alone for one layer ahead. The change would need a factor of about **1600** more free
VRAM than exists.

**Correction to EXP-1840:** that card quoted "~333 MB per layer" from an assumed 20 GB
expert set. The shipped file has a 144 GiB expert set over 41 layers, so a layer is
4 GiB. The conclusion of EXP-1840 (prefetching eight experts in prefill cannot cover a
layer) is unchanged and, if anything, stronger.

## What this closes

The device timeline's 2.232 s of expert copies per window cannot be overlapped by
reserving ahead, and the reason is now a property of the model against the card:
a 144 GiB expert set streamed through a 6.29 GiB cache. Covering one layer while
computing the previous one requires **8 GiB of resident experts**, which needs a card
with at least 14-16 GiB usable for experts after KV cache, not 6.29 GiB.

Measured over this phase, every code-side lever is now closed with numbers:

| lever | evidence |
| --- | --- |
| copy submission, batching, pinned staging | 6 variants, −3.1 % to +6.2 %, neutral |
| host waits and plan staging | −0.03 %, 3103.7/3098.4 |
| readback residency (prefill) | reads needed, skip fails the gate; 0.95 s of prefill |
| readback residency (decode) | 41 reads per token, 12.8 ms per token — a decode lever |
| plan granularity | the model step is already one plan |
| expert wave slack | 2 depths, 3115.9/3117.5 |
| expert prefetch in prefill | 3115.6/3109.4, +22 % traffic |
| one layer ahead reserved | needs 4096 MiB, has 2.56 MiB |
| cache growth, ubatch, profile | measured worse or out of profile |

## Consequence

Prefill stays **3122.4 tok/s**, 8 % short of 3500. On an RTX 4070 the target is not
reachable by scheduling: it needs a bigger expert cache, which is a hardware
property. The honest close of this phase is either to accept 3122.4 as the ceiling of
this card with the protocol and evidence, or to move to the decode target (100 tok/s),
where the measured lever is the 41 per-token readbacks and the invariants are cheaper
to prove on a single step.

## Artefacts

- `runs/resv-083116` — props with the cache occupancy and headroom.
- `experiments/EXP-2026-11-24-1841-one-layer-arithmetic/` — this card.
- The GGUF tensor table was parsed directly from
  `/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf`; the parse is reproducible
  from the format header and needs no engine hooks.
- `runs/base32768-20261124-082000` — the device timeline (0.237 s overlap).

## Revised conclusion

With 498 MiB per expert layer and a 6290 MiB cache holding 12.6 layers, a
one-layer-ahead reservation is **possible inside the same budget** by dedicating one
layer's worth of residency to the prefetch buffer: residency falls from 12.6 to 11.6
layers, which costs roughly 8 % of the re-use capacity, while the copies that are
currently serialised (2.232 s in the EXP-1838 window) could be issued during the
previous layer's kernels.

Whether that trade is positive is a measurement, not an argument: the expected cost
is a slightly higher miss traffic (a few GB at 22.5 GB/s, about 0.2-0.3 s), the
expected gain is up to the 2.232 s of currently un-overlapped transfer. The transfer
engine-s measured volume (90.7 GB per request) and the 22.5 GB/s link figure are
independent of this parse and stand unchanged.

The next experiment is therefore: reserve one layer of the expert cache for the next
layer's planes, issue the copies one layer ahead, verify bit-exactness and measure
both the overlap (trace) and the request time.
