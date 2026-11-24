# EXP-2026-11-24-1835 — The readbacks are genuinely needed: the residency lever is closed

Status: **MEASUREMENT, candidate rejected by its own gate.** Baseline `37ac0f1d`
unchanged (prefill 3122.4, decode 78.225). No code change adopted.

## Who asks for the readbacks

`main.o` in the shipped library references both
`fg_standalone_graph_compute_output` and `fg_standalone_graph_plan_compute`, so a
prefill is executed as **plans of a few nodes**, and every plan ends in
`fg_cuda_activation_graph_end(..., all=false, ...)`, which reads the plan's output
to the host and then, on the device side, that is the same
`activation_download_record` measured in EXP-1834.

## The experiment

One reversible switch (`GOOSE_ACT_SKIP_READBACK=1`) makes the plan-end read return
success without copying, so the question "is this tensor consumed on the host at
all?" is answered by the bit-exactness gate instead of by inspection.

Result (`runs/skiprb-080745`, warm-up plus two measured requests):

| arm | prefill tok/s | decode tok/s | text | tokens |
| --- | ---: | ---: | --- | ---: |
| readback skipped | 3035.8 / 3024.0 | 84.661 / 85.157 | `7229bb112f3e`, `5d00e831a10f` | 512 |
| baseline protocol | 3122.4 | 78.225 | `f8664331bd30` | 512 |

**The gate fails: the output changes.** The host consumes those tensors; they are
not dead copies. VmHWM 21 491 584 kB, swap 0, VRAM unchanged.

The speed numbers of the skip arm are meaningless on purpose: with wrong data the
model routes different experts, so it does less work (decode reads 85 tok/s for the
same reason). That is exactly why the gate runs before any number is quoted.

## The part that is still worth knowing

Skipping the *plan output* removed 1.5 GB of traffic but left **62 400 readbacks of
0.128 GB** that still cost **12.286 s** over the three requests — about 2 KB each
and still roughly 200 µs per call. Those are the small mid-graph reads (route ids
and similar), and their cost is the same queue wait, not transfer.

So the barrier is not one large copy that could be batched: it is tens of thousands
of 2 KB tensors the host genuinely reads, each of which drains the device queue
before it can return.

## Where that leaves the target

Every lever named by measurement is now closed:

| lever | measured outcome |
| --- | --- |
| copy submission (5 variants, batch DMA) | −2.95 % to +6.2 %, +0.13 % |
| pinned plan source / pinned DtoH staging / span coalescing | −3.1 %, neutral, nothing to merge |
| host waits (async plan staging, deferred matrix waits) | −0.03 %, 3103.7/3098.4 |
| expert prefetch depth | already k=8, 0.36 s of host time, 0 discarded waves |
| expert cache growth / policy | worse (bank split `1904/1531`), Belady headroom 2.3 % |
| skipping host readbacks | wrong output, gate fails |
| ubatch 8192 | 641 tok/s, four times slower |
| profile change (ctx 131072) | +0.9 % for double the traffic, out of profile |

The measured budget of one request is 4.2 s of expert traffic at the 21.7 GB/s link
ceiling, about 3.7 s of kernels, and about 4.3 s of readback barriers. There is no
segment left that any of these levers can remove, and the readback barrier can only
be removed by an executor that keeps tensors device-resident — which contradicts
what the host plan scheduler currently needs, as the failed gate shows.

## Consequence

Prefill stays **3122.4 tok/s**, 8 % short of 3500. The target needs one of: more
VRAM so the expert set fits (removing the 4.2 s), a different execution strategy
than per-plan host readback (a redesign with its own gate), or a model whose expert
path does not thrash a 6.3 GB cache. None of these is a tuning change, and I will
not present a neutral candidate as a gain.

## Artefacts

- `runs/skiprb-080745` — the skip experiment with its gate failure, per-site
  readback split, VRAM and VmHWM.
- `runs/actb-080235`, `runs/bon3-080045` — EXP-1834 (bounce active, neutral).
- `runs/actb-080235` also carries the coalescing build's counters.
