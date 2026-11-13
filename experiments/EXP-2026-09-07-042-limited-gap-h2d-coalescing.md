# EXP-2026-09-07-042 — limited-gap H2D coalescing (PRELIMINARY ANALYSIS)

Status: PRELIMINARY_ANALYSIS. Not an executed-experiment, not a performance-claim.
Phase: evaluating a direction from audit A7 (docs/AUDIT-2026-09-06.md:107, "limited-gap
H2D coalescing"). Offline only: the server, model, profiler, build, and runtime sources
were not touched; authoritative sizes are taken from GGUF metadata (offline, without a
launch).
Stage result: the speedup budget is not established, the sign of the effect is unknown;
there is no automatic permission to write the optimization.

## Hypothesis

The scheduler already merges strictly adjacent experts (ggml-backend.cpp copy_experts,
condition `id == last_id + 1`). The idea: allow capturing a small gap with an unselected
expert, so that two H2D transfers merge into one continuous transfer. Unlike the
rejected EXP-033 (packing + device scratch + byte scatter + ID remap), here it is a
direct continuous copy into the previous offsets, no scatter/remap needed. The expected
benefit concerns prefill; the idea promises no decode speedup.

## Novelty

It repeats neither the accepted pinned ring (EXP-038..041, kept the number of copies
calls=chunks=17886) nor the rejected packing/scatter (EXP-033). It differs in a concrete
offline calculation of the number of transfers and additional bytes.

## Two independent threshold candidates (both on equal footing)

These are two different definitions of a "small gap"; neither disproves the other, and
there is no mandatory transition from a byte threshold to an expert count.

1. Candidate A — a fixed byte threshold: merge two transfers if the byte
   gap between them ≤ 512 KiB and the combined span ≤ slot (16 MiB). Result:
   12 316 transfers (−31.14%) at +2 785 MiB (+12.57%), ≈0.50 MiB per eliminated call.
   Implementable directly in the scheduler: compare the byte gap between adjacent ranges.
2. Candidate B — a gap by expert count: merge if there are ≤ N
   skipped experts between the ranges (of any size) and span ≤ slot. At N=1: 9 434 transfers
   (−47.25%) at +5 296 MiB (+23.90%), ≈0.63 MiB per eliminated call.

Candidate A is cheaper in extra bytes but merges less (for down projections the gap of a
single skipped expert = 921 088 bytes > 512 KiB, so A does not touch single skips for down).
Candidate B merges more at the cost of more bytes. The choice of threshold is a separate decision,
not a consequence of the calculation.

A useful fact about the gap: each transfer's size already includes trailing padding of 512 bytes
(`min(expert_size,512)`, except the last expert). The byte gap when skipping exactly
one expert = `expert_size − 512` (gate/up 524 288; down 921 088; blk.2 gate/up 703 488).

## Expert-slice sizes (confirmed from GGUF, not from the log's GCD)

The authority = `nb[2] = n_bytes // ne[2]` from GGUF metadata (`models/qwen38/UD-IQ3_XXS`).
All expert tensors have the same shape (gate/up `2560x640x512`, down `640x2560x512`),
but the quantization type differs — the model is heterogeneous (expert-tier):

| role | ggml_dtype | nb[2] bytes | layers |
|------|-----------:|-----------:|------:|
| down | 20 | 921 600 | 48 |
| gate | 22 | 524 800 | 47 |
| up   | 22 | 524 800 | 47 |
| gate | 21 | 704 000 | 1 (blk.2) |
| up   | 21 | 704 000 | 1 (blk.2) |

The third size (704 000) is the blk.2 layer, whose gate/up lie in a different quant-tier
(ggml_dtype 21 instead of 22) with the shape unchanged; hence the larger expert byte step.
Warning: the GCD of the offsets observed in the log is only an upper-bound estimate of
expert_size; by itself it does not guarantee the exact `nb[2]` (it could also have been a
divisor). Here the GCD coincidentally matched the GGUF across all 144 tensors, but the
calculation relies on the GGUF.

## Safety per the EXP-041 sources (conditional)

Source of truth: `patches/pinned-ring.patch` sha256 e1e6803... (byte-identical to the
accepted EXP-038) + `work/llama.cpp-exp041/ggml/src/ggml-backend.cpp` (1699-1741) and
`ggml-cuda.cu`.

- Destination `input_cpy` is a permanent device weights-tensor, expert by offset
  `id*expert_size`. A skipped expert is written with ITS OWN real host bytes to ITS OWN
  offset. Not compact/scratch, offset identity is intact, no scatter/remap.
- Lifetime (EXP-037 dependency_map): dst is overwritten per split; the last consumer is
  the MUL_MAT_ID of the same split on the same stream; the next overwrite is the next request.
- There is no conflict over the skipped regions: they are not in `used_ids` and are not
  read by this node; overwriting them with correct bytes is harmless; the same stream
  orders the write.
- The ring returns false under graph capture and falls back to set_tensor_async; any
  coalescing must preserve the fallback.

Invariants under which coalescing remains safe (otherwise — not): (1) the same tensor and
the same offsets (offset = id·expert_size), do NOT pack; (2) the same compute stream + the
ring's event discipline; (3) the combined span ≤ slot (16 MiB), otherwise the ring splits
it into several chunks/events and the per-call saving is diluted; (4) `padding_end` for the
combined `last_id`; (5) `last_id < n_expert`. Address coincidences in the log are NOT proof
of safety; the proof is the code invariant + the EXP-037 lifetime derivation.

## Payoff — the sign is NOT established

The calculation yields only two quantities per candidate (the number of eliminated calls
and extra bytes); time cannot be derived from them without measurements on the delivered
EXP-041 runtime.

- The per-call saving = pinned submission + `cudaEventRecord` + ring bookkeeping.
  EXP-037 gave only a PREDICTION about submission (95-380 ms per 19013 calls = 5-20 µs/call).
  This is NOT a measured upper bound of the total saving: events/bookkeeping may be larger
  or smaller; there is no direct measurement.
- The addition = extra CPU-gather memcpy + extra PCIe/H2D + extra writes to the device.
  These CANNOT be added up as a mandatory increment to wall time: the ring covers two
  slots, gather can overlap with GPU consumption, and H2D/gather may have unoccupied
  bandwidth.
- The claim that "submission has left the critical path" is NOT proven by the ring
  speedup: EXP-038 gave +13% with the call count unchanged, but that does not measure the
  residual contribution of a single call after the ring.

There is only one correct conclusion: the sign of the effect is unknown. Neither a
"plus" nor a "minus" can be asserted. The payoff threshold (breakeven) requires the
missing measurements (see below). The old pageable timings (164.2 µs/call) do not carry
over to the new runtime.

## Missing data for the payoff estimate (on the delivered EXP-041)

1. The measured cost of a single call on the pinned ring: `cudaMemcpyAsync` submit +
   `cudaEventRecord` + ring bookkeeping in µs/call (not the EXP-037 prediction).
2. The measured gather bandwidth width (memcpy host->pinned), its share on the critical
   path, and the presence of unoccupied bandwidth, to absorb +12.57%..+23.90% without
   growing wall time.
3. Whether the GPU has H2D idle-headroom in the pinned build (the 1832 ms headroom from
   EXP-037 is PRE-ring, invalid after the ring).
4. The actual distribution of merged ranges on the current scheduler/model, to confirm
   12 316 (A) / 9 434 (B) for the current layout.

## Stage verdict

The speedup budget is not established, the sign of the effect is unknown. The offline
calculation proves a reduction in the NUMBER of transfers, but NOT in execution time. It
is too early to start implementation and a model A/B; items 1-3 (measurements on the
delivered runtime) are required before a decision on a candidate. This document grants
no automatic permission to write the optimization. The direction is not selected as a
priority.

## Artifacts

- `scripts/exp042_limited_gap_calc.py` — reproducible offline calculation (log + GGUF sizes).
- `benchmarks/exp042-limited-gap-h2d-coalescing-offline.json` — the numbers of both candidates and the sizes table.
- Input: `results/archive/EXP-2026-09-06-034/raw/exp032-h2d-ts/h2d.log` (sha256
  3267cbeaeec4f25fd328e88db8b882f73b2b307ad0913d452b6aa56577b55945).
