# Independent Goose Apex 32K/512 acceptance

The scope is one substantive Apex Q4_K_M workload on the RTX 4070.
Each gate closes once its evidence is recorded. A failed check returns
to the gate that owns the defect. This checklist covers eight gates.
Functional qualification runs before memory and speed acceptance.

**Closed: 5 of 8. Remaining: 3.**

1. [x] **Pin the workload and control.** Verify the model digest, exact
   request bytes and the EXP-1476 executable and library manifest.
   Verify all archived responses against the content, token and
   probability digests below. This establishes a content-qualified
   reference. Its archived speeds are historical. Verified the local
   model digest, all 25 manifest entries including the request, and
   the three archived response digests.
2. [x] **Seal the independent candidate.** Build from tracked sources
   with `tools/build-goose-inference.sh`. Record the revision, toolchain,
   executable, linked archive and embedded GPU code hashes. Check the
   source audit and ELF dependencies for C++ and run CPU/CUDA graph,
   state recovery and HTTP regressions through the rebuilt artifact.
   The clean candidate below passed. Repeat this gate for the final
   executable after any runtime fix.
3. [x] **Match the effective run profile** (requires 1 and 2). Use the
   same hardware, weights, request, context, Q4 KV, one sequence, no
   prompt reuse and the accepted launch profile. Validate the actual
   prefill batch/ubatch of 4096/4096, CPU thread/affinity settings and
   the 4,992 MiB expert-cache budget on both entry points. Record the
   effective configuration. Physical 4096/4096 now completes on real
   Apex and is reported by `/props`.
   The [explicit cache controls](#explicit-cache-profile) verify the
   Q4_0 K/V format and the 4,992 MiB budget on real Apex.
   [EXP-1590](APEX-PERFORMANCE-AUDIT.md#cpu-batch-workers-for-quantized-row-gather)
   verifies 16 threads on the quantized prefill gather.
   [EXP-1591](#effective-cpu-profile) supplies the decode and batch
   limits of 8 and 16 and verifies the effective CPU affinity of
   the frozen reference and the Goose comparison profile.
   The frozen checkpointed reference evaluates the final 4,096
   prompt tokens in parts of 4,092 and 4. The
   [experimental matched tail](CUDA-PARITY-EXP1476.md#actual-apex-checkpoint-tail-geometry)
   reproduces that physical sequence with both runtime limits
   still at 4,096.
4. [x] **Verify the capture path** (requires 1). Use one server at a
   time under the existing Apex lock. Capture the exact request and
   full response, startup configuration, loaded libraries, and RAM,
   VRAM and swap evidence for the serving PID. Verify the capture and
   response gate on a fixture and exercise the rejection path.
   Confirm the prefill/decode timing windows and token denominators
   match, including the treatment of the first sampled token.
   [EXP-1588](#capture-and-timing-convention) verifies the HTTP fixture,
   the rejected request, and one real capture from each executable.
   Both return 512 IDs and report timing rates based on 511 steps.
5. [x] **Verify real-model readiness** (requires 2). On Apex, record
   that all mapped pages are resident before admission. Check actual
   CUDA dense, expert-cache and KV/SSM allocations, and verify that the
   serving process loads only the intended runtime and system
   libraries, without C++ runtime dependencies. A lossy placement
   or a missing required backend keeps this gate open. Verified the
   live 4096/4096 candidate and all 5,301,139 resident model pages.
6. [ ] **Qualify the real response** (requires 1, 5 and the response
   validator from 4). With the exact
   request, record 32,768 evaluated prompt IDs, zero reused prompt
   tokens, 512 returned output IDs and non-whitespace content.
   Archive token and content hashes and every probability row. Compare
   response coherence, factual grounding, instruction following and
   repetition against the control. Investigate reproducible quality
   defects and rerun this gate after fixes. The full 4096/4096 request
   completed with the scalar, MMQ, EXP-1528 and ten-change package
   candidates. The ten-change package response begins to repeat the input.
   ID and probability parity fail. The first differing output is at
   index 1 (1716 versus 198). See the
   [full diagnostic](CUDA-PARITY-EXP1476.md#ten-change-apex-32k512-diagnostic).
   The short layer comparison matches the first GDN output and
   state, all first-layer router logits and all 992 selected expert
   ID positions bitwise. Gate/up, expert SwiGLU and Q6_K down
   now match on the live first layer. The previous checkpoint
   first token matches the 4096/1 reference. Remaining tensor
   differences are recorded in the
   [numerical checkpoint](CUDA-PARITY-EXP1476.md#numerical-checkpoint-before-response-quality-tests).
   The separate short and long
   [response-quality comparison](CUDA-PARITY-EXP1476.md#apex-response-quality-checkpoint)
   confirms coherent generation and the HTTP EOS fix. The
   [earlier speed checkpoint](CUDA-PARITY-EXP1476.md#pinned-staging-for-pageable-expert-uploads)
   reports median 1,049.754 prefill tok/s and 26.256 decode steps/s.
   It preserves the capacity analysis and opening input repetition.
   The [paired boundary captures](CUDA-PARITY-EXP1476.md#first-apex-decode-divergence-and-saved-ssm-history)
   locate a first-layer Q6_K projection difference before the
   history copy, between different microbatch shapes. The
   [matched-tail follow-up](CUDA-PARITY-EXP1476.md#actual-apex-checkpoint-tail-geometry)
   brings the last three QKV rows into exact agreement with
   EXP-1476 and removes the opening task repetition in both
   full runs. [EXP-1587](CUDA-PARITY-EXP1476.md#cooperative-mmq-on-the-checkpoint-tail)
   restores cooperative MMQ at 4,092 tokens and preserves that
   entire response, with 1,026.590 prefill tok/s and 26.838
   decode steps/s on the matched-tail profile. The response
   still miscounts the repeated records. The [four-query Flash tail
   comparison](CUDA-PARITY-EXP1476.md#four-query-flash-attention-checkpoint-tail)
   confirms the reference tile executes on Apex and improves the
   estimated count from 3,000 to 1,600 against 1,723 complete
   records. The action plan still truncates and observed decode
   is 25.567 versus 26.993 steps/s. The four-query tile and
   single-token SSM arithmetic remain experimental.
   [EXP-1594](CUDA-PARITY-EXP1476.md#single-kernel-imrope-for-apex-decode)
   preserves the matched-tail control response, with median
   1,124.843 prefill tok/s and 27.718 decode steps/s across
   two full requests using the fused path. The response still
   overestimates the repeated record count.
   The [cache placement series](APEX-LAUNCH-COMPARISON.md)
   preserves that response on EXP-1595 and measures up to
   31.053 median decode steps/s at the selected 5,920 MiB budget.
   The full response gate remains open.
   Gates 3 and 4 qualify the later speed comparison.
7. [ ] **Accept memory** (requires 5 and 6). Record per-process RSS and
   its high-water mark, model page residency before and after the
   request, per-process sampled VRAM maximum and separate whole-card
   occupancy. Log sample intervals, process swap and system swap-out.
   The candidate must maintain full model residency and avoid swap
   growth and RAM/VRAM regression against the same-session control.
   EXP-1597 records 8,200 MiB sampled peak VRAM for the selected
   Goose profile and 8,202 MiB for the original EXP-1476. Goose's
   RSS high-water remains about 1 GiB higher. The memory gate
   stays open together with the response gate.
8. [ ] **Accept reproducible speed** (requires 1–7). Use sequential
   A–B–B–A with matched cache and thermal conditions. Keep cold and
   warm observations separate. Require the correctness gate on each
   response and same-session median parity or better for both prefill
   and single-response decode. Report units, absolute rates, per-run
   values and spread, memory, exact binaries and raw evidence.
   Resolve a regression in the same gate and rerun the affected checks.
   The [Optimized Hybrid comparator](OPTIMIZED-HYBRID.md) tracks
   the best confirmed hybrid separately. Final optimization targets
   surpassing that comparator with quality preserved. Record matched
   settings, matched device memory and tuned-profile comparisons.

## Explicit cache profile

EXP-1589 accepts `--cache-type-k q4_0`,
`--cache-type-v q4_0` and `--expert-cache-mib N` on
the independent binary. Q4_0 is the supported KV layout.
The loader uses the existing Q4 KV state and the shared
Goose SLRU planner. The budget limits the model's expert
bank on the GPU. Whole-slot packing determines the actual
allocation. `/props` reports both byte counts, both KV
formats and the independent mmap host-registration counters.
The automatic path keeps its existing 4,992 MiB ceiling
and VRAM adjustment. An explicit request fails when CUDA
is unavailable, the budget exceeds available device memory,
or a supported group cannot fit a layer's experts.

The HTTP suite passes 36 requests with explicit Q4_0 and
rejects unsupported cache types, zero MiB and byte overflow.
On the RTX 4070, a 4,096 MiB startup allocates
4,292,247,552 bytes and reports all 5,301,139 model
pages resident. A 1 MiB request fails the layer-fit rule.
A 16,384 MiB request reports 8,123,604,992 available bytes
after model allocations and exits before serving. The CPU
fixture rejects an explicit GPU budget with CUDA disabled.

The capture runner passes an explicit GPU budget when the
fifth positional argument is supplied. Both arms below use
the same binary, matched checkpoint tail, exact request and
4,096/4,096 limits.

| Apex 32K/512 diagnostic pair | Automatic A | Explicit 4,992 MiB B |
|---|---:|---:|
| KV K / V | Q4_0 / Q4_0 | Q4_0 / Q4_0 |
| Configured expert limit, bytes | 5,234,491,392 | 5,234,491,392 |
| Actual expert bank, bytes | 5,234,122,752 | 5,234,122,752 |
| Registered host weight bytes | 4,227,973,120 | 4,227,973,120 |
| Prefill, tok/s | 1,039.476 | 1,041.207 |
| Decode, steps/s | 27.057 | 27.070 |
| Process RSS high-water, KiB | 22,947,692 | 22,947,656 |
| Process VRAM after request, MiB | 7,272 | 7,272 |
| Process swap / system swap-out growth | 0 / 0 | 0 / 0 |

Both responses and all probabilities match outside timing.
The two captured property objects are identical and both
maps contain no C++ runtime, libggml or libllama. This pair
verifies selection and behavior with the explicit budget.
The final performance comparison remains open.

Raw captures, startup probes and the executable are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/explicit-cache-exp1589`.
Executable SHA-256
`81029bb5d93e05f9d30f7da4943738d5a804f01a154c0c1da87996bc47479826`.

## Effective CPU profile

EXP-1591 adds `--threads N` for eligible decode CPU
work and the two phase-specific CPU masks and strict
placement flags. The standalone assigns logical CPUs
using the existing Goose mask planner. System bindings
apply the selected masks and restore the coordinator's
original mask after each graph. The prompt phase is
explicit, including one-token prompt graphs. On the Apex
profile, the gather uses 16 threads for large prompt batches.
Each decode step has one gather row, so the row runs on the
coordinator under the configured eight-thread limit.

The frozen EXP-1476 executable was observed through
short 4096/129 and full 32K/512 requests with its
original 8/16 counts and explicit `0f0f`/`ffff` masks.
Tracing the server and its children recorded no
`sched_setaffinity` calls in either request. A separate
`taskset -c 0` control recorded the expected successful
syscall. The server process had CPUs 0–31 allowed at
readiness and after the full response. The full response
matches the frozen content, IDs and probability hashes.

Use `APEX_GOOSE_THREADS=8 APEX_GOOSE_THREADS_BATCH=16`
with inherited affinity for comparison with that binary.
The explicit strict masks are available through the
CLI and `APEX_GOOSE_CPU_AFFINITY=1` for separate runs.
On the short Goose trace, strict prefill workers 0–15
each report one CPU with the same index. The decode
coordinator reports CPU 0. Both phases restore the
initial allowed mask 0–31. Mask fixtures pass strict
ordering, wraparound, non-strict mode and invalid masks.
The HTTP and model-step checks pass, including the
one-token prefill and decode distinction.

| Unprofiled Goose Apex 32K/512, same binary and 8/16 thread limits | Inherited A | Strict B |
|---|---:|---:|
| Prefill, tok/s | 1,127.697 | 1,126.500 |
| Decode, steps/s | 26.959 | 26.898 |
| Process RSS high-water, KiB | 22,953,120 | 22,952,664 |
| Process VRAM after request, MiB | 7,272 | 7,272 |
| Process swap / system swap-out growth | 0 / 0 | 0 / 0 |

Both use the exact frozen request, context 262,144,
4,096/4,096 limits, matched checkpoint tail, Q4_0 KV
and explicit 4,992 MiB GPU expert budget. They reproduce
the same response, IDs, probability rows, SLRU counters
and all 5,301,139 resident pages. The strict arm checks
placement behavior. The inherited arm records the
effective profile for parity work. Formal performance
acceptance uses its own repeated unprofiled pairs.

The binary, validator, fixtures, raw captures and affinity
traces are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/cpu-affinity-exp1591`.
Executable SHA-256
`bba340b17a3c25c5429bba64eab104d9fca1800639fb28f9e8d4714794b40c98`.

## Capture and timing convention

`tools/apex-test.sh` uses the existing Apex lock for both executables.
Set `APEX_GOOSE_BIN` to an independent `frankenmoe-infer` executable
and `APEX_REQUEST_COUNT=1` for one arm. The runner verifies that its
own PID holds the listener. It saves the request, response, launch
arguments, relevant Goose options, ELF dependencies, live process maps,
status, per-PID VRAM and separate card and swap snapshots. The Goose
path also records `/props` before and after the request. The two
VRAM snapshots identify the serving PID. The peak-resource gate uses
its own sampled request window.

EXP-1476 records the prompt boundary after sampling and accepting the
first token and ends generation timing after accepting the final token,
before reporting its probabilities. See `server_slot_stats` and the
non-speculative sampling loop in `4e722775`. Goose uses the same phase
boundaries around its prompt preparation, sampler and probability
reporting. The response still carries 512 output IDs and a
`predicted_n` of 512. The two derived decode fields use 511 steps.
A one-token completion reports zero decode time and rate.

Use `tools/validate-apex-32k.py --capture REQUEST REFERENCE RESPONSE`
to check the fixed request, response structure and rate denominators.
The report includes the first differing ID and all hashes. The
capture verdict covers these data contracts. Response quality is a
separate gate.

EXP-1588 ran the 36-request HTTP fixture against the rebuilt Goose
binary. The one-token cases verify zero decode time. The two-token
case verifies a single decode step. The capture runner saved the
valid fixture and released the listener after an invalid token
returned HTTP 400. The fixed-workload validator rejects both a
whitespace response and the old 512-step timing fields.

The following captures ran sequentially with the frozen request and
4,096/4,096 limits. Goose used the matched checkpoint-tail profile.

| Capture observation | Frozen EXP-1476 | Independent Goose |
|---|---:|---:|
| Prompt tokens / sampled IDs / decode steps | 32,768 / 512 / 511 | 32,768 / 512 / 511 |
| Prefill interval, s | 12.287 | 31.358 |
| Prefill, tok/s | 2,666.833 | 1,044.954 |
| Decode interval, s | 8.192 | 18.856 |
| Decode, steps/s | 62.377 | 27.101 |
| Process RSS high-water, KiB | 21,863,992 | 22,948,988 |
| Process VRAM at ready / after, MiB | 6,596 / 8,202 | 6,510 / 7,272 |
| Process swap / system swap-out growth | 0 / 0 | 0 / 0 |

Both listener and status snapshots identify the serving PID.
The capture validator passes both responses. The fresh EXP-1476
response matches the frozen response hashes. Goose matches
EXP-1587's matched-tail response outside the timing object,
including all 512 IDs and probability rows. Its live maps contain
no `libstdc++`, `libggml` or `libllama`. This is a capture check
with one request per executable. Resource peaks, quality and
reproducible performance remain open under their own gates.

The binary, fixture, raw captures and validator output are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/step-timing-exp1588`.
Goose executable SHA-256
`ade09ef5546b9fec6c6b26efb6835bc066d17181d20499aae7d78eae5a782d43`.

## Fixed evidence

The control is EXP-1476 from [the baseline record](APEX-BASELINE.md#verified-content-in-exp-1476).
Its raw archive is `results/apex-runs/rel-exp1476-20261119-212613` in the
original checkout. The control is used for comparison only. The
independent artifact is `frankenmoe-infer`. The newline-only run under
`results/p610-32k-1197` is excluded.

| Evidence | SHA-256 |
|---|---|
| Apex model | `c3b92d94cd03a7730ad45cc45de8c753e369e0228b2f5d83d819ce2ea6ab3355` |
| Request bytes | `484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f` |
| UTF-8 content | `c2ff42cc1219ea4dfe6d69f02e008d0f90c0fc56260736a0aafb82cec362924f` |
| Output IDs, compact JSON | `fdf27fb67f12a5c0dcfa04ad59daf47100749df70cc096ed6cda8f9285e3d54c` |
| Control probability rows, compact sorted-key JSON, UTF-8 | `20ac4af910d2e4c92dcbcaacdef1085b74241b2ee7447bb7825f9c18a99b5dd8` |

## Clean candidate evidence

Revision `c0ad7cca4e5c4609eb279504858fb8c2913a8832` was built from
scratch in `/home/end0/AI/LlamaServer/franken-goose-apex-gate.5QTT9m`.
That directory contains the compiler digests, build log, ELF dependency
table, and test logs. The source audit found no C++-bearing sources.
The executable declares only `libc` and `libm` dependencies. The PTX
bytes were verified inside the executable.

| Artifact | SHA-256 |
|---|---|
| Inference executable | `46b8045ec078af9f97faec0492ca76d13aba5119ba0439d8c6feb5dbb78be35a` |
| Inference archive | `8442f0d64e22beea422045fbca328101ea04dc4edab3e9f332ba5109c0fd6d2e` |
| CUDA PTX | `2d17f94fab033ea2e16e399ada0bdb8c5029d2dec14e10573e6d67c045e94345` |
| Composed Goose object | `33dea848af0dfdb1effe8e2198a1f9960e3a03a91a24a6bfbc46f8868a4dcdc1` |

Five synthetic gates passed through this build. They covered CPU
chunked prefill, mixed CPU/CUDA continuation, quantized expert dispatch,
step and workspace recovery, and the 34-request HTTP suite.

The subsequent 4096/4096 candidate and the measured CUDA regression
are recorded in [the execution audit](APEX-CUDA-REGRESSION.md).
