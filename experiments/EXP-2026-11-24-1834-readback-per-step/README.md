# EXP-2026-11-24-1834 — The barrier is one readback per graph step, and it is architectural

Status: **MEASUREMENT, candidate rejected.** Baseline `37ac0f1d` unchanged
(prefill 3122.4, decode 78.225). No code change adopted.

## The readbacks, split by source

Every `cuda.download` call site was tagged (`GOOSE_EXP_TRACE=1`,
`runs/actb-080235`, three requests):

```
Goose dtoh site activation_download_record      63936 calls    1.653 GB   12.877 s
```

All of them, 100 %, come from `activation_download_record`. `read_state`, the
state key/value pair path and `activation_publish` produce none during a prefill.
That is 21 312 readbacks per request, 25.8 KB each, 4.3 s per request.

## The pinned-staging hypothesis, now actually tested

The previous experiment's gate was broken: it required `cuda.event_sync`, which
this profile loads only for `GOOSE_CUDA_EXPERT_STAGE_PAGEABLE`, so the bounce never
ran and its numbers were plain controls. The gate now loads its own symbols and
defers event creation to the first copy (the earlier attempt created the event
before the CUDA context existed, which also failed). With it genuinely active:

```
Goose CUDA: dtoh bounce active (pinned staging + event)
Goose dtoh bounce: 63936 copies 1.653 GB, transfer 12.877 s, host memcpy 0.129 s, fallbacks 0
```

| arm | prefill tok/s | decode tok/s | text | tokens |
| --- | ---: | ---: | --- | ---: |
| pinned bounce, warm-up + two measured | 3145.5 / 3126.3 | 78.525 / 77.614 | `f8664331bd30` | 512 |
| unpinned control (same build, gate off) | 3110.1 / 3110.3 | 78.052 / 77.739 | `f8664331bd30` | 512 |
| blocked path, earlier block | 3117.8 / 3124.4 | 77.183 / 77.681 | `f8664331bd30` | 512 |

VmHWM 21 494 972 kB, swap 0, VRAM peak 10 681 MiB, all arms bit-exact. The
difference is inside the block's noise (±0.6 %), so pinned staging is neutral:
**the cost is the wait for the device, not the transport**, now established by a
positive control rather than by an untested claim.

## Why coalescing cannot fix it

The obvious next step was to merge the per-range copies into one covering span per
graph step, and it was implemented and measured (`runs/actb-080235` is that build):
the readback count did not move, because a graph step records **one** range
(`activations.count == 1`), so there is nothing adjacent to merge. The prefill is
therefore 21 312 sequential steps of "launch a couple of kernels, wait for the
device, read one tensor back", and the stages cannot be merged without changing how
many tensors the host asks for.

## What that means for the target

The 3500 target needs 1.13 s. The readbacks cost 4.3 s per request and every
transport-level treatment of them is now measured as neutral (blocking copy, pinned
bounce, span coalescing). The only remaining treatment is to ask for fewer tensors:
keep intermediate activations device-resident when their consumer is a device
operation, so that the host reads hundreds of tensors instead of 21 312.

That is a contract change in the graph executor — which node output needs a host
copy — and it cannot be done as a wrapper. It needs its own experiment with a
differential gate (kernel-level equality, not only end-to-end text), because a
wrong residency decision corrupts results silently.

## Rejected in this experiment

- pinned DtoH staging: 3145.5/3126.3 against 3110.1/3110.3, inside noise, bit-exact;
- span coalescing of the pending ranges: one range per step, nothing to merge;
- (previous experiment) deeper expert prefetch and `GOOSE_SSM_REFERENCE=0`.

## Artefacts

- `runs/actb-080235` — per-site split, bounce active, coalescing build, three
  requests, VRAM and VmHWM.
- `runs/bon3-080045` — bounce finally active, two requests.
- `runs/site3-075802`, `runs/site2-075635`, `runs/site-075529` — the earlier
  attempts whose gate was broken (controls).
- `runs/gate-080009` — the probe that showed `ready 0`, which found the broken gate.
