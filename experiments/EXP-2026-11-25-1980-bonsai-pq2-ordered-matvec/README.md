# EXP-2026-11-25-1980 — Ordered PQ2 matvec for Bonsai decode

**PROVEN on short requests.** Each warp still assigns the same columns to
the same lanes. The candidate decodes one FP16 scale per 128 weights and
executes the four original F32 FMA updates in their original order. The final
warp reduction is unchanged. Register allocation fell from 36 to 26 without
spills. This experiment changes only the Bonsai diagnostic CUDA path.

Four real matrices produced byte-identical F32 projection outputs. Direct
byte loads were faster than exchanging codes across lanes for all tested
matrices. The exchange variant was rejected in favor of direct loads.

| CUDA event mean, ms | Original | Direct ordered | Warp exchange |
| --- | ---: | ---: | ---: |
| GDN QKV | 0.1132 | 0.0696 | 0.0775 |
| GDN output | 0.0697 | 0.0436 | 0.0482 |
| FFN down | 0.1932 | 0.1182 | 0.1320 |
| Vocabulary | 2.6152 | 1.7945 | 1.8662 |

## Short unprofiled results

RTX 4070, local PQ2_0-mtp GGUF, 1025/32, context 36864, ubatch 1024,
Q4_0 K/V, greedy with EOS masked, no MTP. Four fresh Goose processes
were run in order A–B–B–A using one build and the two kernel modes.
The same ordered input IDs and output limit were used in one PrismML
HTTP request with prefix caching disabled.

| Arm | Prefill, tok/s | Decode, tok/s |
| --- | ---: | ---: |
| Goose control, average | 240.15 | 13.68 |
| Goose candidate, average | 240.26 | 16.03 |
| PrismML llama.cpp, commit 910442b0 | 1170.99 | 54.08 |

Candidate decode time fell by 338.5 ms on the first pair and 347.2 ms on
the reverse pair. The average decode throughput increase was 17.18%.
All four native outputs were byte-identical across 32 token IDs and
the selected F32 logits. All device workspace allocations were exactly
1,569,405,696 bytes. Peak VRAM and RAM were outside this short block.
The discrepancy from PrismML begins at position four, as in EXP-1979,
and this specific change preserves the preexisting native trajectory.

The model still trails its compatible llama.cpp reference. For the long
EXP-1977 workload, a 70% gain over PrismML means 1858.45 prefill and
81.54 decode tok/s under the same settings. EXP-1980 is evidence for a
short native improvement. Its long-workload impact remains unmeasured.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1980-bonsai-pq2-ordered-matvec
bash "$exp/source/build.sh" build/bonsai-ordered-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-matvec.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-ordered-repro/ordered.cubin
```

Use `bonsai-ordered-infer` and `ordered.cubin` for inference with the
EXP-1979 options. Set `BONSAI_UBATCH=1024`. The kernel runs by default.
Set `BONSAI_MATVEC=scalar` for the control or `exchange` for the alternate
implementation. Take the first 1025 input IDs of EXP-1977 and pass 32
as the output budget with `--ignore-eos`. Keep CPU mask 0f0f for native.
Use the equivalent 1025/32 shortening on the exact PrismML reference
configuration from EXP-1977.

[Ordered full-request measurements](results.json).
[Projection data](projection.json).
