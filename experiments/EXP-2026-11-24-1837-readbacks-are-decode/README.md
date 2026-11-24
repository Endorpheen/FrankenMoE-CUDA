# EXP-2026-11-24-1837 — The readback chain is a decode cost, not a prefill cost

Status: **MEASUREMENT, correcting EXP-1833..1836.** Baseline `37ac0f1d` unchanged
(prefill 3122.4, decode 78.225). No code change adopted.

## Why the previous attribution was wrong

EXP-1833..1836 measured `cuMemcpyDtoH` over a block of *two requests, each with 512
predicted tokens* and attributed the whole 42 624 calls and 8.8 s to "the request".
With the activation paths now counted separately, that attribution does not hold.

## The separation

Counters on both activation paths (`GOOSE_EXP_TRACE=1`), warm request plus measured
request, same profile:

| request shape | `prepare` | `read_address` | downloads | DtoH time |
| --- | ---: | ---: | ---: | ---: |
| 32768 / 1 token, two requests (`runs/apath2-081701`) | 26 044 | 4 212 | **722** | **1.893 s** |
| 32768 / 512 tokens, two requests (`runs/cpyt-074738`) | — | — | **42 624** | **8.7 s** |

The two `n_predict=1` requests issue **361 readbacks each**. The 512-token block
issues 42 624, so the difference — **41 902 calls — belongs to the 1 024 decode
tokens**, that is **about 41 readbacks per generated token**. `prepare` never
publishes (`publish 0`) and `download_all` is never used, so the reads come from
`read_address` on the per-node path, and the dirty-activation retirement is working:
67 MB retired per graph without a copy.

## What that means for prefill

Prefill's own readback share is **~0.95 s of a 10.5 s request**, not 4.3 s. The
prefill budget is therefore: about 4.2 s of expert traffic at the 21.7 GB/s link
ceiling, about 3.7 s of kernels, about 0.95 s of readbacks, and the rest in the
serialisation between them. None of the earlier measurements change, but their
label does: the readback chain is not where prefill's 8 % went.

## What it means for decode, which has its own target

Decode measures 78.225 tok/s, that is 12.8 ms per token, and 41 readbacks per token
account for most of it: 20 951 reads over a 512-token generation, priced at
0.3-0.6 ms each as the device finishes the token's work. The residency question that
failed as a *prefill* lever in EXP-1835 is a **decode** lever: at one token per step
almost every intermediate tensor is produced and consumed inside the same step, so a
device-resident path would remove the per-token host roundtrips rather than the
per-plan ones.

## Consequence for the two numeric targets

- **Prefill 3500**: the remaining time is expert traffic at the link ceiling plus
  kernel time. Both are device-side; no host lever exists, which is consistent with
  every neutral result since EXP-1825.
- **Decode 100**: the measured lever with the right size is the per-token readback
  chain (41 calls per token). That is where a residency change should be attempted
  first, because the win is per token and the invariants are easier to check on a
  single step.

## Artefacts

- `runs/apath2-081701` — two `n_predict=1` requests: 722 downloads, 1.893 s, and the
  prepare/read_address split.
- `runs/apath-081607` — first path accounting (one cold request: 361 downloads).
- `runs/cpyt-074738` — the 512-token block with 42 624 downloads (re-labelled here).
- `runs/glog-081435` — dirty-activation retirement active (12.5 MB per graph).
