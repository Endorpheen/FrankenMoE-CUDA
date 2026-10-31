# EXP132: native one-token speculative decoding on EXP107

Status: REJECTED for baseline promotion after a bounded diagnostic screen.
No engine source, runtime libraries, installed launcher, or Desktop settings changed.
The accepted EXP107 server was restored on8081, healthy/idle; all nine mapped
runtime hashes match its immutable manifest.

## Reference and workload

Project baseline d73f7d59bd0b50a4f0310759be25af61961cbfd1, runtime and flags from
[EXP107](../exp107-expert-overlap/launch-profiles.json). Apex Q4_K_M, context262144,
all40 CPU expert layers, b/ub4096, threads8/16, masks0f0f/ffff, poll0, one slot,
FA/Q4_0 KV, mmap, cache-ram0/checkpoints32, EHS off. No new weights downloaded.

Two fixed greedy requests per process: the frozen32768-token prompt/128 output,
and839-token repeated Python source/256 output. Eight processes in manual order:
A1, M1, S1, K1, V1, N1, S2, A2. Six candidate configurations, including an inactive
S1 negative diagnostic and the valid S2 replacement. Total16 measured requests;
all completed. Resource monitoring only alongside each request; no background
builds or other benchmark workloads. Native initialization/warmup is outside
request timing. No automatic parameter search.

MTP shares the target model weights and uses its included one-layer head.
Ngram-cache has a hardcoded8-token draft in this version and was excluded from
the one-token scope. External draft/EAGLE/DFlash/DSpark models were not tested:
no verified compatible local drafter was selected. These exclusions are not
performance rejections of those methods.

## Qualified code screen

Each candidate below and control A2 have full monitoring coverage, process
VmSwap0, system swap-out0 and major faults0 during the code request. A1 code
had2 major faults and is excluded as a qualified performance control. These
are individual diagnostic observations, not five-pair acceptance estimates.

| Profile | Code decode tok/s | Change vs A2 | Accepted / drafted |
| --- | ---: | ---: | ---: |
| A2, speculation off | 28.8954 | reference | none |
| M1, MTP n_max1 | 19.3578 | -33.01% | 127/127 |
| S1, simple key12/value1 | 28.7436 | -0.53% | 0/0, inactive |
| K1, map-k key12/value1 | 27.5772 | -4.56% | 71/75 |
| V1, map-k4v key12/value1 | 27.0720 | -6.31% | 71/74 |
| N1, mod match24/min1/max1 | 27.9262 | -3.35% | 60/64 |
| S2, simple key1/value1 | 20.6157 | -28.65% | 72/127 |

No active speculative profile beats the clean control. MTP loses even with
100% acceptance on this code sample. High acceptance does not establish a
speedup: draft, batched verification and state handling remain real costs.
Their individual shares were not profiled in this screen.

## Long prompt and resource qualification

A1/M1/S1/K1/V1/N1 full long-request windows have system swap-out149/1031/89/
210/496/779 pages, respectively. Process VmSwap and major faults remain0.
Their full-request timings are contaminated and must not support clean speed
claims. Clean end control A2 long decode is27.7020tok/s; clean S2 is16.7582.
The unchanged control observations span27.7020-28.0992tok/s long and
28.8954-29.1028tok/s code, with the initial resource caveats above.

## Memory and correctness

Sampled reference and ngram peak/retained VRAM:3348/2928MiB. MTP long reaches
3740/3340MiB; MTP code reaches3762/3342MiB. Code peak +414MiB (+12.37%); retained
+414MiB (+14.14%). This includes the actual additional MTP context and state;
sharing weights does not imply zero additional memory.

Every selected greedy token ID matches both controls (2304 candidate tokens,
including384 from inactive S1; controls also match each other). K/V/N and S1
have exact ordered-top5/probabilities. M1/S2 code fail the predeclared strict
numerical gate: first ordered-top5 mismatch at position185 and maximum
comparable probability delta0.00389939599, above0.001. Selected tokens still
match; this is not evidence of measured answer-quality degradation. No gate
was relaxed to accept a speculative result. The historical comparison helper's
ambiguous same-selected-ID mismatch wording is clarified in the curated output.

Source inspection explains inactive S1: common_ngram_simple_draft refuses
copy_max < size_ngram. Therefore value1/key12 cannot emit a draft. Exactly one
valid replacement key1/value1 was tested; it loses. Ngram modes do not request
bounded recurrent snapshots through need_n_rs_seq(), unlike MTP. Checkpoint
fallback is therefore a potential cost, not an independently measured timing
attribution or a newly established runtime bug.

## Decision and artifacts

No promotion, tuning sweep, stochastic/quality expansion, or prefix-session
regression: no profile qualifies. Findings do not rule out longer drafts,
trained lightweight drafters, or future improvements to verification/rollback.
EXP107 remains the baseline with all its accepted optimizations intact.

Files: plan.json, launch-profiles.json, screen-results.json, analyze.py,
restored-baseline.json, local-evidence-manifest.json, SHA256SUMS. Raw requests,
curl traces, SSE streams, logs and monitoring stay local under
results/archive/EXP-2026-10-31-132/raw; the manifest records hashes and sizes.
