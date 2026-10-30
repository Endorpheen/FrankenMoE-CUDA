# EXP107: bounded expert-upload/compute overlap

Status: ACCEPTED for the measured Apex profile under Igor's explicitly accepted
tradeoff: more than 10% prefill throughput for less than 10% peak VRAM growth,
with at most a small decode regression and unchanged numerical results.
Reference: EXP101, project 87209f121765d0fa4f4af752ea56a548864f5493.

## What changes

Two external GPU expert-copy slots replace ordinary graph-arena staging for
eligible execution graphs. A nonblocking upload stream waits for each slot's
read-complete event, uploads the original selected byte ranges, and publishes a
ready event. The compute stream waits for readiness and records completion after
its split, outside CUDA graph capture. This overlaps already-selected gate/up/down
copies with earlier work; no future-layer routing is predicted.

Total slot capacity is capped at 512 MiB (420 MiB on Apex). Allocation failure
falls back before binding any tensor. Slots are released when allocation moves to
an ineligible graph, including the small CPU-expert checkpoint tail/decode. Their
allocation cost is included in cold and branch requests. Source mmap registration,
lazy compute reservation, lazy KV, precision, arithmetic kernels, checkpoints,
thread placement, sampling and EOS handling are preserved.

The final caller enables the feature only for qwen35moe, one GPU plus CPU,
one sequence, normal causal inference with FA, no MoE cache or pipeline
parallelism. The backend additionally requires registered contiguous Q4_K/Q6_K
expert weights and prefill with at least 32 routed tokens. Metadata/no-alloc
reservations do not allocate the ring. Other architectures, including Flint and
Qwen3Next, retain the original path. `GGML_EXPERT_OVERLAP=0` disables the feature.
This is scoped support, not an all-model or parallel-slot compatibility claim.

## Manual five-pair result

RTX 4070 12 GB, Ryzen 5950X, Apex-35B-V1.0 Q4_K_M. Same exact EXP101 profile:
context262144, CPU MoE, FA, Q4_0 KV, b/ub4096, threads8/16, masks0f0f/ffff,
strict affinity, poll0, np1, mmap, speculation/EHS off, cache-ram0/checkpoints32.
Weights are resident before each request; cold means no reusable prompt state,
not a disk-cold model. Each fresh process executes cold32768 then repeat32764+4,
up to128 outputs, seed123, unchanged sampling/EOS and top5. Order:
A1/B1, B2/A2, A3/B3, B4/A4, A5/B5. No parallel benchmark/build/helper inference.

| Pair | Prefill throughput change | Cold decode change | Repeat decode change |
| --- | ---: | ---: | ---: |
| 1 | +10.783% | +2.470% | -7.052% |
| 2 | +10.609% | -0.421% | +10.435% |
| 3 | +9.364% | -10.618% | -8.301% |
| 4 | +9.116% | -1.887% | +1.876% |
| 5 | +10.701% | -0.261% | +2.461% |
| Median paired change | **+10.609%** | **-0.421%** | **+1.876%** |

Prefill wins 5/5. Arm-median prompt times: 14.30036 ->12.97812 seconds;
arm-median prefill throughput: 2291.41 ->2524.86 tok/s. These ratios differ from
the median of the five paired ratios; do not substitute one for the other.
Decode varies substantially; the third pair is retained, not discarded. No
causal explanation for that outlier or promise of identical decode is asserted.
Desktop background was not globally isolated. Process swap, system swap-out and
major faults were zero across all ten sampled post-ready request blocks.

All 1280 compared output IDs per arm, ordered top5 IDs and probabilities match
exactly (max delta0). Separate Apex branch at24576 also matches exactly.

## Peak and retained memory

Every pair: sampled process peak3188 ->3348 MiB (+160MiB/+5.019%).
After prefill the 420MiB ring is released: steady observed process memory is
2928MiB, versus3188MiB for EXP101 (-260MiB/-8.156%). Both states are also
confirmed in the final guarded build; the off switch returns peak3188MiB.
The smaller retained state is consistent with removing expert staging from
ordinary graph-arena planning. These are process NVML samples, not claims about
all contexts or an exact attribution of every driver allocation.

## Mechanism, not profiled speed acceptance

Matched Nsight captures move exactly the same 131140214784 expert-copy bytes
in28128 H2D spans. They move from the compute stream to a separate upload stream.
Kernel/H2D interval intersection in the client cold-prefill window:
EXP1010 ns; EXP1071415232689 ns. Profiled windows14.3784 and12.9567 seconds
are diagnostic only; the five unprofiled pairs above establish performance.
The overlap interval is not added to other potential savings.

Profiler caveat: installed driver13.0 is newer than this Nsight build, which
used12.6 tracing libraries; no NVTX events were present. Preserve those warnings
when interpreting this scoped timeline. New trace identifies Q4_K/Q6_K MMQ as
about3.266s summed kernels in the12.9567s candidate prefill window; this is not
a measured removable critical-path share.

## Final-build and compatibility gates

Formal pairs used the preserved v1 libraries. The final change only adds the
qwen35moe architecture predicate at scheduler creation, outside timed inference.
No pipeline/kernel/copy code changed afterward. Final default and off modes
both pass Apex cold/repeat exactly. Observed final cold times13.1147/14.7222s
are a functional switch check, not a second statistical speed claim.

Flint cold32768, branch24576 and early rewind4096->16384 match the stored EXP101
results exactly, max probability delta0; sampled peak remains2536MiB. Those
three cases ran before the final architecture predicate; they used unchanged
Flint kernels and never selected a Q4_K/Q6_K pipeline candidate. The final
predicate explicitly bypasses the pipeline for Flint. No Flint speed claim or
full-context, multi-slot, allocation-failure-injection acceptance is made.

The model-free harness tests three projections through two slots over eight
iterations for Q4_K and Q6_K. Control/overlap output bytes match exactly, including
release/reallocate between iterations4/5 with retained graphs. Budget and
unsupported-type refusals pass. A short CUDA trace records21 actual
cudaGraphLaunch calls. Q6_K release/reallocate under CUDA memcheck:0 errors.
No benchmark claim is based on that small harness.

The initial Bscreen probe accidentally loaded copied SONAME aliases containing
old libraries; it is excluded. SONAME symlinks were corrected before all
qualification/formal runs; mapped candidate libraries were checked. Early HTTP
503s during model loading did not execute requests and are not measurements.
Switching back from Flint required slow exFAT disk reads; that load time is
excluded from prefill/decode results.

## Reproduction and preservation

Apply `patches/expert-h2d-compute-overlap.patch` after the EXP101 patch chain.
Patch replay reconstructs all five changed/new source files byte-for-byte.
`build-commands.json` records the incremental compiler/linker arguments; only
libggml-base, libggml-cuda and libllama change. Reused CUDA kernel/CPU/server
objects and accepted binaries remain untouched. Runtime identity includes all
nine executable/library files, not just the unchanged server executable.

Runtime: /home/end0/AI/LlamaServer/exp107-expert-overlap/build/bin.
Full local sources, objects, traces and harness binaries remain there.
The local raw archive is hashed in local-evidence-manifest.json; prompts and
raw process details are not published. Launch profiles are foreground argv;
no installed Desktop profiles, services or launcher defaults are changed here.
