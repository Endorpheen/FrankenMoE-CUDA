# EXP-2026-09-09-052 — L2-only implementation: isolated CUDA Graph variants (VARIANTS=1/2)

- Status: `DONE` — A/B completed, performance candidate `REJECTED` (implementation+model-free-gate
  2026-09-09: L2-only, default `GGML_CUDA_GRAPH_VARIANTS=1`, model-free gate 32/32, memory
  measured; model A/B of 8 pairs = 4×A→B + 4×B→A per Igor's instruction of 2026-09-09 «запускай»
  ("go ahead") + «4 раза AB и 4 раза BA» ("4 times AB and 4 times BA"), 16 server runs with a
  single binary, all arms sequential).
- Tree: `work/llama.cpp-exp052-variants` (a copy of the exp046 baseline + only the EXP-052
  changes); build: `build/exp052-variants` (Release, GGML_CUDA_GRAPHS=ON). Do not touch L1.
- Implementation (per the EXP-051 decision):
  - the value of the `cuda_graphs` map — `ggml_cuda_graph_entry` (per-key: arch-flag, last_used,
    diagnostics counters, `slots`), `common.cuh:1266-1278`;
  - a slot = the former `ggml_cuda_graph` (graph/instance/warmup_complete/uid/last_used/node_props),
    `common.cuh:1238-1263`; the per-key slot limit — env `GGML_CUDA_GRAPH_VARIANTS` (default 1,
    clamp 1..8), `ggml-cuda.cu:2930-2943`;
  - slot selection: a clean scan of memcmp snapshots (`ggml_cuda_graph_slot_matches`,
    ggml-cuda.cu:2946), then a new slot if there is headroom, then an under-warmed one, then LRU;
    compatibility — the former byte-for-byte memcmp of `node_properties` (the whole node's
    ggml_tensor + src data/ne/nb), no hashes;
  - reset/capture/launch — on the selected slot; a neighbor's snapshot is not overwritten;
    ExecUpdate-fail — destroy+re-instantiate of its own slot only (seam
    `GGML_GRAPH_TEST_FAIL_UPDATE`);
  - the 10 s sweep — per key; destroy — per slot; `set_enabled`/`update_required`/
    `update_executable` moved to the slot-based API; `graph_compute`/`evaluate_and_capture` take
    the selected slot.
- Model-free gate: `results/archive/EXP-2026-09-09-052/` (`exp052_gate.c`,
  `run-model-free-exp052.sh`, logs `gate-*.log/.out`, `SHA256SUMS.txt`). The GPU backend is live,
  no models. Result: **32/32 PASS** (after two harness fixes along the way, see below).
- Scenarios and results (each — against the control path `GGML_CUDA_DISABLE_GRAPHS=1`, round
  checksums match in all scenarios):
  - S1 alt (1->3->1->3 on one key): VARIANTS=2 — both slots reach launch, exactly 2 captures, no
    warmup reset after warmup; VARIANTS=1 — the EXP-050 trap reproduced exactly (0 launches, a
    reset/unstable/capture cycle).
  - S1b rebuild: the ctx is recreated on the same metadata buffer — 1 capture, then launch only
    (key/address lifetime through llama reset() confirmed on a synthetic test).
  - S2 third (third shape 7): LRU eviction, >=3 captures, both original shapes return to launch
    on their own slots; recovery of the evicted slot is complete (reset -> capture -> launch — a
    harness sequence fix, the first version broke off at capture).
  - S3 shift: a change of metadata pointers with the same shape -> a new key, first_call+capture,
    launch on both keys.
  - S4 failupd (seam ExecUpdate): the marker exactly 1 time; after the failure the neighboring
    slot continues to launch, the recreated slot reaches launch on its own (checked by line order
    in the log). Important: the snapshot does not see the renaming of a SRC tensor (the snapshot
    holds only src data/ne/nb — upstream node_properties semantics); the trigger was made the
    rename of the node's own tensor (`node[0].name`).
  - S5 sweep: after 11 s of idling the key was reset (first_call anew), re-convergence to
    launch, clean teardown (exit 0).
  - OFF: without `GGML_GRAPH_DIAG` — zero diagnostics lines, checksums match.
- Note on the state machine: the natural (not seam) `event=exec_update_failed` fired 2 times in
  S2 — a real ExecUpdate failure when re-capturing another shape into an occupied slot; the
  destroy+re-instantiate fallback worked, checksums matched the control.
- Actual memory (measured, not an estimate):
  - host: `event=slot_struct bytes=96` + a snapshot of 1056 B/node (`event=sizeof_node_props`).
    The additional VARIANTS=2 slot for one draft key: 96 + 116x1056 = 122 592 B; the second key:
    96 + 63x1056 = 66 624 B; total extra host for the two draft keys = 189 216 B (~185 KiB).
    For target splits — only if a second variant actually takes hold for the key ( upper bound
    per split: 96 + 1056 x n_nodes of the split ).
  - device: the `mem` gate (100 keys, 4 calls per key): memory taken by graphs ctl=2 097 152 B;
    VARIANTS=1 (100 instance) = 10 485 760 B; VARIANTS=2 (200 instance) = 20 971 520 B.
    Extra instance+graph = 10 485 760/100 = ~104.9 KiB ( cudaMemGetInfo granularity 2 MiB ->
    a range of ~84-105 KiB per instance for a 1-node graph). For the draft, an extra 2 instance
    ~ 0.2 MiB VRAM against a background of 10.6 GB; the instance cost for 63/116-node graphs is
    to be checked in the A/B (cudaMemGetInfo before/after + slot counter).
- Result: L2-only is implemented, default VARIANTS=1 is byte-for-byte compatible with the old
  behavior (the EXP-050 trap reproduced), VARIANTS=2 gives coexistence of shapes 1/3. Ready for
  a preliminary A/B under the EXP-046 protocol — launch only with Igor's approval.
- Key hashes (full list — `results/archive/EXP-2026-09-09-052/SHA256SUMS.txt`):
  ggml-cuda.cu `27c2ebdd…`, common.cuh `3ba28d50…`, libggml-cuda.so `74573811…`.

## Model A/B (EXP-046 protocol, 8 pairs: 4×A→B + 4×B→A)

- Approval: Igor, 2026-09-09 («запускай» ("go ahead") — the preliminary pair; «4 раза AB и
  4 раза BA» ("4 times AB and 4 times BA") — the full plan). Measurements manual and strictly
  one arm at a time; GGML_GRAPH_DIAG off.
- Binary `build/exp052-variants`, the same one in both arms. The only variable is the env
  `GGML_CUDA_GRAPH_VARIANTS`: A=1 (old behavior), B=2. Both arms: `GGML_CPU_IQ2S_FUSED=1`
  (the accepted EXP-046 default). Port 8081.
- Arm order: server start -> listening -> warmup `request-short.json` (n_predict=128,
  temperature=0, cache_prompt=false) -> measured `request-decode256.json` (n_predict=256) ->
  telemetry (VmRSS/VmSwap, VRAM with the process alive) -> SIGINT -> clean-exit check
  (ring staged, graphs reused, prompt eval line).
- Metric: `timings.predicted_per_second` from measured.json (decode tok/s); correctness —
  content sha256 (python hashlib over the raw `.content`), draft_n/accepted, stop=limit, ring,
  VmSwap.
- Artifacts: `results/archive/EXP-2026-09-09-052/ab/pair{1..8}-{A,B}/`.

| Pair | Order | A tok/s | B tok/s | Δ=(B−A)/A | sha A= B | draft A/B | ring A/B | Note |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | A→B | 16.758 | 19.014 | +13.46% | yes `9270353f…` | 138/234 = | 25326/25326 = | warmup-wall A 21.88 s versus B 11.95 s, prompt A 3656 versus B 1984 ms — arm A ran on a cold machine (the cold anomaly pattern of EXP-046/047 pair1); a candidate for exclusion from the clean medians |
| 2 | A→B | 20.996 | 20.843 | −0.72% | yes | = | = | machine warm (warmup 7.67/7.53 s, prompt 1057/1093 ms); VmSwap 0/0 |
| 3 | A→B | 20.887 | 20.749 | −0.66% | yes | = | = | warmup 7.66/7.56 s, prompt 1076/1077 ms; VmSwap 0/0 |
| 4 | A→B | 20.770 | 21.064 | +1.41% | yes | = | = | warmup 7.51/7.55 s, prompt 1091/1078 ms; VmSwap 0/0 |
| 5 | B→A | 20.844 | 21.061 | +1.04% | yes | = | = | B ran first (21.061), A second 20.844; warmup 7.80/7.67 s; VmSwap 0/0 |
| 6 | B→A | 20.753 | 20.782 | +0.14% | yes | = | = | B ran first (20.782), A second 20.753; VmSwap 0/0 |
| 7 | B→A | 21.110 | 20.967 | −0.68% | yes | = | = | B ran first (20.967), A second 21.110; VmSwap 0/0 |
| 8 | B→A | 20.709 | 20.277 | −2.09% | yes | = | = | B ran first (20.277 — the lowest B arm; VRAM during the arm 11257 MiB, the desktop ate more than usual), A second 20.709; VmSwap 0/0 |

- Aggregates (full values — `predicted_per_second`):
  - clean 7 pairs (without pair 1): deltas −2.089 / −0.725 / −0.674 / −0.662 / +0.139 / +1.040 /
    +1.416 → **median −0.66%**, mean −0.22%, B wins **3/7**;
  - by order: A→B clean median −0.66% (3 pairs), B→A clean median −0.27% (4 pairs) — no
    asymmetry against the candidate, and no effect either;
  - independent medians of clean arms: A 20.843701 / B 20.843498 → **−0.001%** (a statistical
    zero);
  - all 8 pairs: median −0.26%.
  - arm spread: A 20.709–21.110, B 20.277–21.064.
- Correctness: all 16 runs 256/256 stop=limit, content sha256
  `9270353f0601d5d660a61ee72b94ece46c5d7ed425321da8e88bd09f1abd9757` identical and equal to
  EXP-046/047, draft 138/234 (0.58974) in all arms, ring 25326/25326, graphs reused 56→171,
  exit by SIGINT ~4 s, no orphans. VmSwap 0 in all arms except pair1-B (130 024 kB — desktop
  pressure, the variant's host cost ~185 KiB).
- Pair 1 (+13.46%) — a confirmed cold anomaly of arm A (warmup 21.88 s versus 7.5–8.0 s for all
  other arms, prompt 3656 ms versus ~1080 ms, the arm itself 16.76 versus 20.71–21.11 of the
  other A arms); the same pattern as EXP-046 pair1 / EXP-047 A1. Not included in the clean
  aggregates.
- A/B result: **the L2-only two-variant effect on decode in the server profile was not
  confirmed** — clean-pair median −0.66%, independent medians −0.001%, the ≥2% threshold not
  reached, the sign negative. The EXP-051 estimate (+1.4–1.7% locally from saving 1.64 ms
  +capture on a 106 ms step) did not materialize on the server: the likely reason — the saved
  1.6–2 ms/step do not sit on the critical path (the step is limited by CPU serialization and
  MUL_MAT_ID, EXP-048), and/or the share of captures in real traffic is below the model
  estimate.
- Verdict: `REJECTED` as a performance candidate. Default `GGML_CUDA_GRAPH_VARIANTS=1` stays;
  the VARIANTS=2 code is correct (byte-for-byte the same output, the EXP-050 trap is removed)
  and remains opt-in in the `work/llama.cpp-exp052-variants` tree; it is not carried into the
  baseline/launcher. L1 was not added (an EXP-051 condition).

- Pair 1, details: A — VmRSS 34 914 904 kB, VmSwap 0, VRAM after exit 2265 MiB, exit ~4 s;
  B — VmRSS 36 412 568 kB, VmSwap 130 024 kB (~127 MiB; desktop pressure on the machine, the
  VARIANTS=2 host cost of ~185 KiB cannot cause it — recorded as is), VRAM with the process
  alive 10 996 MiB, exit ~4 s, ring 25326/25326. Graphs: A reused 56→171; B reused 56→171 (the
  counter covers the target context only, cumulative — 115 verify-reuse during measured in both
  arms; B's gain sits in the draft context, which the counter does not see — see EXP-050).
- Additional measurement per Igor's Variants (slot counter + cudaMemGetInfo in the A/B): the
  slot counter is diagnostics-only (`exp052_slots_allocated`, only with GGML_GRAPH_DIAG); in the
  measured arms diagnostics were off, so the device cost of the 63/116-node graph instances
  remains the gate's estimate (<=0.4 MiB for the 4 draft instances, invisible in nvidia-smi
  against 10 996 MiB).
