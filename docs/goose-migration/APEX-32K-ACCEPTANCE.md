# Independent Goose Apex 32K/512 acceptance

The scope is one substantive Apex Q4_K_M workload on the RTX 4070.
Each gate closes once its evidence is recorded. A failed check returns
to the gate that owns the defect. This checklist covers eight gates.
Functional qualification runs before memory and speed acceptance.

**Closed: 3 of 8. Remaining: 5.**

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
3. [ ] **Match the effective run profile** (requires 1 and 2). Use the
   same hardware, weights, request, context, Q4 KV, one sequence, no
   prompt reuse and the accepted launch profile. Validate the actual
   prefill batch/ubatch of 4096/4096, CPU thread/affinity settings and
   the 4,992 MiB expert-cache budget on both entry points. Record the
   effective configuration. Physical 4096/4096 now completes on real
   Apex and is reported by `/props`. The CPU executor remains serial.
   Explicit KV type and expert-cache controls are pending. Treat
   performance under a different effective profile as diagnostic.
4. [ ] **Verify the capture path** (requires 1). Use one server at a
   time under the existing Apex lock. Capture the exact request and
   full response, startup configuration, loaded libraries, and RAM,
   VRAM and swap evidence for the serving PID. Verify the capture and
   response gate on a fixture and exercise the rejection path.
   Confirm the prefill/decode timing windows and token denominators
   match, including the treatment of the first sampled token.
   The offline validator `tools/validate-apex-32k.py` already accepts
   all three archived controls, rejects the whitespace-only sample
   and identifies the first altered output ID. The archived control
   reports 512 IDs and uses 511 steps to calculate its decode rate.
   The candidate HTTP field currently uses 512. The validator records
   both the raw timings and the rate derived from 511 steps.
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
   candidates. The latest package response begins to repeat the input.
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
   [latest full diagnostic](CUDA-PARITY-EXP1476.md#cooperative-dense-q4_k-mmq)
   reports 804.407 prefill tok/s and 20.333 decode steps/s.
   It preserves the recorded capacity analysis and opening input
   repetition. The full response gate remains open.
   Gates 3 and 4 qualify the later speed comparison.
7. [ ] **Accept memory** (requires 5 and 6). Record per-process RSS and
   its high-water mark, model page residency before and after the
   request, per-process sampled VRAM maximum and separate whole-card
   occupancy. Log sample intervals, process swap and system swap-out.
   The candidate must maintain full model residency and avoid swap
   growth and RAM/VRAM regression against the same-session control.
8. [ ] **Accept reproducible speed** (requires 1–7). Use sequential
   A–B–B–A with matched cache and thermal conditions. Keep cold and
   warm observations separate. Require the correctness gate on each
   response and same-session median parity or better for both prefill
   and single-response decode. Report units, absolute rates, per-run
   values and spread, memory, exact binaries and raw evidence.
   Resolve a regression in the same gate and rerun the affected checks.

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
