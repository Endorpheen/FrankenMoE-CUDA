# EXP-2026-09-09-046 — fused IQ2_S mul_mat_id: model A/B pairs

Kind: `model-ab` (technical assignment from the senior helper in Codex dated 2026-09-09; the
coordination block approved by Igor on 2026-09-09 — an explicit «да» ("yes") for the pair +
diagnostic start; after that, all pairs are direct instructions from Igor on 2026-09-09:
pairs 2–3 B→A, pairs 4–6 A→B, pairs 7–8 B→A «чтобы было 4 таких 4 таких» ("so that there
would be 4 of each, 4 of each") — a balanced plan of 4 A→B + 4 B→A). Utility threshold per
Igor's decision: decode gain ≥2% tok/s. Prefill is not a goal.

## Hypothesis

Reusing unpacked IQ2_S weights when experts match across MTP verify tokens (batches of
2–3 rows) speeds up decode. At 22 tok/s the 2% threshold ≈ 0.44 tok/s — a utility criterion,
not a forecast.

## Candidate (readiness verified without launching the model)

- Tree `work/llama.cpp-exp046-iq2s-verify` = the accepted runtime (base `4aaad5d3` + the
  expert-tier chain + EXP-045 two-thread gather) + the fused delta in exactly 4 runtime
  files: `ggml/include/ggml-cpu.h`, `ggml/src/ggml-cpu/ggml-cpu.c`, new
  `ggml/src/ggml-cpu/ggml-cpu-iq2s-grid.h` (md5 `93859f83d87ab784ad27508286ec3123`),
  `tools/server/server-context.cpp` (window opening only on a decode-only batch view,
  the phase is taken from the explicit per-token is_prompt, not from the batch size) + tests/
  (gate test and wall-clock bench). `ggml-cpu.c` md5 `9fa2bb5e7aa2935e10de09b0bdf53074`.
- CUDA build `build/exp046-cuda-ab` (Release, arch 89, NCCL, shared), BUILD_EXIT=0. Hashes:
  `llama-server` `1bf46dc5fa2d9ab6ee5cbd696ca0021e9a5b58566762367272f6a2d5b0e404c8`,
  `libggml-cuda.so.0.22.0` `bb67604bb72977bc4a6a694fb46ec3ab27dc8f0ede8450fc2950ef6af40013d8`,
  `libggml-cpu.so.0.22.0` `8c8268201a45f119bc98fe600c7a09f0ab2a23f5471161938647e92d8802dc4c`.
- The switch (verified from code, not from documentation): env `GGML_CPU_IQ2S_FUSED` —
  unset or "0" = OFF, anything else = ON; the value is cached by the library on first
  access → set it BEFORE process start. An additional gate — the
  `ggml_cpu_iq2s_fused_set_window` window, which in the server opens only for batches
  where all rows are in the decode phase (MTP verify, 2–3 tokens); prefill always bypasses
  fused.
- Correctness: gate test `test-iq2s-fused-mmid` FROM THIS CUDA build — ALL PASS,
  `fused_dispatches=51`; in the gate, ON is byte-for-byte equal to OFF (window toggling),
  threads do not affect the result.

## Proof of fused activation in the server path (diagnostic start before the pair)

`GGML_CPU_IQ2S_FUSED=1 GGML_CPU_IQ2S_FUSED_STATS=1`, one warmup request (41+104 tokens),
without measurements: `fused_groups=[0, 21374, 9964, 0]`, `window_opens=56`,
`fused_dispatches=376056`, clean SIGINT (exit 0). 96–98% of multi-token verify-decode
groups went into the fused path — coverage in real decode is almost complete, unlike in
the synthetic microbench (see below).

## CPU microbench (context; a separate series from the same day)

One MUL_MAT_ID call (src0 IQ2_S 2560×640×512, src1 f32 2560×rows, ids 10×rows, seed 42,
10 warmup + 100 measurements per arm, 3 OFF/ON pairs, AMD Ryzen 9 5950X, gcc 15.2.0):
- group 3 (rows=3, a shared expert for all rows): +1.8…+4.0% across pairs, medians 1.0232x
  (4 threads) / 1.0274x (8 threads);
- group 2 (rows=2): neutral, medians 0.9940x / 0.9938x;
- 2+1 split: from −0.5% to −2.5% (medians 0.9948x / 0.9771x).
In a single call fused covered 1 group out of ~19 experts; in real verify-decode coverage
is almost complete — this explains the larger server-side effect. Full logs:
`results/archive/EXP-2026-09-09-046/microbench/`.

## Pair protocol

One binary, one command (Igor's profile: -ngl 99 --cpu-moe, ctx 196608, KV q4_0, -np 1,
-t 12, MTP n_max=2 draft-cpu, EHS=0, --jinja, reasoning-effort low, port 8081). A: env=0;
B: env=1. In each arm: start → listening → warmup (`request-short.json` from EXP-040,
sha256 `4d1597f5…`) → measurement (`request-decode256.json`, sha256
`5975261ba9441955f53227d8c7784b176c7fbcd96d5661040d50374f4655d2ac`, temperature=0,
n_predict=256, cache_prompt=false, stream=false) → clean SIGINT. Profiler and diagnostic
counters are turned off in A/B. Strictly sequential, no parallel tasks.

## Results

A series of 8 pairs, balanced by order: 4 pairs A→B (1, 4, 5, 6) and 4 pairs B→A (2, 3, 7,
8); all — one binary, argv, requests, protocol; the arms are switched only by env before
process start:

| Pair | Order | A (OFF), tok/s | B (ON), tok/s | Pair gain |
| --- | --- | --- | --- | --- |
| 1 | A→B | 16.448117 | 20.653632 | +25.57% (machine anomaly on A, see below) |
| 2 | B→A | 20.238738 | 20.860160 | +3.07% |
| 3 | B→A | 19.882224 | 20.680121 | +4.01% |
| 4 | A→B | 20.510737 | 20.856941 | +1.69% |
| 5 | A→B | 20.563762 | 20.635405 | +0.35% |
| 6 | A→B | 20.411659 | 20.813188 | +1.97% |
| 7 | B→A | 20.281859 | 20.874860 | +2.92% |
| 8 | B→A | 19.980582 | 20.817319 | +4.19% |

Gain summary (100×(B/A−1) per pair):

- all 8 pairs: median **+3.00%**, mean +5.47% (skewed by the pair 1 anomaly), min +0.35%,
  max +25.57%, B wins **8/8** (one-sided sign test p = 0.0039);
- clean 7 pairs (without anomalous pair 1): median **+2.92%**, mean +2.60%, B wins 7/7;
  independent arm medians: A 20.282 / B 20.817 → **+2.64%**;
- by orders: B→A pairs (2,3,7,8) +3.07/+4.01/+2.92/+4.19 (median +3.54%); clean A→B
  pairs (4,5,6) +1.69/+0.35/+1.97 (median +1.69%). Arm B is stable (20.64–20.87 in all
  8), arm A varies more strongly (19.88–20.56) and is slower in the pairs where it runs
  second after B (mean A-second 20.10 versus A-first 20.50). If A slows down when running
  second after B, this increases the measured gain of B, rather than decreasing it: the
  effect magnitude depends on order. "Cold B versus warm A" without confirming telemetry —
  an assumption, not a fact.

Controls across all 16 runs: 256/256 tokens stop=limit; content sha256 `9270353f0601d5d6…`
identical in all arms; draft acceptance identical down to the digit (measured 0.58974
138/234 mean 2.18, warmup 0.41964 47/112 mean 1.84); expert ring staged 25326/25326;
VmSwap=0; VRAM 10608–10807 MiB; RSS 37 214–37 217 MiB in both arms (the pair 1 difference
did not reproduce — page cache); clean SIGINT exit ~3 s, no orphans. Warmup 104-token
decodes of pairs 4–8: A 16.16/16.99/16.66/16.92/15.89, B 16.89/16.94/17.05/16.87/16.72
tok/s.

## Validity and limitations

- Correctness: the response text is byte-for-byte identical between A and B in all 8 pairs;
  draft acceptance identical; ring counters identical; VmSwap=0; no CUDA errors (the only
  line containing error/abort in the logs is the known harmless warning
  `common_fit_params`, as in all healthy launches of this profile).
- The measured requests are valid: 256/256 tokens, stop=limit (sustained decode, EOG did
  not fire) in all 16 runs.
- The order is balanced (4 A→B + 4 B→A), B wins under both orders, but a dependence of
  effect magnitude on order is present: in B→A pairs the gain is larger, and the slowdown
  of A running second increases the measured gain of B. Final wording: the observed decode
  gain is about 3% on this workload; B is faster in all pairs; a dependence of effect
  magnitude on order is present.
- The arm A anomaly of pair 1 (16.45 tok/s) is confirmed as the machine's state at that
  moment, not a property of the arm: arm A of pairs 2–8 gave 19.88–20.56 tok/s. The pair 1
  gain (+25.57%) is not used as an effect estimate.
- The RSS difference of pair 1 (+0.8 GiB in B) did NOT reproduce: in all runs of pairs 2–8
  RSS is 37 214–37 217 MiB in both arms — confirmed as page cache warmth, not fused
  allocations.
- Mechanistically the effect is plausible: the measured request has acceptance 0.59 and
  mean len 2.18 → almost all verify batches are 3-token → almost all expert groups go
  through the fused path, whereas warmup has acceptance 0.42 → the effect is more modest
  (warmup decodes barely separate).
- Limitations: all 8 pairs within one day on one machine; one request (256 tokens,
  temperature 0); the effect scale (~3%) is comparable to the variability of arm A
  (19.88–20.56), so the sign is robust (8/8, p = 0.0039), while the point estimate has an
  uncertainty of about ±1%; the estimate by independent arm medians is +2.64%.

## Verdict

CONFIRMED POSITIVE: B wins **8/8** pairs in the balanced plan of 4 A→B + 4 B→A
(one-sided sign test p = 0.0039); median gain **+3.00%** across all 8 pairs and **+2.92%**
across the clean 7; by independent arm medians **+2.64%**. The real effect scale is
**about +3% decode tok/s** (not +25.6% of the anomalous pair 1), consistent with the CPU
microbench (+2.3…+2.7% per group-3 call with almost complete group coverage in real
verify-decode). Igor's 2% utility threshold is exceeded under all aggregation variants;
the common decision-rule requirement of "at least five consecutive pairs" is exceeded
(8 pairs). The decision to port into the baseline (export the patch into the chain, build
the default runtime, switch the launcher) and to commit — only by a separate decision of
Igor.

## Artifacts

`results/archive/EXP-2026-09-09-046/`:
- `diag/` — server.log (including the IQ2S_FUSED_STATS dump), warmup-response.json;
- `ab/` — pair 1: `A-*`/`B-*` (server.log, warmup.json, measured.json, telemetry.txt),
  `env-argv.txt` (argv identical to all subsequent runs);
- `ab/pair2-B/` … `ab/pair8-A/` — server.log, warmup.json, measured.json, telemetry.txt,
  env-argv.txt for each arm of pairs 2–8 (order in the directory name: pairN-A / pairN-B);
- `hashes-preflight.txt` (hashes re-verified before pairs 2–3, 4–6 and 7 — byte-for-byte;
  no rebuilds were performed between pairs 7 and 8), `request-decode256.json`;
- `microbench/` — smoke + 6 logs of the CPU series (bench binary `build/exp046-speedtest`,
  md5 `a0e67ae4af9d62da8c1bafd92c469e65`; gate `24aaa3c20813c17853d41147ab22b3d9`).
