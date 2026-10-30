# EXP-088: additional near-tail checkpoint — REJECTED

The reference is accepted EXP-087, project commit `c3c52dcc8f15a6c960d2777b98e2f8a50620c54d`. The new goal requires another5% improvement beyond that baseline. Its existing memory saving is not counted again.

Adding a checkpoint before the last1 or2 prompt tokens really reduces repeated prompt work. However, splitting the old4-token tail into3+1 or2+2 changes the numerical execution sufficiently to fail the pre-stated selected-ID/ordered-top5/probability gate. No engine change or performance improvement is accepted.

| Diagnostic repeat | EXP-087 control | Tail1 | Tail2 |
|---|---:|---:|---:|
| Reused prompt tokens | 32764 | 32767 | 32766 |
| Evaluated prompt tokens | 4 | 1 | 2 |
| Client first generated token, ms | 164.812 | 60.015 | 134.496 |
| First selected-ID divergence, zero-based | — | 65 | 35 |
| First ordered-top5 divergence, zero-based | — | 3 | 0 |

The cold and repeat cases fail identically within each variant. All requests use32768 input IDs, up to128 output IDs, seed123, temperature1, top_p0.95, top_k20, min_p0, EOS respected and5 probability records. Configured context262144, batch/ubatch4096, CPU MoE, Q4_0 KV, one slot, threads8/16 and existing CPU masks remain unchanged. Qualification uses3 fresh test-server starts and7 requests: control cold/repeat/late branch; tail1 cold/repeat; tail2 cold/repeat. The last planned tail1 branch was skipped at the numerical stop gate.

These latency observations are not a five-pair acceptance series. Control resources also record one page of system swap-out and3 major faults; candidates have no process swap, no swap-out growth and no request-window major faults. Do not use the diagnostic timings to claim a delivered speedup. One extra PARTIAL_ONLY snapshot costs65864428 bytes (~62.8MiB); RSS is not a matched memory acceptance series, and process VRAM peak was not separately sampled.

The model-free tests pass old SWA/recurrent bounds and new count/byte/overflow/disable checks. Those tests do not prove model numerical equivalence. The source control-flow probe demonstrates that both near-tail boundaries are reachable; an independent helper report had contradictory control-flow/restore conclusions and was not accepted as safety evidence. Its eviction warning is valid: an additional interior point may alter future anchor selection under pressure. No pressure or persistence acceptance is claimed after the numerical failure.

An untouched control context object, complete ten-member static archive and server implementation reproduce EXP-087 bitwise. Initial incomplete-header/archive build attempts were discarded. Experimental code and binaries remain isolated under `/home/end0/AI/LlamaServer/exp088-near-tail-checkpoint`; no rejected feature patch is delivered. EXP-087 was restored by one additional service startup on8081 with all runtime hashes verified. Installed Desktop/runtime profiles and old accepted builds are unchanged.

`metrics.json` records the exact scope and failures; `local-evidence-manifest.json` hashes local SSE, traces, resources and build evidence. Raw prompts, helper conversations, private instructions, model weights and binaries are excluded from Git. The standalone `analyze_ttft.py` only reads curl traces; it never sends requests. It detects generated-token arrays rather than treating early HTTP headers as TTFT.

Retry only with a mechanism preserving numerical execution, or explicit separate validation criteria approved before implementation. Do not keep trying arbitrary tail lengths or relax the current gate to turn this result positive. The next source-feasibility direction is the CPU-weight load/pinning path, preserving batching and all model arithmetic.
