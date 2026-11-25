# EXP-2026-11-25-2050 — Bonsai prefill profile after fused Flash Attention

**Diagnostic attribution.** The new Nsight capture uses the accepted
EXP-2049 HTTP binary and CUDA image on RTX 4070. It submits the fixed
EXP-1977 raw prompt with 16 returned tokens instead of 512, in context
36,864, batch/ubatch 4096/1024, Q4_0 K/V, greedy and MTP off. Compare the
new capture to the historical EXP-2019 and PrismML EXP-2010 diagnostics
with matching request IDs. All 16 output IDs match. Both raw wall time
under tracing and kernel cost attribution are separate from scored A/B.

| Prefill activity, ms | PrismML EXP-2010 | FrankenMoE EXP-2019 | FrankenMoE EXP-2049 |
| --- | ---: | ---: | ---: |
| GPU timeline span | 28,405.45 | 50,386.67 | 43,880.13 |
| Main matrix kernels plus activation packing | 16,328.17 | 29,757.59 | 29,930.92 |
| Flash stages, tiles and merge | 4,064.58 | 14,632.97 | 8,022.59 |

The matrix row includes `mul_mat_q` and `quantize_mmq_q8_1` for PrismML
and `bonsai_pq2_cached_64x128_k64`, `bonsai_pq2_mmq_int8`,
`bonsai_pack_half` and `bonsai_pack_q8_128` on FrankenMoE. These are
kernel duration sums, not exclusive allocations of total latency.
PrismML Flash is `flash_attn_ext_f16`. FrankenMoE Flash includes all
`bonsai_flash*` kernels in the bracket.

The newest trace places 27.29 s in the existing FP16 PQ2 kernel.
The fused Flash entry accounts for 7.234 s in 400 dispatches. Relative
to EXP-2019, the summed Flash time falls by 6.61 s, close to the
6.78 s prefill improvement in the independent EXP-2049 pairs.
The remaining Flash gap is about 3.96 s. Matrix work is the leading
opportunity, with about 13.60 s between the measured matrix groups.
Use these cross-trace deltas to prioritize hypotheses. Use paired
32K/512 performance to establish a production improvement.

## Bonsai acceptance levels

Freeze the GGUF and its hash, raw prompt IDs, context, batch/ubatch,
K/V formats, sampler and seed, EOS policy, MTP policy, and cache state
for both runtimes. Record clocks, board and process memory, and actual
device placement. Keep base and candidate build receipts and raw data.

1. **Strict equivalence.** Compare all 512 emitted IDs and response text
   on the fixed long requests. Compare full-vocabulary logits at identical
   forced histories and test changed PQ2, attention and GDN operations on
   matching inputs. Where a path claims exact arithmetic, require bitwise
   equality on those outputs and on state transitions in the relevant
   fixtures. Compare KV and GDN after prefill and after each decode step
   in the instrumented fixtures, with buffer layouts normalized when
   needed. Report the exact bitwise scope. Identical responses alone do
   not demonstrate equal logits or internal states.
2. **Functional equivalence.** If the arithmetic changes, preregister
   numerical tolerances for full fixed-history logits, top-token or top-k
   agreement and intermediate tensor or state checks before evaluating
   the candidate. At the first free-generation divergence, replay on the
   same preceding IDs to distinguish arithmetic effects from a changed
   history. Compare independently scored programming, reasoning, extraction
   and long-context tasks against the accepted FrankenMoE build and PrismML.
   Require preserved scores and explicitly report numerical deviations.
   Retain the strict long-request token and text comparisons as separate
   results. Use the existing EXP-1994 and EXP-2040 fixtures where applicable.

Keep the priority on prefill parity, then the 1.7× prefill target on
matched hardware. Verify decode and process memory alongside paired prefill.
Retain Apex and Inkling release gates for shared changes. This trace tests
the 16-ID prefix only. It adds no new evidence for full logits, state
equivalence, complex task quality or cross-architecture execution.

## Reproduce

Use the EXP-2049 package, trim the fixed EXP-1977 request to
`n_predict=16`, and collect one run with Nsight Systems:

```bash
nsys profile --trace=cuda --cuda-graph-trace=node --sample=none --cpuctxsw=none \
  --duration=83 --cuda-flush-interval=1000 --kill=sigterm --output=PATH/cuda \
  PATH/package/frankenmoe bonsai PATH/model.gguf 19197
nsys export --type sqlite --output PATH/trace.sqlite PATH/cuda.nsys-rep
python3 experiments/EXP-2026-11-25-2050-bonsai-prefill-attribution/source/attribute.py \
  PRISM_EXP2010_TRACE FRANKEN_EXP2019_TRACE PATH
```

Save the HTTP result as `response.json` in the same directory as the
SQLite export. The first collection had missing CUDA driver injection.
The next collection lost its final CUDA buffer on early process exit.
The reported collection retained all 16 vocabulary-kernel boundaries
with periodic CUDA flushes and timer-driven termination.

[Kernel attribution and trace hashes](data/attribution.json).
