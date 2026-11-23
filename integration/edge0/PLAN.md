# Edge0 first inference integration (continuing EXP915)

**Historical reference.** This note records an Edge0 experiment from the Flint lineage. See [the model registry](../../docs/goose-migration/MODEL-ARCHITECTURES.md) for current qualification.

Status: IN PROGRESS, no model generation claimed.

## Hypothesis
Preserve the existing qwen4exp 48-layer HC/QSA/recurrent/PLE convolution backbone.
Connect the accepted Goose latent expert and FP8 positioned-row provider to it
through a CPU custom-op execution boundary. Reuse the common Goose router and
shared expert graph semantics. No full weight conversion or new heavy engine.

## Fixed first-run target
Pinned pack dab5ce727d494e2018626fe8896bfdc774215b17, Q4 D/U, Q3_K gate/up,
IQ4_NL down. 512 context, batch/ubatch 1, one sequence, MTP off.
One short Russian completion, normal EOS, no quality/speed claim before it works.
Record loading, TTFT, actual prefill/decode token counts, peak RAM/VRAM and swap.

## Memory plan (ESTIMATE)
Skeleton mmap 5.58 GiB virtual/resident upper bound. Shared/direct dense weights
are in the skeleton, not duplicated. One layer D/U <=28.2 MiB, ten expert
records 11.11 MiB, bounded FFN arena 64 MiB, existing recurrent/attention state
under 512 context <0.3 GiB before graph and allocator overhead. PLE row scratch
2560 bytes and F32 output 10 KiB/token. No mapping/residency of the 47.68 GiB PLE
table or the 26.66 GiB expert pack. Measured peak remains UNKNOWN.
Initial correctness comes before speed. Start CPU-only if needed; CUDA backbone
placement can then be enabled without changing accepted Apex libraries.

## Gates and stop conditions
Compare route IDs/weights, shared contribution and combined latent FFN with an
independent native GGML graph on real layer weights. Verify PLE history and
chunked/step input equivalence and direct decoded rows. Fail closed on read,
shape, invalid scale/group, nonfinite data or context capacity errors.
Build all required integration once for actual ABI/link risks; retain passed
component receipts. Then run one owned CLI with 15-minute wall limit, 12 GiB
process memory ceiling and no process swap growth accepted. No second heavy
model or unrelated process termination. Accepted Apex is immutable.

Temporary native glue is debt, not completion of the Goose-only runtime.
No C++ removal or unrelated refactoring is authorized here.

## Outcome
First basic Russian answer reached, not general quality/performance acceptance.
See experiments/EXP-2026-11-17-916-edge0-first-inference/README.md for all results
and retained negative observations. Long-context, chunked PLE equivalence and
server acceptance remain unproved. Next work is measured decode profiling,
not further Apex tests.
