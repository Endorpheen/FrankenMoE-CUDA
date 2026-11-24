# EXP-2026-11-24-1838 — On the device timeline the expert copies do not overlap the kernels

Status: **MEASUREMENT.** Baseline `37ac0f1d` unchanged (prefill 3122.4,
decode 78.225). No code change adopted yet.

## What was traced

A fresh trace of the prefill request (`bash tools/prefill-profile.sh base32768
checkpoints/cand-prefill-jobpack-split/frankenmoe-infer`), that is the acceptance
prompt of 32 768 tokens with `n_predict` 1, under `nsys`, with the sqlite export
analysed by `tools/prefill-budget.py`.

## The device timeline

Inside the prefill window (first MMQ kernel to the first 32-byte read-back, 6.021 s):

| quantity | measured |
| --- | ---: |
| kernels | 3.717 s (61.7 %) |
| memcpys | 2.232 s (37.1 %) |
| union | 5.712 s (**94.9 %**) |
| device idle | 0.310 s (5.1 %) |
| **overlap between copies and kernels** | **0.237 s** |
| expert-sized HtoD (≥128 KiB) | 50.0 GB in 2.220 s = **22.5 GB/s** |

So the device is not idle and the link is at its ceiling: the problem is that the
two kinds of device work are **almost perfectly serialized**. Out of 2.232 s of
expert transfers, only 0.237 s happens while a kernel runs. The top kernels are the
expected ones — `goose_flash_causal_mma` 482 ms, `goose_gdn_mma_v32` 322 ms,
`goose_mmq_q4_dense_m128n256_pipeline` 320 ms, `goose_mmq_q4_routed_m128` 304 ms.

The host side of the same window is small: `cuLaunchKernel` 11 580 calls / 41 ms,
`cuMemcpyHtoDAsync_v2` 80 547 calls / 184 ms, `cuMemcpyHtoD_v2` 1 859 calls /
4.9 s of blocking (the plan copies, which EXP-1831 showed can be made async without
changing throughput), `cuCtxSynchronize` 26 ms.

## The segment of the required size

Hiding the expert transfers behind the kernels would remove up to **2.2 s inside the
measured window** — the window is one part of the prompt pass, so the full-request
figure is larger (the acceptance request moves 90.7 GB of expert planes, ~4.2 s at
this link speed). That is comfortably above the 1.13 s the 3500 target needs, and it
is the first device-side segment found with that size.

## The change it names

Queue the next expert wave's transfers **before** the current wave's kernels, so the
copy stream runs ahead instead of alternating with compute. The machinery is already
there: a four-slot event ring (`EXPERT_SLOT_EVENTS`), `expert_slot_release()` keeping
"one wave of slack", and `expert_split_planes` letting the third plane transfer while
the gate/up projections run. Measured overlap says that slack is not being used:
0.237 s out of 2.232 s. The change is to deepen it to two waves and to record the
achieved overlap, so the gate can check both correctness and that the counter moves.

## Caveats stated, not assumed away

- The budget window is 6.021 s of a 12 554.8 ms prompt pass, so it characterises a
  part of the prefill rather than all of it; the proportion of copies to kernels is
  what the change targets, and that proportion is measured directly.
- The trace's tail after the request contains about 10 s with no device activity and
  no call in flight, which the tool cannot attribute; it is outside the request
  window and is not used in any number above.

## Artefacts

- `runs/base32768-20261124-082000` — trace, request, response, budget output,
  binary hash, env and argv.
- `tools/prefill-profile.sh`, `tools/prefill-budget.py` — the capture and analysis.
- `runs/fullprof-000121` — the earlier trace, which reproduces the same 0.24 s
  overlap in its own window.
