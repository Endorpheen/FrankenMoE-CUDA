# EXP-2026-09-09-048 — which decode operation remains noticeable and was never optimized (offline)

Status: `DONE` (`kind=offline-analysis`, 2026-09-09; assignment from the senior helper: using the
available data, identify the remaining operation with a noticeable share of decode, pick a
candidate for ≥2% without parameter sweeping; do not assign any new specific optimization before
this check). The server, model, profiler, and build were NOT launched; there are no new runtime
measurements — only an offline analysis of saved artifacts. No performance claims; all shares
below are estimates with explicit bounds, tied to direct measurements.

Fix of 2026-09-09 per Igor's comments (before the commit): the conclusion was weakened to a
hypothesis. The W2 window is an approximation of verify, not its measurement; sums of CUDA API
times are not removable latency; the absence of CUDA-graph in W2 does not prove its absence in
real verify; the data justify a research priority, not the exclusion of the remaining
possibilities. Final wording: the offline analysis singled out host serialization as a promising
hypothesis; its contribution and removability in the current decode require confirmation.

## Sources (all — saved artifacts)

1. EXP-043 trace: nsys sqlite `f86cd32ba4c05d611e448dd76d793b517233a5a4af934f8017f149ce48e34a2f`
   (`/tmp/exp043-profile/exp043.sqlite`, the hash matches the one recorded in the EXP-043 card),
   opened strictly read-only (`immutable=1`). The W2 window = the 4-token trunk batch from the
   EXP-043 tail attribution: SE+88.1…SE+217.6 ms = [152850556149, 152980056149] ns, length
   129.5 ms. This is a structural twin of the decode verify pass: a batch < 32 keeps the
   MUL_MAT_ID experts on the CPU (`GGML_OP_OFFLOAD_MIN_BATCH=32`, ggml-cuda.cu:5641), the same
   per-layer GPU clusters, per-layer D2H readbacks, and per-node synchronizations. The trace
   runtime is EXP-041 (ring, without two-thread gather and without fused IQ2_S); neither change
   affects the structure of the decode path (gather is prefill-ring, fused is a variant of the
   same CPU kernel).
   Transfer limitation (explicit): W2 is an approximation of verify, not a measurement of verify.
   Graphs (node shapes and sizes), checkpoint mode (W2 is the tail of a prefill batch after a
   checkpoint break), CUDA-graph capture eligibility, and expert routing may differ. The
   ±20–25% error stated below is a working assumption, not a measured value.
2. EXP-046 microbenchmark (`results/archive/EXP-2026-09-09-046/microbench/`): absolute time of a
   single MUL_MAT_ID call on a real shape (D=2560, NROW=640, NE=512, top-10, 3 rows with one
   shared expert set), 8 threads: OFF 451.8–457.6 µs, ON 441.7–443.7 µs (pair medians).
3. Decode step arithmetic from the EXP-046/047 cards (arm B of EXP-046): 12224.26 ms / 115
   verify graphs = 106.3 ms/step; mean len 2.18; acceptance 0.58974; 234 draft passes over 115
   steps (≈2.03/step).
4. The only kernel-level decode profile is EXP-014/020 (2026-09-02, the configuration BEFORE
   MTP: iq2_s 29.19% + iq4_nl 24.50% + iq3_s 0.81% of cycles; libgomp spin 39.96%, localized in
   the per-node graph barrier). No kernel-level profile of the CURRENT (MTP+fused) configuration
   exists.
5. Closed-out levers: EXP-019 (both kernels at their ceiling: iq2_s at most +20% kernel-time,
   iq4_nl — no headroom in any mode), EXP-021 (iq4_nl fused unpacking is worse even at Ny=8, the
   1.41x ceiling only at Ny=64), EXP-047 (draft threads), EXP-015/016/017/018
   (pool/schedule/wait-policy).

## Decomposition of the W2 window (4-token pass, 129.5 ms, 48 layers)

| Component | Value | Share of window |
|---|---|---|
| GPU kernels busy (union) | 17.920 ms, 4559 kernels, mean 3.93 µs, ≈95 kernels/layer | 13.8% |
| GPU memcpy busy (H2D 69×20.5 MiB + D2H 98×3.4 MiB + D2D 401×35.6 MiB) | 2.380 ms | 1.8% |
| **GPU busy total / idle** | **20.301 / 109.199 ms** | **15.7% / 84.3%** |
| `cudaLaunchKernel` (API, single submit thread) | 4394 calls, 13.530 ms, mean 3.08 µs | 10.4% |
| `cudaStreamSynchronize` | 396 calls (≈8/layer), 6.405 ms, mean 16.2 µs | 4.9% |
| `cudaMemcpyAsync/2DAsync` API | 568 calls, 2.402 ms | 1.9% |
| CUDA-graph activity | 1 instantiate + 1 capture + 1 launch for the whole window | ≈0 |
| Remainder ≈ 95–105 ms — CPU compute and host logic | expert MUL_MAT_ID + ggml barriers + scheduler | 73–81% |

Convergence check: 4394 `cudaLaunchKernel` + 165 `cuLaunchKernel` = 4559 = exactly the number of
GPU kernels — every operation of the pass is launched individually, there is no grouping. D2H 98
≈ 2/layer (ids/activations for the CPU split). Top GPU kernels — `mul_mat_vec_q` Q6_K/Q8_0
(dense, 5.76+2.88 ms), `k_bin_bcast` add/mul (948 kernels, 1.19 ms), `quantize_q8_1` (496,
0.54 ms), `argsort` (0.53 ms), `gated_delta_net` (0.46 ms) — a swarm of micro-kernels,
latency-bound, not FLOP-bound.

## Budget of a verify step of the current runtime (~106.3 ms; transferred from the W2 approximation + microbenchmark; the error is an assumption)

| Block | Estimate | Share of step | Status |
|---|---|---|---|
| CPU MUL_MAT_ID of target experts | ~59–64 ms (96 gate/up calls ≈ 410–445 µs + 48 down ≈ 450–500 µs with nearly full group sharing of 2–3 rows) | ~56–60% | gate/up IQ2_S closed by EXP-046 (+3%); **down IQ4_NL was never optimized** |
| — including ffn_down_exps IQ4_NL | by EXP-014 ≈ 46% of the expert block, by byte arithmetic ≈ 35% | **~20–27% of step** | **was never optimized** |
| Host serialization of the pass (launch chains + syncs + barriers/scheduler) | ≥ 19.9 ms in API-time sums (13.5 launch API + 6.4 sync) + barrier remainder; together with the GPU swarm — a fixed ~35–45 ms per pass | ~24–28% as an upper estimate of presence, up to ~33–42% with the barrier remainder | **never optimized** |
| GPU swarms (dense/router/attn/GDN) | ~17–20 ms busy | ~16–19% | never optimized, but latency-bound (mean 3.93 µs) |
| draft ×2 passes | 3–6 ms | 3–6% | closed by EXP-047 (threads), placement — EXP-023/024 |
| checkpoint saves in decode | ~16.3 ms / 32 tokens | ~1% | Igor's decision of 2026-09-08 — keep |

Consistency: fixed costs ~35–45 ms + expert block ~59–64 ms + draft 3–6 ms ≈ 97–115 ms versus
the measured 106.3 ms/step — the budget converges within the stated error.

Important caveat (Igor's comment): sums of CUDA API times are NOT equal to removable latency.
`cudaStreamSynchronize` may be waiting for useful GPU work; launch time may overlap with kernel
execution. The shares above are an upper estimate of the presence of host serialization in the
step, not a proven lower bound of savings from reducing it.

## Comparison with the registry of closed-out directions

- **ffn_down IQ4_NL (20–27% of step)** — the only large UNtouched operation, but all kernel
  levers are measurably closed: EXP-019 (no headroom in either the L3 or the DRAM mode),
  EXP-021 (hoist unpacking regresses even at Ny=8: 0.19–0.36x; the 1.41x ceiling only at Ny=64
  — not our case). The EXP-046 experience sets an upper bound for "the same trick": in a real
  call, group ALU savings gave only +2.3–2.7% per call (the call is not ALU-bound); for IQ4_NL
  the hoist share of the weight is smaller than for IQ2_S → estimated ceiling <1–2% end-to-end,
  below the 2% threshold. Requantizing the down projection = phase P8 (deferred for quality);
  GPU staging of down in decode — negative arithmetic (9.2 MiB/layer ≈ 0.54 ms H2D at 17.3 GB/s
  versus ~0.5–0.6 ms of CPU work). Conclusion: a noticeable share exists; all kernel levers
  tested so far are exhausted, so the research priority is below host serialization — but the
  data justify a priority, not the exclusion of IQ4_NL from ≥2% candidates in principle
  (Igor's comment: the overly strong "cannot give ≥2%" was withdrawn).
- **Host serialization of the verify pass (≥24–28% floors)** — never optimized once in the
  entire history (all experiments touched prefill-transfer, gather, expert CPU kernels, draft,
  threads). Sub-blocks: (a) 4559 individual launches per pass (13.5 ms API); (b) 396 stream
  syncs (6.4 ms); (c) per-node ggml barriers (by EXP-020 — the only unclosed form of OpenMP
  waiting; 39.96% of cycles in the pre-MTP profile); (d) 98 D2H readbacks of ids/activations in
  the weights branch of the split (ggml-backend.cpp:1643–1727, a full synchronize per node). The
  CUDA-graph infrastructure exists in the tree (USE_CUDA_GRAPH, ggml-cuda.cu:2868–4459), but is
  not active in the W2 window: 1 instantiate per process versus 4559 individual launches in a
  single pass. This does NOT prove the absence of CUDA-graph in real verify and does not
  establish the cause (Igor's comment): the specific capture failure conditions for target
  verify — topology changes between steps, update limits, failed capture, the multi-split
  structure — have not yet been analyzed. Next offline step (no launches): find the failure
  conditions in the code and determine which of them actually hold. Do not remove syncs and D2H
  in the process — they may provide necessary dependencies.
- Everything else is below the threshold or closed: draft 3–6% (EXP-047), checkpoints ~1%
  (Igor's decision), GPU swarms 16–19% busy as a consequence of the structure, not an
  independent limiter.

## Conclusion

1. Based on the available data, two operations with a noticeable share remain in decode that
   were never optimized: (a) `ffn_down_exps` IQ4_NL CPU vec_dot, ~20–27% of step; (b) host
   serialization of the verify pass (launch chains/syncs/barriers/D2H), floors ~24–28% of step.
2. Final wording (accepted by Igor 2026-09-09): the offline analysis singled out host
   serialization as the highest-priority promising hypothesis; its contribution and removability
   in the current decode require confirmation. For IQ4_NL the tested kernel levers are closed by
   direct measurements (EXP-019/021 + the EXP-046 lesson), which gives it a lower priority but
   does not rule out ≥2% in principle. "The single candidate" and "cannot give ≥2%" were
   withdrawn as too strong.
3. Attribution limitations (in summary): (i) W2 is an approximation, not a measurement of
   verify (graphs, checkpoint mode, capture eligibility, routing may differ); (ii) API-time
   sums are not removable latency (sync may wait for useful GPU work, launch overlaps with
   execution); (iii) the ±20–25% error is a working assumption; (iv) no kernel profile of the
   current MTP-decode exists (EXP-014 is pre-MTP). If an unambiguous separation of expert
   kernels / barriers / host code is needed — the option is one CPU profile of the current
   decode (perf with a temporary `perf_event_paranoid≤2`, as in EXP-014/026, separate approval
   from Igor).
4. The next step was assigned by Igor on 2026-09-09 (no launches): find in the current code the
   specific CUDA Graph capture failure conditions for target verify and determine which of them
   actually hold. The potential benefit is ≥2% decode via reducing the CPU cost of launching
   GPU operations. Do not remove syncs and D2H in the process: they may provide necessary
   dependencies.

## Artifacts

`results/archive/EXP-2026-09-09-048/`: `analysis.sql` (exact queries, read-only immutable),
`offline-results.txt` (output). The source is the EXP-043 trace (see above), there are no new
binaries.
