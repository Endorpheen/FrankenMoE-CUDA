# EXP-2026-09-09-046 — fused IQ2_S in MTP verify (decode candidate)

Status: `PLANNED` — a plan without launching the model; execution only after Igor's approval
(rule 1).
Kind: `kind=performance` (a future A/B), the primary metric is decode tok/s.

## Hypothesis

During MTP verification the target model processes a verify batch of 1+1..2 tokens (sampled +
draft, usually 3 rows). In CPU `MUL_MAT_ID` rows are grouped by the chosen expert (`cne1` —
the number of rows of one expert in a call). If 2–3 rows fall on one IQ2_S expert, the
unpacking of its weights can be done once for all rows — saving on the dequant cost of these
groups shortens target verification time and raises sustained decode tok/s. Prefill is not
a goal.

## Novelty (why this is not a repeat of EXP-028)

- EXP-028 (`experiments/rejected/EXP-2026-09-05-028-fused-iq2s-prefill.md`) tested PREFILL on
  a tree before pinned ring. According to EXP-030, bulk prefill is executed by CUDA MMQ, so
  in that experiment the CPU fused path was almost never executed (only the 4-token
  checkpoint tail). The EXP-028 rejection applies to prefill and is not a rejection of
  decode.
- A proper paired decode trial of this path has never happened. The medians 22.17 (control)
  vs 23.90 (fused-retry) tok/s from `results/archive/EXP-2026-09-06-034/raw/exp028/` are a
  side effect of 16-token generation without a paired protocol; they are not declared to be
  a speedup.
- In the decode of the current accepted runtime the fused path executes on every verify
  step: `--cpu-moe` keeps experts on CPU at batch < 32 (EXP-043), the mean MTP acceptance
  length is 2.42 (EXP-023/024 logs) — the verify batch is usually 3 rows, i.e. groups of 2–3
  are possible at every step.
- EXP-021 (`results/exp021-fused-bench.txt`): IQ2_S fused micro-speedup at Ny=2 — 1.36x,
  Ny=4 — 1.84x, maxdiff=0.0. This is the candidate's rationale, not proof of a server-side
  gain.

## Expected benefit

≥5% decode tok/s under identical load (at a nominal 22 tok/s — about 23.1). A utility
criterion, not a forecast. The model, quality, MTP n_max=2, checkpoint, and the accepted
profile are preserved.

## Baseline

- Current runtime: `build/exp045-default-runtime` (default-on pinned ring + two-thread
  gather), launcher `scripts/run_qwen38_server.sh`, THREADS=12, `--cpu-moe`, MTP
  draft-n-max=2, greedy temp=0.
- Saved decode reference points (not directly comparable, different convention and
  binaries): EXP-040 short-prompt decode median 16.0924 tok/s (ring OFF) / 15.6159
  (ring ON); EXP-028 archive 22.17 (control) — 16 tokens, not paired.
- A fresh control A is taken within the A/B itself (rule R2: there is no separate
  baseline experiment).

## The single variable

The fused IQ2_S path for groups of 2–3 rows on the CPU `MUL_MAT_ID` path (target verify),
env switch `GGML_CPU_IQ2S_FUSED` (one binary, OFF = the accepted runtime's behavior).
Unchanged: IQ4_NL (ffn_down_exps), thread count, MTP parameters, offload, quantization,
checkpoint, ring/gather.

## Candidate scope (minimal)

- Source: new directory `work/llama.cpp-exp046-iq2s-verify` = a fresh clone + the chain
  `expert-tier-integration + integration-drift + mtp-sidecar + pinned-ring + pinned-ring-default-on +
  two-thread-gather` (base 4aaad5d3), on top — a port of the EXP-028 fused hunks
  (`ggml_compute_forward_mul_mat_id_iq2s_one_chunk` + `ggml-cpu-iq2s-grid.h`).
- Build: new directory `build/exp046-iq2s-verify`; accepted builds, the launcher, and
  `work/llama.cpp-integration` are not touched. Suspect object files from old builds are
  not used.
- Guard: `type == GGML_TYPE_IQ2_S && cne1 >= 2 && !ggml_is_numa()` (as in EXP-028) plus
  a phase restriction `src1->ne[1] >= 2 && src1->ne[1] <= 3` (the verify ubatch size at
  n_parallel=1; the prefill checkpoint tail with ne[1]=4 is thereby excluded). Env is read
  once (in EXP-028 there was a `getenv` on every call — the port fixes this). The
  single-row path (cne1==1) stays stock vec_dot.
- Draft is excluded by type: in the MTP draft
  (models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf) the blk.48 experts are Q4_K/Q8_0,
  not IQ2_S; the type guard lets nothing from the draft context into fused, including
  catch-up passes with a whole batch. The batch-size restriction is not identified with the
  phase: verify attribution is confirmed by a counter (Correctness step 3) over ne[1] and
  cross-checked against the number of verify steps in the slot log.
- Applicability by coverage: IQ2_S = ffn_gate_exps + ffn_up_exps (24.08 GiB of ~46 GiB of
  expert weights); ffn_down_exps — IQ4_NL (21.6 GiB) — outside the candidate; layer-2
  gate/up — IQ3_S — outside the candidate. The batch-size guard is not identified with the
  verify phase: the phase is confirmed by the counter (see Correctness), not by the guard.

## Port verification (result of comparing EXP-028 ↔ the current runtime)

- The region in `ggml-compute-forward_mul_mat_id` in the current tree is identical to the
  EXP-028 base (the hook `g_expert_ready_hook` is present in both) — the hunks port without
  conflict.
- Strides/row mapping: fused uses the same `(i11 + i12*ne11)*row_size` and
  `MMID_MATRIX_ROW` as the stock path with `wdata` converted to contiguous Q8_K (for IQ2_S,
  f32 src1 is always converted — the strides branch is not needed). The `dst_col[ir0]`
  write with stride 1 — the same assumption as stock (dst mul_mat_id contiguity assert).
- Scratch: no additional allocations; the wdata layout does not change; the atomic chunk
  counters are reused (`atomic_current_chunk + cur_a`); fused splits only nr0 (for gate/up
  ne01=640, 40 chunks across 12 threads) — compatible with the current worker scheduling.
- The numerical order of accumulation differs from stock vec_dot (one float accumulator per
  row versus per-subblock ones); EXP-021 gave maxdiff 0.0 on the same shapes. The
  correctness gate requires matching the control and investigating any divergence (do not
  write it off as a near-tie).
- No interaction with the pinned ring: the ring stages CUDA experts; CPU experts read the
  GGUF mmap directly.

## Correctness plan (before performance)

1. A model-free unit gate on real Qwen shapes (ne00=2560, ne01=640, 512 experts,
   top-k=10, rows=1/2/3): sparse/repeated expert IDs (including id=0 and id=511),
   comparing ON vs OFF output (exact match; otherwise — investigate), at 1 and 12 threads;
   check the NUMA guard on the current machine.
2. Cross-check of the `iq2s_grid` value from `ggml-cpu-iq2s-grid.h` against the array in
   `quants.c` (byte-identical).
3. An env-gated counter of fused calls + a `cne1` (1/2/3+) histogram by phase; one short
   run (a 395-token request, n_predict=64, temp=0) confirms hits specifically in the verify
   phase (decode steps), separate from the prefill checkpoint tail. At the same time this
   is a minimal measurement of the share of eligible verify groups — the only missing
   observation (the EXP-029 histogram refers to the 4-token checkpoint tail and does not
   carry over to decode).
4. Model determinism: identical requests, temp=0, identical seed; ON vs OFF — matching
   response tokens and MTP acceptance/rollback counters; investigate any divergence.

## Decode A/B plan (after the candidate and gates are ready; launch only after rule 1)

- One binary, OFF/ON; identical prompt, cache policy, sampling (temp=0), MTP n_max=2.
- Warmup separately. A new fixed input `benchmarks/inputs/exp046-decode.json`
  (probe: ≥256 actual tokens without early EOG; otherwise replace the request).
- The target is 256 actually generated tokens; early EOG/empty response — the probe is
  invalid.
- One preliminary pair; the number of further pairs to be agreed based on its result
  (for ACCEPT — at least 5 pairs per the roadmap rule).
- Primary metric: decode tok/s under the actual-token-counts convention
  (`tokens_generated / (predicted_ms/1000)`), identical for both arms. Additionally:
  decode time, generated tokens, draft/accepted counts, CPU, RSS, VRAM, VmSwap, sha256 of
  responses. Prefill — record separately, do not include in the primary gain.
- Measurements are manual, strictly one at a time; profiled times must not be used as
  performance.

## Criteria

- ACCEPT: decode median ≥ +5% on paired data (≥5 pairs, a majority of pairs in favor of
  ON), correctness passed (unit gate + matching responses/acceptance), no memory/swap
  regressions.
- REJECT (including without a model A/B): if the step 3 histogram gives a share of IQ2_S
  rows in groups ≥2 below ~20%, or the unit gate/micro level does not confirm the saving,
  or the overhead (counter, branch) eats the gain. There is no automatic search over other
  settings.
- Changing the default runtime is a separate decision of Igor.

## Risks

- A 3-row verify batch yields groups ≥2 only when neighboring tokens' routes match; the
  share of eligible groups in decode is unknown (no data — covered by step 3 before the
  performance A/B).
- CPU experts in decode are partly bandwidth-bound: the ALU saving (1.36x per dot at Ny=2)
  may not convert into wall-time.
- Difference in accumulation order → token divergence under greedy; must be investigated.
- EXP-028 legacy: CUDA-graph OOM on tight VRAM — fused does not touch the CUDA path, but a
  full correctness run of the profile is mandatory.
- A 2–3-token prompt/resume ubatch formally matches verify by shape — hitting fused is
  numerically harmless (exact-match gate), but phase attribution rests only on the counter;
  if the counter disagrees with the number of verify steps, do not accept the candidate.
