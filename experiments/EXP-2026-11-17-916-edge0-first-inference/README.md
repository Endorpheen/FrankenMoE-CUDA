# EXP-2026-11-17-916: first Edge0 inference through FrankenMoE

Status: **ACCEPTED FIRST BASIC RUSSIAN ANSWER; NOT PERFORMANCE OR GENERAL QUALITY ACCEPTANCE**.

The integrated engine executes the real 48-layer qwen4exp backbone, including
HC, recurrent/full attention, indexer, PLE token history and dilated convolution.
A CPU GGML custom-op boundary invokes the existing Goose router, EXP915 latent
expert graph, a new ten-expert/shared composition, and bounded external weight
reads. EXP910/911 supply actual FP8 PLE rows. This is FrankenMoE's retained
qwen4exp/GGML implementation, not a separately launched Strata engine.

The hypothesis and initial bounds were recorded before implementation in
`integration/edge0/PLAN.md`. Native C++ remains required; the temporary host
integration is debt. This result does not complete the standalone Goose migration.

## Fixed inputs and layout

Reuse the EXP908 full-file SHA receipts for the complete local HF revision
`dab5ce727d494e2018626fe8896bfdc774215b17`. No weights were downloaded, converted,
modified or duplicated. This pack differs from current-main D/U; do not mix them.
D/U: Q4_0, eight groups, 2560 ->1280 ->2560. Routed gate/up: Q3_K,
down: IQ4_NL. Ten routed experts plus a full-width Q8_0 shared expert.
The column scale reciprocal is applied exactly once after D.

The loader skips only the absent ordinary expert matrices and external PLE table
when this explicit pack mode is selected. The ordinary qwen4exp path remains.
The common sampler/tokenizer, state and native CUDA backend are retained.
All sources used to build llama.cpp are staged under LlamaServer. No Apex
runtime, profile, source checkout, or accepted ELF set is overwritten.

## Integration and gates

Seven Goose modules compose/check and emit C with the pinned 2026.3-string
compiler. The full native model library and CLI link; `ldd -r` passes for the
saved independent checkpoint. Only changed CLI objects were rebuilt after its
API and decode-counter corrections. Original failures/logs remain available.

An independent native GGML graph checked real layers 0/47, top-10 IDs and route
weights (absolute tolerance 2e-6 fixed before the run), and all 2560 combined
FFN outputs per layer (absolute + relative tolerance 2e-5 each). Both cases
passed; maximum absolute output error was 4.65661287e-10. The actual custom-op
callback, Goose route, latent experts and shared branch executed. Peak component
RSS: 329892 KiB. The first fixture failed on test-buffer alignment; only that
fixture was corrected and rebuilt. No numerical tolerance was changed.

PLE uses the existing KV predecessor lookup and native PLE history/convolution
state with Goose positioned FP8 row decode. The real generation exercises this
path. Chunked-versus-single-shot PLE equivalence, concurrent sessions, long
context, server transport and broad quality have **not** been accepted here.

## Real generation results

One process at a time, RTX4070 12 GiB, ctx512, batch/ubatch1, t8, ngl48,
MTP off, greedy diagnostic sampling and unmodified normal EOG/EOS policy.
The exact local Jinja template is rendered with `enable_thinking=false`, which
the template supports. This is explicit diagnostic sampling, not the card's
recommended temperature1/top-p0.95/top-k20 profile.

| Run | Prompt / visible output tokens | Load s | Prefill tok/s | Decode compute steps/s | Max RSS MiB | Process VRAM MiB | Result |
|---|---:|---:|---:|---:|---:|---:|---|
| 1 | 26 /3 | 11.0820 | 0.9380 | 1.2180 corrected | 6020.20 | 5296 | `Парис`, rejected spelling, normal EOS |
| 2 | 29 /2 | 1.4304 | 1.2913 | 1.1542 | 6135.14 sampled | 5296 | `четыре`, correct basic answer, normal EOS |

Run2 asked, in Russian, what two plus two equals, requiring one Russian word.
Its exact answer SHA256 is
`fd4bca526fa9c822c80fd007b1a14851db4e63ebe898d4dc87add43f7c34f077`.
First-token latency was 22.461936 s. Two decode evaluations took 1.732847 s,
including the final evaluation producing EOS. The original run1 counter omitted
that final compute from its numerator; preserve its raw report and correct the
interpretation to 3 /2.463059 s, not 0.8120. No tokens were hidden or EOS masked.

These outputs are too short for representative throughput or quality claims.
Run2 follows earlier reads and is not a cold-load comparison. Run1 process swap
was zero but system swap-out grew 35616 pages; run2 both remained zero.
RSS excludes global filesystem page cache. Streaming bounds application-owned
buffers, not the kernel's reclaimable page cache. Current explicit buffers are
108248064 bytes: one layer D/U, ten expert records and a 64 MiB arena. PLE uses
2560 raw bytes plus 10 KiB F32 rows per token. There is no application expert
retention cache yet. CPU/GPU compute, copies and I/O remain unprofiled.

## Apex control, completed before the subsequent optimisation request

Same protected nine-ELF runtime set, identical SHA manifest, ctx262144,
cache4992 MiB, t8/tb16, 32768 actual prompt tokens and 512 predicted tokens,
seed123, three serial requests, no prompt cache. EXP917-919 are **not** wired
into this server. Do not attribute these numbers to those components.

| Condition | Decode median (range) | Prefill median (range) | RSS after MiB | Total GPU MiB | Answer SHA prefix |
|---|---:|---:|---:|---:|---|
| Historical accepted | 55.5176 (51.7606-57.5273) | 2625.2066 (2489.3719-2906.5786) | 21350.90 | 11468 | c2ff42cc1219 |
| First diagnostic, 66.3% file resident before start | 40.1018 (36.2918-40.4350) | 2140.5581 (1231.8018-2151.1416) | 19525.21 | 11601 | c2ff42cc1219 |
| Corrected control, 100% file resident before start | 55.4914 (54.9132-55.6045) | 2833.3014 (2608.4275-2855.2742) | 21355.47 | 11641 | c2ff42cc1219 |

Corrected relative observations: decode -0.0472%, prefill +7.9268%, RSS +0.0214%,
total GPU +1.5085%. GPU totals include desktop/other clients, not exclusively
the server. This is preserved-code control, not evidence of a new optimisation.
All six response strings AND token arrays matched the pinned control. Both
blocks had process swap0 and no system swap-out during the requests.

The existing registration residency guard explains the first block's mode:
startup pin acquisitions3889 pages versus4765609 after full residency. The
remaining 207710 absent pages (~812 MiB) were read **between** lifecycles, then
mincore verified 100%. No competing read/compile was done during requests.
The lightweight mincore monitor was initially rejected by automatic review as
if it read the whole model; inspecting its source proved it only reads the page
bitmap, and the monitor was then approved. All owned processes exited and port
9101 was released; no unrelated service was stopped.

## Saved checkpoint and reproduction

Independent byte copies, verified hashes:
`/home/end0/AI/LlamaServer/edge0-first-answer-20261117/`.
Six runtime ELF files occupy220039792 bytes; external CUDA/system dependencies
are recorded in `dependencies.txt`. Source patch, generated Goose input,
manifest, both requests/answers/logs and command are preserved. No model weights
are included. Parent directories and source snapshots remain immutable.

Run the saved successful request:

```sh
LD_LIBRARY_PATH=/home/end0/AI/LlamaServer/edge0-first-answer-20261117/run \
FRANKEN_EDGE0_NGL=48 \
/home/end0/AI/LlamaServer/edge0-first-answer-20261117/run/edge0-cli \
/home/end0/AI/LocalAI/Models/Edge0-120B-q3k \
/home/end0/AI/LlamaServer/edge0-first-answer-20261117/prompt.txt 32
```

This is a CLI, not an HTTP server. The second argument is an already rendered
prompt file. `integration/edge0/run.py` renders an arbitrary `--prompt` through
the local template, records resources, and refuses an occupied runtime port.
Build stages are selectable via `integration/edge0/build.py --out <new-directory>
--step stage|goose|configure|build`. Compilation/link is the reason for the full
integration build; no CUDA kernel rebuild was performed.

## Retention audit and next action

Baseline C++ replaced-and-removed0; dead C++ deleted0. The retained baseline
closure remains248213 lines/350 files; minimum necessary subset UNKNOWN.
The new native resource/callback/CLI glue is210 lines and remains explicit debt.
No claim of independent Goose execution follows from seven composed modules.

Igor subsequently prioritised actual Edge0 performance. Next: instrument a real
longer decode without changing numerical policy, identify I/O, D/U, CPU/GPU,
synchronisation and graph-creation costs, then optimise measured bottlenecks.
Keep this first-answer checkpoint and compare exact output tokens with normal
EOS. Do not resume Apex benchmarking while working on that task.
