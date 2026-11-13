# EXP-2026-09-09-047 — a separate CPU thread count for MTP-draft (A/B)

- Status: `REJECTED` (`kind=model-ab`, 2026-09-09; the senior helper's assignment; Igor's
  approval for pairs 1–2 (4 runs) and, after the first report, his direct instruction
  «еще по разу AB и BA» ("one more A→B and one more B→A pair") for pairs 3–4; 8 server
  runs in total within a single day, a balanced plan of 2 A→B + 2 B→A).
- Hypothesis: the single-token split-MTP draft pass (1 MoE layer, a batch of 1 token,
  ~2 passes per verify step) spends extra time synchronizing 12 CPU threads; lowering
  the draft thread count to 6 speeds up the preparation of proposed tokens, potentially
  ≥2% of decode. Target-verify stays on 12 threads with the accepted fused IQ2_S. This
  is a hypothesis, not a promised gain.

## Novelty (step 1)

Not tested before per history: EXP-013 measured `-t 12` vs `16/24/32` only for the
target context; EXP-022/023/024 — draft sources and placement (ngram, CPU-sidecar,
split GPU); EXP-017/018 — process-global OMP policies. A grep over all cards (including
`experiments/rejected/`) for `threads-draft / n_threads_draft` — empty.

## Runtime support (step 2)

The separate setting exists in the stock binary; no rebuild is needed:

- `common/arg.cpp:4070` — `--spec-draft-threads, -td N` (and `-tbd` for the batch ones);
  writes only to `params.speculative.draft.cpuparams`.
- `common/speculative.cpp:2517` — `common_base_params_to_speculative` copies the target
  parameters into the draft context and overrides them only when a positive `-td` is
  given; the default is `common_cpu_params.n_threads = -1` (`common/common.h:69`), so
  without the flag draft inherits 12.
- `tools/server/server-context.cpp:1104,1128` — the target context is built from
  `params_base` independently and before the draft branch; `-td` cannot affect target.
- `src/llama-context.cpp:1444,2557` — `batched = ubatch.n_tokens > 1`: single-token
  draft passes take `n_threads` (`-td`), batch ones `n_threads_batch` (`-tbd`; when
  unset = `-td`, which is why arm B adds `-tbd 12`, pinning today's effective value).

An activation nuance: the draft context is created directly via `llama_init_from_model`
(speculative.cpp:2609/2625) and does not log its own thread count; server.log shows only
the target threadpool (`llama threadpool init, n_threads = 12`). The proof of the
plumbing is the code chain above + the absence of flag parsing errors.

## Pre-launch estimate of the draft share (step 3, offline from saved data)

- The decode step (EXP-046 arm B): 12224.26 ms / 115 verify graphs = 106.3 ms/step;
  draft passes 234/115 ≈ 2.03, each 1 token.
- Pass cost: ~37 MiB Q4_K_M for the top-10 draft experts (EXP-024); the bandwidth
  reference — a batch pass of 45.1 ms / 391 rows (EXP-043, tail attribution, item 6)
  ≈ 15 GB/s effective → 1.2–2.5 ms/row + fixed overhead of 0.3–1 ms.
- Draft share ≈ 3–6% of step time; the full-success ceiling ≈ +1.5–3% of decode, a
  realistic estimate ≈ +1% — below the 2% threshold. The risk is symmetric (a loss of
  streaming throughput on 6 threads → −1–2%).

The choice of one value: 6 (not 4) — the dot draft is bandwidth-bound; 4 is closer to
the throughput-loss regime, 6 halves the OMP team while keeping bandwidth headroom. No
sweep over values was performed.

## Protocol

Against EXP-046: one binary `build/exp046-default-runtime/bin/llama-server` (sha256
`3b109f0f80e56e4b6631eba8cee451ea89b0cfe16e17ea0383cc324e5090c2ec`, libggml-cuda
`59259256…`, libggml-cpu `98f2cd6c…`); the argv of the EXP-046 pair verbatim (see
`results/archive/EXP-2026-09-09-047/ab/env-argv.txt`), clean env (fused IQ2_S ON by
default, ring ON by default). Arm B = the same argv + `--spec-draft-threads 6
--spec-draft-threads-batch 12`. Warmup with request-short.json (EXP-040) + measurement
with request-decode256.json (sha `5975261b…`, temp 0, n_predict 256, cache_prompt
false), endpoint `/completion`, port 8081, telemetry every 0.5 s from `/proc/<pid>`.
Order: pair 1 A→B, pair 2 B→A (approval); pairs 3 A→B and 4 B→A — per Igor's direct
instruction after the first report. Threshold ≥2%.

## Results

| Arm | Config | decode tok/s | ms/256 | prompt tok/s | warmup tok/s |
|------|--------|--------------|--------|--------------|--------------|
| A1 | draft 12 (inherited) | 15.90 | 16037.77 | 3.35 — anomaly | 9.02 |
| B1 | draft 6 | 18.93 | 13472.06 | 34.79 | 16.04 |
| B2 | draft 6 | 19.97 | 12770.26 | 35.62 | — |
| A2 | draft 12 (inherited) | 20.14 | 12660.65 | 36.36 | — |
| A3 | draft 12 (inherited) | 20.60 | 12381.35 | 37.77 | — |
| B3 | draft 6 | 20.15 | 12654.15 | 36.02 | — |
| B4 | draft 6 | 20.12 | 12675.29 | 36.86 | — |
| A4 | draft 12 (inherited) | 20.28 | 12573.07 | 37.01 | — |

- Pair 1 (A→B): +19.1% — NOT interpretable: arm A1 was contaminated by a machine cold
  anomaly (prompt eval 3.35 tok/s — ~10 times below normal, warmup 9.02 vs 16.04 for
  B1; the same class of anomaly as pair1-A of EXP-046, where 16.45 was attributed to
  the machine). Not included in the estimate; B1's 18.93 is recorded but does not enter
  the clean arm medians (the arm ran immediately after the anomalous A1).
- Clean pairs (the same warm status, consecutive):
  - pair 2 (B→A): B 19.97 → A 20.14, candidate delta **−0.85%**;
  - pair 3 (A→B): A 20.60 → B 20.15, delta **−2.18%**;
  - pair 4 (B→A): B 20.12 → A 20.28, delta **−0.79%**.
- Median of the clean pairs **−0.85%**; B is slower in **3/3** clean pairs (one-sided
  sign test p = 0.125). Independent medians of the clean arms: A {20.14, 20.60, 20.28}
  → 20.28; B {19.97, 20.15, 20.12} → 20.12 → **−0.79%** (all four B arms: median
  20.05 → −1.13%). The direction is against the hypothesis in all aggregations; the
  magnitude is on the boundary of / within daily noise.
- Arms A2–A4 (20.14–20.60) are inside the historical A range of EXP-046 (19.88–20.56,
  median 20.282 — a match to hundredths); arms B2–B4 (19.97–20.15) are below the
  historical B of EXP-046 (20.64–20.87); the cross-day comparison is not used, the
  conclusion is by the intra-day pairs.
- The ≥2% threshold was not reached; the sign is systematically negative.

## Correctness (all 8 arms)

- 256/256, stop=limit; content sha256
  `9270353f0601d5d660a61ee72b94ece46c5d7ed425321da8e88bd09f1abd9757` identical in all
  arms and matching EXP-046.
- Draft acceptance is identical to the digit: measurement 0.58974 (138/234, mean 2.18),
  warmup 0.41964 (47/112, mean 1.84); graphs reused 56/171.
- Ring staged 25326 calls / 25326 chunks in every run.
- VmSwap=0 in all telemetry slices (arms B1/B2/A2/B3/A3/B4/A4); A1's telemetry by
  mistake monitored the launch bash wrapper instead of the server pid — A1's VmSwap is
  not confirmed telemetrically, indirectly: no watchdog warnings in the log.
- Clean SIGINT (exit 0), no orphans and no CUDA errors; the VRAM profile is unchanged.

## Verdict and consequences

`REJECTED`: separately lowering the draft CPU thread count to 6 does not speed decode
up but slightly slows it down (clean-pair median −0.85%, B slower in 3/3 clean pairs,
independent medians −0.79%), which agrees with the offline estimate (draft share ≈
3–6% of the step, ceiling ≈ +1.5–3% at full halving, realistically +1%) and indicates
a small loss of streaming throughput on 6 threads instead of a synchronization win.
The runtime does not change: the `-td/-tbd` flags remain available upstream as opt-in
CLI options, are not added to the launcher, and the default (draft inherits `-t 12`)
is preserved. No code changes; memory is not affected.

The hypothesis is closed: do not tune the draft thread count separately (downward was
tested at 6 in 4 pairs; a 4/8 sweep was not performed — the effect window is already
below the threshold, the direction is negative).

## Artifacts

`results/archive/EXP-2026-09-09-047/ab/` — env-argv.txt (the full protocol and hashes),
pair1-A/, pair1-B/, pair2-B/, pair2-A/, pair3-A/, pair3-B/, pair4-B/, pair4-A/
(server.log, telemetry.txt, warmup.json, measured.json in each).
