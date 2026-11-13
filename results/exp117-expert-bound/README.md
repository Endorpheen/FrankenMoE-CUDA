# EXP117: conservative MoE launch bounds — rejected for promotion

Status: **REJECTED** at the final, predefined early-stop screen. The production
baseline remains EXP107 at project d73f7d59bd0b50a4f0310759be25af61961cbfd1.
No installed launcher, Desktop configuration, model weights or accepted binary
was changed. EXP107 was restored on8081 and all nine mapped runtime hashes match.

## Final result

| Fixed screen quantity | Result |
| --- | ---: |
| EXP107 control point, median of A1v3/A2v3 | 13024.3755ms |
| Final v4 cold32768 prompt time | 13019.648ms |
| Observed prefill throughput difference | +0.03631% |
| Single-response decode observation | 28.26981tok/s |
| Process VRAM peak / retained | 3348 /2928MiB |
| EXP107 VRAM peak / retained | 3348 /2928MiB |
| Selected128 IDs / ordered top5 / probabilities | Exact, delta0 |
| Request-window swap / swap-out / major faults | 0 /0 /0 |

The fixed2% screen condition was not met. This is effectively a tie, not a
confirmed0.04% improvement. No final-v4 five-pair acceptance series was run
after the stop condition. The screen is not a statistical speed comparison.
The two preselected controls and the candidate all have complete, clean
request-window resource coverage. Their selection was recorded before the
final request; no favorable historical control or repeated final attempt was
chosen after seeing its speed.

## Fixed workload and profile

Ryzen9 5950X and RTX4070 12GB (sm89). Apex-35B-V1.0 Q4_K_M, unchanged
accepted weights. Configured context262144; GPU dense work with CPU MoE weights;
fit off, FA on, Q4_0 K/V, b/ub4096, threads8/16, masks0f0f/ffff, strict affinity,
poll0, one slot, RAM prompt cache0,32 recurrent checkpoints, mmap, speculation
and EHS off. Same sampler: temperature1, top-p0.95, top-k20, min-p0, seed123,
normal EOS. Native completion uses32768 fixed input token IDs,128 output cap,
return_tokens and top5 probabilities. The128 cap is a benchmark choice, not a
new model output limit. Cold means no reusable prompt state; weights are resident
before requests. Refer to reference-profile.json and the preserved fixture hashes.

## Mechanism and scope

The accepted runtime launches expert MMQ against the full token count for every
expert. The copy callback has already downloaded routing IDs to choose expert
uploads. Count exact maximum occupancy from that same snapshot, cache it in one
extra host-only int32, and pass a conservative column bound in private
MUL_MAT_ID op_params[4]. No extra D2H, GPU tensor buffer, weight format,
activation precision or arithmetic kernel is introduced.

The final backend capability permits only sm89 Q4_K/Q6_K shapes where J=128
is provably the existing selector's winner: its column-tile count must be
strictly lower than J=120, and its configuration must fit shared memory.
Small/ambiguous shapes retain the original launch. The caller additionally
limits use to qwen35moe, supported single-GPU plus CPU causal/FA inference,
one sequence, no MoE cache, training, no-alloc or pipeline parallelism.
Unknown backend capabilities retain the original path.

A bound is rounded up to the backend-provided tile width and clamped to the
actual token count. CUDA uses it only when both old and new stream-K plans
remain whole-K tiled, or when stream-K is absent. Thus reduction/fixup ordering
is preserved. The shape, K reduction, precision, quantization, transfers and
existing EXP107 upload pipeline remain intact.

GGML_MOE_EXPERT_BOUND=0 disables the candidate feature. It is default-on only
inside the supported experimental modes; this does not make the rejected
bundle a production default. GGML_MOE_EXPERT_BOUND_DIAG=1 logs route and launch
bounds for diagnosis only. The512-token final activation probe recorded117
host/CUDA expert projections, e.g. raw maximum93 rounded to128 at J=128.

## Why early positive figures were not promoted

The first single v1 observations (+4.199% against an older control and+2.797%
against a fresh control) were exploratory. A v1 CUDA trace confirms actual grid
reduction, but does not establish final-build performance.

The initial v2 five-pair median was+2.884%. Request-window auditing found
swap-out in pairs3..5; three repeat windows also lacked complete coverage.
Those entire pairs were excluded, not just an inconvenient arm. Two clean
pairs were retained and three complete pairs were repeated under quiet swap
checks and longer continuous monitoring. This resource-qualified v2 series
has median+2.3117%,5/5 prefill wins and exact1280 IDs/top5/probabilities.

V2 nevertheless violated a separate correctness contract: CUDA can trust a
reused nonzero graph UID and skip tensor-property comparisons. Changing a
routing-dependent launch grid without invalidating that UID may replay a stale
smaller grid. The32K workload did not exercise that capture case; matching its
outputs was insufficient. V2 was never accepted as a baseline.

V3 fixed UID invalidation and hardware/mode capability gating. Its two new
preliminary pairs gave-0.637% and-0.122%; no full acceptance claim was made.
It was also overly conservative about invalidation: raw maxima can differ
while the effective tile grid is unchanged. V4 normalizes the cache key by the
proven tile width. It retains the UID within a tile bucket and clears it when
geometry changes. The final corrected build then failed the predefined2%
screen above. Earlier results are not transferred to its identity.

This rejects promotion of the present implementation/workload combination.
It does not prove that expert launch compaction cannot benefit another
mechanism or workload. No component-level kernel-time reduction is substituted
for full-request improvement.

## CUDA graph regression

The model-free test uses Q4_K/Q6_K tensors,256 experts,512 tokens,2 routed
experts and3 projections over8 iterations. The scheduler UID is deliberately
reused. Routing changes from balanced to hot at iteration4. A same-bucket
conservative-bound change must retain the UID.

- Full-grid control versus final safe path: byte-exact for both quant types.
- Unsafe negative control, deliberately omitting invalidation: differs starting
  in iteration4. It exposes silent stale-grid output, not an out-of-bounds trap.
- Safe key update preserves capture/replay and same-bucket UID reuse.
- Q6_K safe path under CUDA memcheck:0 errors.

Memcheck alone would not have caught the stale-grid numerical defect. The
negative control and byte comparison are required gates. Test output bytes
remain local; hashes and the standalone test source are included.

## Profiling evidence and limits

The matched EXP107 reference trace has936 large MoE MMQ launches totaling
about2.1565s:624 grids of32768 blocks and312 grids of131072 blocks.
The v1 trace records exact route maxima655..3434 (median1694) rather than4096,
and smaller grids. Those timings describe a superseded prototype and GPU
intervals can overlap H2D. They are not removable critical-path budgets or
performance evidence for v4.

Nsight used12.6 tracing libraries with a newer13.0 driver; preserve that
compatibility warning when interpreting these scoped traces. No CPU-sampling
attribution or universal model-quality claim is made.

## Reproduction and compatibility

Apply patches/moe-launch-bound-experimental.patch after the EXP107 patch chain
at its pinned source identity. Replay reproduces all four changed/new source
files exactly. The private header resides in src/ and uses the existing pinned
GGML internal graph definition; native compilation was checked without extra
ggml/src include paths. build-commands.json records exact incremental commands.
All unchanged CPU/CUDA kernel instances and other runtime components are reused.
Only libllama and libggml-cuda differ from EXP107.

Final experimental source/runtime: /home/end0/AI/LlamaServer/exp117-expert-bound.
Superseded-v1/v2/v3 patches and manifests identify earlier observations and must
not be mistaken for accepted builds. Full raw streams, traces, tensor outputs,
source snapshots and binaries remain local, hashed in the evidence manifest.

Flint and older model launchers remain on their preserved accepted bundles;
no final-v4 Flint or QwenNext model-level performance claim is made. EXP107's
previous Flint qualification remains unchanged. The production service is back
on the exact accepted EXP107 nine-file bundle, with an idle262144-token slot.

## Reopen condition

Require a materially different mechanism, workload or demonstrated bottleneck.
One distinct candidate is a GPU per-expert compact tile schedule derived from
existing expert bounds, with a shape-safe fixed launch envelope. It could avoid
the global hottest-expert bound and CPU recounting, but adds its own prefix/
indexing work and needs separate correctness, capture, VRAM and real-request
qualification. No speedup or implementation of that direction is claimed here.
