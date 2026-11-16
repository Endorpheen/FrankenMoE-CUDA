# EXP-2026-10-30-102: coalescing H2D expert copies through small gaps

Status: REJECTED before implementation (`kind=geometry-gate`). The pre-stated
5% extra-traffic cap cannot be met by this workload; no code was built.

## Hypothesis

`sched_copy_experts` copies runs of consecutive selected experts. Merging runs
across a small number of unused experts would reduce dispatch calls for the
pinned-mmap source, as long as the extra bytes stay small.

## Measured geometry

- Model metadata (server log of the accepted runtime): `n_expert = 256`,
  `n_expert_used = 8`, 40 MoE layers, three expert tensors per layer.
- `llama_context::sched_copy_experts()` (exp090 source, lines 2840-2915)
  already merges consecutive used experts into one `ggml_backend_tensor_set_async`
  per run and adds up to 512 B of padding for MMQ. The ids tensor is read once
  and cached per graph; those mechanisms are not duplicated here.
- With 8 selected experts of 256, the selected bytes per tensor are
  `8 * expert_size`. Merging across one skipped expert adds `expert_size`,
  i.e. 12.5% of the selected bytes; two gaps add 25%. The pre-stated cap is 5%,
  so not even a single full-expert gap is allowed.
- Merging the whole min..max span instead would copy, on average, about 28x the
  selected bytes for uniform routing, far beyond any bandwidth budget.

## Verdict

REJECTED. For the current Apex routing geometry the mechanism has no admissible
gap: adjacent experts are already merged, and every remaining gap costs at least
12.5% extra traffic. The old EXP043 negative result concerned a staging/gather
path and is not used here; this rejection is from the measured expert count and
the current copy code. No performance run was made because the traffic gate is
violated by construction.

## Reopen condition

A different model with a much larger `n_expert_used / n_expert` ratio (so a gap
costs <=5% of a run), or a measured copy-dispatch overhead that dominates even
without merging, would require a new hypothesis.
