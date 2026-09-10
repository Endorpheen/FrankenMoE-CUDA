# EXP-2026-09-09-050 — the cause of the reuse reset: ping-pong of ubatch shapes in the draft context

Status: `DONE` (`kind=diagnostic-run`, 2026-09-09). Igor's assignment of 2026-09-09: an isolated
diagnostic build separating llama graph reuse (L1) and CUDA Graph capture/replay (L2),
with per-key rejection reasons and the first mismatched field; a model-free check of the
counters, then one agreed server run (warmup + decode 256); the times are diagnostics only.
The result had to name a specific cause and place in the code — it is named, see "Cause".

## Diagnostic build

- Tree `work/llama.cpp-exp050-diag` = an rsync copy of `work/llama.cpp-exp046-iq2s-verify`
  (the current baseline) + only the diagnostics patch
  (`results/archive/EXP-2026-09-09-050/exp050-graph-diag.patch`, 7 files, dry-run on a clean
  exp046 base — no discrepancies). Build `build/exp050-diag`, Release -O3 -DNDEBUG,
  GGML_CUDA_GRAPHS=ON, arch 89. Synchronizations, D2H, and algorithms are unchanged.
- One switchable gate `GGML_GRAPH_DIAG` (without the value — zero output, the counters are
  not written; verified by a model-free gate: the OFF log without a single [EXP050- line,
  ON/OFF checksums identical, 10/10 PASS; test-backend-ops CUDA0 MUL_MAT 1193/1193).
- L1: `[EXP050-L1] phase= n_tokens= n_outputs= decision= reason=` from
  `llama_context::process_ubatch` (the can_reuse decision + the first mismatched check with
  the old/new value). L2: `[EXP050-L2] event=decision key= phase= decision= n_nodes=
  calls= launches= diff=` from `ggml_backend_cuda_graph_compute` (the full decision automaton
  per split key; diff is the first mismatched node_properties field with the old/new value).
- Check of the assignment item about LLAMA_GRAPH_RESULT_DEBUG=1: the env is read in Release
  (llama-graph.cpp:1312, without an NDEBUG gate), but the output goes through LLAMA_LOG_DEBUG,
  which the server log callback suppresses at the INFO level (visible only with --verbose),
  and it prints a boolean + "placeholder" without input names. Therefore the diagnostics
  writes its own lines to stderr, bypassing the filter.

## The agreed run (one, rule 1 — the "yes" was received)

argv verbatim from EXP-046 (the difference is the binary path
`./build/exp050-diag/bin/llama-server`), env `GGML_CPU_IQ2S_FUSED=1 GGML_GRAPH_DIAG=1`,
port 8081, a clean SIGINT. Start until listening 3.3 s; requests: warmup
`request-short.json` (104 tokens, 8.48 s) and the measurement `request-decode256.json`.
Outputs by phase and the actual tokens (as required):
256/256 tokens, finish=length, wall 16.10 s, eval 17.46 tok/s, content sha256
`9270353f0601d5d6…` — byte-for-byte equal to all EXP-046 arms; draft acceptance 0.58974
(138/234, mean 2.18) — identical to EXP-046; expert ring 25326/25326; clean exit.
There was no early termination, so the question of diagnostics sufficiency in that case
never arose. All times are diagnostics only; the speedup was not measured and is not claimed.

## The phase-labeling artifact (count it with the correction counter)

The draft detector `n_layer() <= 2` did not trigger: the MTP GGUF declares
`qwen4exp.block_count=49` with 35 physical tensors (the head), so the draft ubatches got
the target_* tag. An offline correction by the (n_tokens, n_outputs) pair, a rewrite of all
combinations per run: 346×(1,1) draft generate, 173×(3,0) draft catch-up (bringing the
accepted tokens up to date in the draft KV), 173×(3,3) target verify, plus 7 boundary ones
(load/after prefill) and 4 prefill — there are no other combinations, the labeling is
complete. In the final statistics below, draft and prefill are separated from target verify.

## Results

L1, measurement segment (117 speculative steps), the cycle per step:
- target verify (3,3): reuse 115, rebuild 2 (the boundary after prefill) → **98% reuse**;
- draft catch-up (3,0): rebuild 117/117, reason `params:ubatch.n_tokens 1->3`;
- draft generate #1 (1,1): rebuild 117/117, reason `params:ubatch.n_tokens 3->1`;
- draft generate #2 (1,1): reuse 117/117.

L2, measurement segment:
- target backend: 49 split keys (n_nodes=113, one per MoE layer) — after the first
  capture, launch on every step (113–116/117), no resets → **full replay**;
- draft backend: exactly 2 split keys (n_nodes=116 and 63; `0x594cc4f0c6c0`,
  `0x594cc4f16d80`), 351 calls each, **launches=0 for the whole run**. The cycle per step
  for each key: catch-up → `direct_warmup_reset` (diff `node[0].ne[1] 1->3` /
  `node[0].ne[2] 1->0`), draft#1 → `direct_warmup_unstable` (the reverse diff), draft#2 →
  `capture` (capture + instantiate), the instance is destroyed by the next step's reset.
  Total per key per run: ~173 capture + ~173 reset + ~173 unstable, 0 replay.

## The server metric "graphs reused 56/171" decoded

task 0 prints 56, task 59 prints 171, and 171−56 = 115 — exactly the number of (3,3)-reuse
in the measurement segment. The counter is cumulative and counts only the target context.
The "56/171" fraction from EXP-049 is a misreading of two snapshots of one counter: target
verify reuse is not 33%, but 98%. The expectation of "low target reuse" is withdrawn — the
hole is elsewhere.

## Cause (the main answer)

**Alternation of ubatch shapes in the MTP draft context: catch-up of accepted tokens
(n_tokens=3) and draft generation (n_tokens=1) go through one chain of graph results and
one backend — the shape changes on every call.** The places:
- L1: `llm_graph_params::allow_reuse` requires `ubatch.n_tokens` equality
  (src/llama-graph.cpp:1487), called from `llama_context::process_ubatch`
  (src/llama-context.cpp:1419) → 2 of the 3 draft ubatches rebuild the graph every step;
- L2: the CUDA graph key = `cgraph->nodes[0]` (ggml/src/ggml-cuda/ggml-cuda.cu:4605),
  property identity is a memcmp of `node_properties` incl. `node[0].ne` (ggml-cuda.cu:2927),
  warmup requires ≥2 identical calls in a row (ggml-cuda.cu:4627–4634): the `ne[1] 1↔3` flip
  (and `ne[2] 0↔1`) on every call never lets the warmup converge.
The target context is unaffected at both levels (98% L1-reuse, 100% L2-replay).

The cost per speculative step (diagnostics, not a speedup claim): the draft backend does
2 direct executions of 179 nodes (116+63) + one capture with instantiate, whose instance
dies on the next step; plus 2 full L1 rebuilds of the draft graph.

## Directions for elimination (the decision is Igor's, nothing has been implemented)

(a) catch-up as three ubatches of 1 token each — all draft calls become shape 1: the L1
chain stabilizes, the L2 warmup converges from the second step on, then pure replay; the
semantics change slightly (3 consecutive passes instead of one batched one), the speed
effect is to be determined only by measurement in a separate EXP;
(b) a namespace for graph results / CUDA graph keys by shape hash instead of the nodes[0]
pointer;
(c) leave as is.

## Artifacts

- `results/archive/EXP-2026-09-09-050/server/` — server.log (10 754 lines, both levels),
  warmup/measured responses and curl times;
- `results/archive/EXP-2026-09-09-050/analysis-offline.txt` — the analysis with commands;
- `results/archive/EXP-2026-09-09-050/exp050-graph-diag.patch`, `exp050_synth.c`,
  `run-model-free-exp050.sh`, `gate-off.log`, `gate-on.log` — the patch and the model-free gate;
- trees: `work/llama.cpp-exp050-diag`, `build/exp050-diag`.
