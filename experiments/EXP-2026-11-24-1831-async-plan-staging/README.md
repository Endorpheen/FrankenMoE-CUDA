# EXP-2026-11-24-1831 — Asynchronous plan staging: mechanism proven, speed neutral

Status: **REJECTED as a gain, ACCEPTED as evidence.** Baseline `37ac0f1d`
unchanged (prefill 3122.4, decode 78.225). Candidate preserved, not adopted:
`checkpoints/exp-plan-async/frankenmoe-infer`.

## Why this change

EXP-1828 measured the plan copy as **5.954 s of queue wait against 0.011 s of
copy**, and concluded the request alternates "6.4 s of host work with an idle GPU,
then 6.0 s of GPU work with a spinning host". The obvious lever was to stop the
host from blocking: stage the plan in pinned memory and hand the copy to the same
legacy stream instead of calling a synchronous `cuMemcpyHtoD` per projection.

## The exact defects found on the way (all with witnesses)

| # | defect | witness |
| --- | --- | --- |
| 1 | two device slots in one buffer, slot stride not matched to the reservation | 5319 of 5320 copies fell back (`runs/pipe-070820`) |
| 2 | `launch_mmq_stream` and its fixup read the buffer base instead of the slot | stream-workers always used slot 0 |
| 3 | one shared pinned staging area for both slots: DMA still reads its source after the call returns | two staging areas per slot |
| 4 | `cuda.packet_host` (one shared 1 MiB pinned area in `upload_packet`) is unfenced once the host runs ahead | sanitizer fault 912 337 bytes past the 1 MiB allocation, whose allocation site resolved to `upload_packet`; gating it made the request pass (`runs/pipe8-072726`) |
| 5 | with the host running ahead, small decode batches became non-deterministic | text hash differed per request (`runs/pacc-072802`, decode 59.5) |

Defect 4 was named by matching the sanitizer's "nearest allocation" address
against every allocation site in the same run (`GOOSE_ALLOC_TRACE=1`, caller
return address resolved with `addr2line` on a `-no-pie` diagnostic build);
`runs/al-072518` is the matching run.

## The design that ships in the candidate

The device buffer stays **single** and the copy stays on the **legacy stream**;
only the copy is asynchronous, from a 4-slot pinned staging ring whose slots are
released after the launches that read them. Every ordering assumption the rest of
the engine relies on is therefore unchanged, which is why this variant is
bit-exact where the two-slot pipeline was not. `GOOSE_PLAN_ASYNC=0` restores the
old synchronising copy, `GOOSE_PLAN_PIPE_MIN` (default 256 jobs) keeps small
decode projections on the old path.

## The result

The mechanism engaged and did what it promised:

```
Goose plan async: 7920 copies, host 0.019 s, slot wait 0.003 s, fallbacks 0
```

7 920 plan copies staged in **19 ms** of host time with **zero** fallbacks, against
the 4.9 s of blocking copies in the baseline. Paired block, alternating arms,
warm-up plus measured request in each arm, same profile:

| arm | prefill tok/s | decode tok/s | tokens | text | VRAM peak |
| --- | ---: | ---: | ---: | --- | ---: |
| base `37ac0f1d` | 3124.4 | 77.681 | 512 | `f8664331bd30` | — |
| cand `44ab9d9e…` | 3132.2 | 78.282 | 512 | `f8664331bd30` | 10 681 MiB |
| base `37ac0f1d` | 3117.8 | 77.183 | 512 | `f8664331bd30` | — |
| cand `44ab9d9e…` | 3108.2 | 78.162 | 512 | `f8664331bd30` | — |

Medians: base **3121.1**, candidate **3120.2**, that is **−0.03 %** — neutral.
Decode parity holds (77.18-77.68 base against 78.16-78.28 candidate), all four
runs are bit-exact, VmHWM 21 497 916 kB against 21 491 288 kB baseline (+6.6 MB,
0.03 %), swap 0.

## What this proves, and what it corrects

1. **The 5.954 s was the host idling, not removable host work.** Removing the
   blocking entirely — the copy is now 19 ms and never waits — changes the request
   time by nothing. The EXP-1828 budget ("6.4 s host / 6.0 s GPU, zero overlap")
   was an artifact of the probe: draining the queue before the copy measures the
   wait, but that wait is not work that can be reclaimed.
2. **The plan-copy and host-overlap direction is now closed by a positive
   control**, not by another failed attempt: the mechanism is implemented, engaged,
   and measured, and the speed does not move. Together with EXP-1825, 1826, 1827
   every host-side lever on this path is measured.
3. **The remaining 8 % is not in the host's copies.** The request time is set by
   what the device and the expert path do; the next axis is the expert path
   (`--cpu-moe` computes experts on the host) and kernel efficiency, not transfer
   submission.

## Consequence

Prefill stays **3122.4 tok/s**, 8 % short of 3500, with the baseline untouched.
The candidate is not adopted: it is neutral, and adopting a neutral change into the
accepted baseline is forbidden by the profile rules.

## Artefacts

- `checkpoints/exp-plan-async/` — candidate binary and source, `SHA256SUMS`,
  explicitly **not** an accepted reference.
- `runs/pair-073330` — the four-arm paired block (base, cand, base, cand).
- `runs/pa-073200` — first run of the candidate with the async-staging counter,
  VRAM before/after, VmHWM, swap, props.
- `runs/pacc2-072938` — prefill-scoped variant still non-deterministic (defect 5).
- `runs/pipe8-072726` — packet staging fenced, request passes (defect 4).
- `runs/al-072518` — allocation trace that named `upload_packet`.
- `runs/san-071650`, `runs/san2-072109` — sanitizer faults.
