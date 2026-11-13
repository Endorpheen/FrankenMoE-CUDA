# EXP-2026-09-08-044 — checkpoint tail of 32 tokens

Date: 2026-09-08. Status: `REJECTED` (`kind=offline-design`, refusal to implement;
not a measured performance rejection). Closed by Igor's explicit decision of 2026-09-08.

## Hypothesis and expected benefit

Increase the final checkpoint batch from 4 to 32 tokens, keeping the checkpoint
mechanism, so that MUL_MAT_ID passes the existing CUDA offload gate. The benefit is
a shorter prefill and a shorter wait for the first response. The usefulness benchmark:
≥3% prefill without a restore regression. The baseline is the accepted EXP-041; the
diagnostic durations are taken from EXP-043 and are not an unprofiled A/B.

## Verification without implementation

- In the EXP-041 sources `tools/server/server-context.cpp`, the array
  `checkpoint_offsets = {4 + n_ubatch, 4}` defines the final split.
  For the studied request, changing the tail gives 363+32 instead of 391+4.
- In `ggml/src/ggml-cuda/ggml-cuda.cu`, `ggml_backend_cuda_device_offload_op`
  compares the batch size with `op_offload_min_batch_size`; 32 reaches the threshold
  of 32.
- Moving the checkpoint position is mechanically possible but is not a verified
  implementation: the consequences for prefix reuse, restore, short prompts, and
  ubatch boundaries were not tested. After a restore, reprocessing of a larger suffix
  may be required. The checkpoint is not disabled.
- EXP-043 attributed about 129.5 ms to the last CPU batch of 4 tokens,
  and 16.3 ms (112.592 MiB) to the checkpoint save. Shifting the position by itself
  does not eliminate the checkpoint save. The 129.5 ms is the cost of the old section,
  not a guaranteed gain: the new tail also has to be computed.

## The cost estimate and its limitations

Per the local agent's offline report, passed on by Igor: 48 MoE layers, 512 experts,
top-10; the combined size of one expert's projections is roughly 1.887 MiB with the
caveat about quant-tier differences (exact sizes are in EXP-042). The proposed model
`unique(n) ≈ 10*n^0.536` estimates 60–70 unique experts per layer for
32 tokens, i.e. several GiB of extra tail staging. This is an
extrapolation, not a measurement of the new split's routing.

Example scenario: 32 unique experts per layer give
`48*32*1.887 MiB ≈ 2.83 GiB`; at the 16498 MiB/s bandwidth measured in EXP-043,
transferring that volume takes about 176 ms. That is already comparable
to the 129.5 ms of the old CPU tail, even before accounting for gather and computation.

However, 32 experts per layer are NOT a lower bound: the tokens may repeatedly
select one set of 10 experts. The unique(32) distribution cannot be derived
from a single aggregate for 391 tokens. Nor can all of the tail staging be counted
as a pure addition: the first batch shrinks from 391 to 363 tokens, and its selected
sets and transfers will also change. Gather and H2D can overlap.

Therefore the claims "the cost is certainly 2–4 times higher than the gain" and "the
calculation unambiguously proves a slowdown" are unconfirmed. The scenario shows the
risk of expensive repeated staging, sufficient for the decision not to spend resources
on the candidate now. There is no measured speedup budget.

## Decision

`REJECTED` as an implementation candidate: Igor approved closing it per the
senior helper's recommendation after reviewing the local agent's report. A positive
benefit budget is not justified, and repeated staging carries a substantial risk. The
code, build, server, model, profiler, and A/B were not launched; the runtime and memory
are unchanged. New raw artifacts, tests, and a separate benchmark JSON are not required.

Lowering `GGML_OP_OFFLOAD_MIN_BATCH` to 4 was not done and is not an
equivalent check: the offload also changes for other eligible operations,
including possible decode/verify shapes. Its result and the reason for choosing the
upstream threshold are not established by this analysis.

Moving the checkpoint to after the whole prompt is considered separately:
`experiments/checkpoint-post-prefill-decision-2026-09-08.md`.
For that direction Igor also decided to keep the current checkpoints.
Neither of the variants requires another run to close.
The next direction is not assigned.

## Existing sources

- `experiments/EXP-2026-09-07-043-prefill-bottleneck-profile.md` — tail attribution.
- `results/archive/EXP-2026-09-07-043/` — the saved trace and analysis.
- `benchmarks/exp042-limited-gap-h2d-coalescing-offline.json` — projection sizes.
- Sources `work/llama.cpp-exp041/` — reading the checkpoint/offload code without edits.
- The local agent's offline report, passed by Igor on 2026-09-08 — a scenario-based estimate of the new selected sets.
