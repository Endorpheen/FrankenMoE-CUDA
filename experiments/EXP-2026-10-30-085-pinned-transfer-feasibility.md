# EXP-2026-10-30-085: bounded pinned transfer feasibility and compatibility gate

Status: REJECTED (`kind=model-free-feasibility`) for proceeding with this direct bounded-ring transfer candidate. Correctness passed; the performance gate failed. No runtime change or accepted model speedup.

## Hypothesis recorded before implementation

The large pageable H2D path identified by EXP-084 may become cheaper with two 16 MiB pinned staging slots, including CPU memcpy, event waits and the final stream drain. Compare it with direct pageable submission on the same trace-derived size/order sequence and a representative real GGUF backing file. Require at least 5% median complete-path reduction and at least four of five paired wins before considering integration.

Reference: project commit `7e7c5bc4f792ee71a9a259c41a15332c0a5d036c`; the existing unified Flint/Apex runtime is immutable. The pinned budget is 32 MiB, with no model-wide registration and no extra copy stream.

## Scope of this first gate

The Nsight capture identifies copy sizes and order, not exact expert tensor names or GGUF source offsets. This gate uses its cold-prefill pageable H2D geometry and deterministic offsets in the real Apex GGUF. It is a representative transfer feasibility test, not an exact replay of routing or proof of model-level speedup. A positive result requires a later integration experiment and matched model A/B.

All llama.cpp/CUDA probe build artifacts stay in a separate directory under `/home/end0/AI/LlamaServer/`. Existing working source/binaries, old experiment builds and `work/llama.cpp-integration` are unchanged. Preparation and Apex helper inference must finish before GPU measurements begin. Runs are manually invoked, serial, with the working Apex server stopped and later restored.

## Compatibility contract

- Preserve all previously accepted source patches and builds; do not overwrite them with a new-model candidate.
- No model architecture, quantization, routing, token ordering, KV/recurrent checkpoint or MTP behavior changes in this gate.
- Byte-copy correctness includes unaligned sizes/offsets, sentinel guards, 16 MiB chunk boundaries, repeated slot reuse and final drain. The mechanism must preserve arbitrary bytes, including expert padding.
- Any later integration must retain native behavior for ineligible shapes/backends, graph capture and allocation failure; unsupported models/modes must not silently enter an unverified path.
- Later integration acceptance must include Apex and Flint model/state checks, and an explicit legacy matrix for Qwen/Ornith quant types, CPU MoE, small decode/verify batches and speculative paths. Model-free byte checks do not establish whole-model coverage.
- New features may be promoted only after their correctness/performance gates; accepted features are default-on with an off switch. A staging probe mode is not production activation.

## Planned execution and stopping conditions

1. A user-visible local Apex task extracts the pageable H2D geometry with a small read-only SQLite helper. Parent reviews it; do not rewrite correct code cosmetically.
2. Parent prepares a standalone CUDA probe and correctness cases. Probe allocations and resources are bounded, core dumps disabled.
3. Run correctness first. Invalid bytes, guards, allocation handling or lifetime stop the branch.
4. Only after correctness passes, manually run five direct/ring pairs with balanced order, equal warm-page preparation and a final drain included in every reading. Record memory, process swap and new global swap-out. No concurrent helper, build or benchmark.
5. Reject on insufficient complete-path benefit; do not integrate merely because host enqueue improves. If the representative gate passes, preserve its exact limits and prepare the next real-callback/model integration gate.

## Results

The local Apex helper implemented `results/exp085-pinned-feasibility/extract_geometry.py`. A substantive review found that its first version sorted the copy-size sequence; Apex corrected it to preserve the SQL trace order and use a separate sorted list for percentiles. Parent independently compared all rows with the read-only SQLite query. The delivered geometry contains 28128 positive byte sizes totaling 131140214784 bytes, maximum 90317312 bytes. The corrected helper code was accepted without parent rewriting.

The standalone CUDA-runtime probe was compiled with g++-13 under `/home/end0/AI/LlamaServer/exp085-pinned-feasibility/`. It uses two 16 MiB pinned slots, completion events before slot reuse, the same nonblocking stream as its direct control, and a final synchronization inside the timed path. Ring allocation/event initialization is included in total time. Common device-buffer allocation and identical full-file page preparation are outside the timer. No model or inference kernel runs in the probe.

Correctness: **60 byte-and-sentinel cases PASS**, including zero/unaligned sizes, 512-byte boundaries, 16 MiB and two-slot boundaries, repeated reuse, partial-allocation failure with direct fallback, and refusal to stage inside graph capture. The source is a real 21713463136-byte Apex GGUF, mapped read-only; performance offsets are deterministic and deliberately labeled as representative rather than captured expert offsets.

Five pairs were invoked manually, serially, with alternating pair order. The ten readings took approximately 381 seconds after correctness, within the stated eight-minute block. No helper inference, build or other benchmark ran alongside them.

| Pair | Order | Direct total s | Ring total s | Paired time reduction |
| --- | --- | ---: | ---: | ---: |
| 1 | direct, ring | 13.355131 | 13.774050 | -3.137% |
| 2 | ring, direct | 13.329319 | 13.395609 | -0.497% |
| 3 | direct, ring | 13.543721 | 13.089118 | +3.357% |
| 4 | ring, direct | 13.507299 | 13.926907 | -3.107% |
| 5 | direct, ring | 13.233399 | 13.603688 | -2.798% |

The primary paired median is **-2.798%**: a slowdown, with **1/5 wins**. The required >=5% reduction and >=4/5 wins both fail. Separate arm medians are direct 13.355131 s and ring 13.603688 s; their ratio is a different statistic and is not substituted for the paired result.

Both modes transfer exactly the same 131140214784 bytes. Direct makes 28128 submissions; ring makes 29238 chunks. The extra submissions arise from the 16 MiB slot limit. This result does not establish which portion of the loss comes from CPU gather, chunking or memory-bandwidth contention.

## Resources and compatibility outcome

Every timed reading reported VmSwap=0, zero major faults and unchanged system `pswpout=7940326`. The ring uses 33554432 pinned bytes; the common device buffer is 90317312 bytes in both modes. Peak recorded RSS is 21303372 KiB direct and 21336656 KiB ring, a 33284 KiB difference across separate runs. This is a representative source-mapping probe RSS, not model runtime RSS. Preparation/helper/server-restart intervals are not claimed globally swap-free.

The idle working Apex was stopped before GPU checks and restored afterward with the identical command/environment on 8081; health returned `ok`. All executable/library hashes still match the EXP-083 reference. No engine source, existing runtime, accepted old build, launch profile or legacy integration checkout was modified. Therefore this rejection cannot remove the earlier accepted Qwen ring or alter Flint/Apex checkpoints. It does not add new whole-model regression coverage.

## Verdict and retry condition

REJECTED for moving this direct bounded-ring candidate into the current engine. Working byte copies alone are insufficient, and the measured complete path does not clear the pre-stated gate. No model A/B or runtime integration was started.

The rejection is scoped to this representative isolated-transfer path. It does not invalidate EXP-038's measured benefit on its older Qwen workload, nor prove that staging can never help with actual compute interleaving. A retry requires materially changed evidence: exact callback ranges/order and interleaved compute, or a revised mechanism with a quantified gather/submission cost. Do not repeat the unchanged five-pair probe or disable old accepted optimizations based on this result.

## Artifacts and environment limit

`results/exp085-pinned-feasibility/` contains the accepted geometry extractor, geometry, standalone probe, correctness result, ten readings, paired comparison, review and provenance. The probe is an experiment artifact, not an engine feature; no production opt-in switch was introduced.

An initial helper self-check was blocked by the installed Desktop executor's filesystem policy for `/tmp`; its successful artifact was produced inside the project directory instead. This environment issue is separate from the helper's corrected ordering bug. No Desktop security policy, installed profile, package or application was modified in this experiment. Granting actual extra write roots in that installed runtime remains a separate pending configuration/runtime task; prompt text does not grant that permission.
