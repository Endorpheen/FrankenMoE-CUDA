# EXP-2026-10-31-117: routing-aware GPU MoE launch bounds

Status: **REJECTED for promotion** after the final corrected v4 early-stop screen.
Reference: EXP107, project d73f7d59bd0b50a4f0310759be25af61961cbfd1.

## Hypothesis recorded before implementation

The current MMQ grid uses the entire batch as each expert's possible column
count. The CPU copy callback already has exact routing IDs. An exact occupancy
maximum can remove empty tiles without extra transfers/GPU storage, while
preserving arithmetic and the accepted EXP107 pipeline. Target: practical2%
real cold-prefill gain with unchanged VRAM, numerical/resource gates and no
material decode loss. Only supported qwen35moe/CUDA sm89 inference is eligible;
other modes retain the old path. Candidates remain isolated from accepted builds.

## Findings and finalization

The new EXP107 trace attributes about2.1565s to936 large expert MMQ launches,
with grids of32768/131072 blocks. The v1 diagnostic confirms route maxima
655..3434 (median1694) and actual grid reduction. GPU-time sums are not assumed
to be removable request latency, especially under H2D overlap.

V2's resource-qualified five pairs show+2.3117% median prefill and exact1280
IDs/top5/probabilities. Three initially contaminated pairs were replaced in
full; their original+2.884% median is not accepted evidence. A subsequent
fixed-UID routing-change test exposes a stale captured-grid defect in v2,
so that version was never promoted despite its32K output equality.

The corrected implementation invalidates graph identity when geometry changes.
Final v4 obtains a proven J=128 granularity from the CUDA backend, rounds the
conservative bound to that width, preserves the UID within a tile bucket, and
keeps whole-K reduction/fixup ordering. Unsupported/small/ambiguous cases retain
the original path. Q4_K/Q6_K capture/replay and same-bucket reuse are exact;
the unsafe negative control fails at routing growth; Q6_K memcheck has0 errors.

## Final predefined screen

Control point fixed before execution: median of unchanged EXP107 A1v3/A2v3,
13024.3755ms. Final v4 cold32768/up to128 outputs:13019.648ms, observed+0.03631%
throughput, effectively a tie. All128 selected IDs/ordered top5/probabilities
match exactly (delta0); complete request-window process swap, system swap-out
and major faults0. Sampled peak/retained VRAM3348/2928MiB, unchanged.

The2% continuation condition fails. Stop without a final-v4 acceptance series,
compatibility expansion or baseline promotion. Earlier versions' percentages
are not transferred to this final build. Preserve the corrected prototype and
its regression tests; do not present this as an accepted speed improvement.

## Artifacts and state

- [Full report, conditions and limits](../results/exp117-expert-bound/README.md).
- Replayable final patch: patches/moe-launch-bound-experimental.patch.
- Final and superseded manifests, commands, numerical/resource evidence and
  standalone graph regression: results/exp117-expert-bound/.
- Local source/build: /home/end0/AI/LlamaServer/exp117-expert-bound.
- Accepted EXP107 restored on8081, health OK, idle262144-token slot, all nine
  mapped runtime hashes verified. Installed launchers and previous models remain
  unchanged; no test server remains.

Reopen only with a changed mechanism/workload and a new bounded hypothesis.
A per-expert compact GPU work schedule is a distinct future direction, not an
implemented or measured result of this experiment.
