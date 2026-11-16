# EXP-2026-10-31-111: Flint lazy-KV compatibility

Status: COMPATIBLE (correctness; no performance claim).

EXP090 vs EXP101 on the Flint profile (context 131072, CPU MoE, FA, Q4_0 KV,
threads 8/16, reasoning-budget 4096) with cold 32768, shared-prefix branch
(cache 24576 / prompt 8192) and early branch (cache 4096 / prompt 12288).
IDs, ordered top-5 and probabilities identical in all three cases (delta 0);
no truncation or error in the lazy/SWA/checkpoint paths. VRAM 3264 -> 2536 MiB
(-728 MiB). The apparent 2.4x prefill was cold page-cache: a warm-cache control
reproduced 1459 tok/s vs candidate 1441; no speedup claimed.
Artifacts: results/opencode-next/EXP111/.
