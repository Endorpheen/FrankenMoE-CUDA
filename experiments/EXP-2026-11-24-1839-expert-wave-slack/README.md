# EXP-2026-11-24-1839 — Depth-2 expert waves: measurable, neutral, and why

Status: **REJECTED as a gain (neutral), bit-exact.** Baseline `37ac0f1d` unchanged
(prefill 3122.4, decode 78.225).

## The change

EXP-1838 measured only 0.237 s of overlap against 2.232 s of expert copies, and the
copy stream's own fence waits for an event recorded one wave back
(`expert_slots_ring[index - 2]`, `EXPERT_SLOT_EVENTS = 4`). The change makes that
slack explicit and settable — `GOOSE_EXPERT_WAVE_SLACK` (default 1, which is exactly
the previous behaviour; 2 selects `index - 3`, two waves of slack) — so the copy
stream could run ahead by one more wave without touching the ring size or the release
point arithmetic anywhere else.

## The measurement

Acceptance request, warm-up plus two measured, same profile:

| arm | prefill tok/s | decode tok/s | text | tokens |
| --- | ---: | ---: | --- | ---: |
| slack 2 (`runs/slack-082350`) | 3115.9 / 3117.5 | 78.645 / 78.715 | `f8664331bd30` | 512 |
| baseline protocol | 3122.4 | 78.225 | `f8664331bd30` | 512 |

Bit-exact, decode at parity, VmHWM 21 496 048 kB, swap 0, no CUDA errors. The
difference is inside the block's noise, so the change is neutral: extra slack on that
fence does not create overlap.

## Why the fence was not the limit

The wave that a projection reads cannot be known before that projection's router
output exists, and the router is computed by the projection's own kernels. The order
is therefore structural, not a matter of fence placement:

1. layer k's kernel produces the routing for layer k;
2. only then can the expert planes of layer k be named and copied;
3. the next wave is enqueued when the host plans layer k+1, which happens after those
   kernels have been submitted and, for the routing, after they have run.

Releasing the fence earlier cannot help because the copy that would use the slack has
not been enqueued yet. The prefetch machinery that predicts routes is active
(`k=8`, 19 958 launches, 220 ms of wait for the block) but its copies do not appear as
device-side overlap either — the trace of EXP-1838 counts 0.237 s of it.

## Consequence

- The device timeline is closed as a lever: it is 95 % busy, the link is at
  22.5 GB/s, and the alternating order is set by a data dependency (routes →
   experts) rather than by scheduling policy.
- Overlapping would require predicting the next layer's expert set well enough to copy
  it before its router exists — a speculative-loading scheme with a wrong-prediction
  cost, which is a different project, not a tuning change.
- Prefill stays **3122.4 tok/s**, 8 % short of 3500, with the baseline untouched and
  no candidate adopted.

## Artefacts

- `runs/slack-082350` — the slack-2 arm: three requests, VRAM, VmHWM, swap, props.
- `runs/base32768-20261124-082000` — the trace that measured 0.237 s of overlap.
- `experiments/EXP-2026-11-24-1838-expert-copies-serialized/` — the device timeline.
