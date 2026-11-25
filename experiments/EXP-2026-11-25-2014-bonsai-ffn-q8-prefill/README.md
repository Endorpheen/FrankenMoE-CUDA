# EXP-2026-11-25-2014 — Selective Q8 FFN gate/up prefill for Bonsai

**Accepted in the experimental Bonsai HTTP line for the last 16 layers.**
All 64 layers and the last 32 are rejected on quality. This experiment
reuses the actual `bonsai_pack_q8_128` and `bonsai_pq2_mmq_int8` kernels
from EXP-1994 and the gated scope suggested by EXP-2009. It adds no
global allocation and does not change DP4A decode.

For batches of at least 128 tokens, and only at layers 48–63
(zero-based), quantize the shared 5120-wide gate/up activation into
signed Q8 with one F32 scale per 128 values. Both PQ2 projections
reuse that pack and run an integer Tensor Core kernel. The existing
packed activation and attention partial arenas hold codes and scales
while the sequential FFN nodes run. The code checks output width
17408 and gate/up destinations. The layer counter resets per graph
evaluation. FFN down, Attention, GDN, other layers, and short tails
keep the accepted FP16 prefill path.

## Cumulative quality gate

All cases use the EXP-2013 norm, EXP-2012 DP4A decode and EXP-2011
Flash path. The EXP-2009 result without these cumulative changes
does not authorize combining them. The independent code-trace
fixture reveals the interaction:

| Scope | Matching free sequence prefix | Scored code cells |
| --- | ---: | ---: |
| All gate/up layers | 59 of 258 | 18/63 → 13/63 |
| Last 32 layers | 80 of 258 | 18/63 → 17/63 |
| Last 16 layers | 258 of 258 | 18/63 → 18/63 |

Last 16 also reproduces the full ledger, scheduling and incident
extraction generations and their independent evaluations. All
37 sampled same-history top-1 choices agree, with top-10 overlap
at least 9/10 and maximum absolute logit delta 0.42170. Arithmetic
differs from FP16, so the latest measured evidence is scoped to
these fixtures. The ledger and scheduling answers remain incorrect
in both arms. Incident extraction is correct in both.

## End-to-end benchmark

Two balanced pairs on one RTX 4070: B–A then A–B. Each pair uses the
same binary and image internally. The second binary defaults to the
last 16 layers and the final B sets no Q8 environment variable.
All use 32768/512, context 36864, batch/ubatch 4096/1024, Q4_0 KV,
greedy with EOG masked, MTP and prompt reuse off, affinity 0f0f.

| Pair | Prefill A → B, tok/s | Decode A → B, tok/s |
| --- | ---: | ---: |
| B–A | 591.782 → 609.643 | 30.810 → 30.821 |
| A–B default | 591.124 → 609.479 | 30.115 → 30.661 |
| Aggregate phase times | 591.452 → 609.561 | 30.458 → 30.741 |

Candidate prefill saves 1.646 s in the mean of both pairs, a 3.06%
improvement in throughput. Both controls run slower than EXP-2013
historical arms. No cause is assigned, since this block did not
record clocks or isolate ambient GPU use. Only within-pair differences
are scored. Decode arithmetic is identical across both modes.
The observed decode differences are not assigned to this change.

All four long requests reproduce the whole PrismML token sequence
and completion text SHA-256. Peak VRAM is unchanged at 8932 MiB.
VmHWM is 7,456,372–7,457,376 KiB for A and 7,457,252–7,457,656 KiB
for B. Every process and system swap-out delta is zero. This is a
Bonsai-scoped dispatch. Apex and Inkling release binaries and CUDA
images are unchanged. Their full shared-candidate gates remain open.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2014-bonsai-ffn-q8-prefill
dev=build/bonsai-ffn-q8-prefill-repro
out=build/bonsai-ffn-q8-prefill-http-repro
bash "$ex/source/build.sh" "$dev"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/quality.py" last16 "$dev/quality" "$dev" \
    /home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf \
    build/bonsai-integration/inventory.json build/bonsai-integration/vocabulary.gguf \
    /home/end0/AI/LlamaServer/llama.cpp/build/bin/llama-tokenize
bash "$ex/source/build-runtime.sh" "$out" "$dev"
```

Run the EXP-2013 server with `"$out/frankenmoe-infer"` and
`BONSAI_CUDA_IMAGE="$out/pq2-int8.cubin"`. A uses
`BONSAI_FFN_Q8_PREFILL=0`; B leaves it unset. Use the exact
EXP-1977 request. `1` and `last32` are diagnostic modes retained
for reproduction. See [paired measurements](data/results.json),
[quality](data/quality-last16.json) and [hashes](data/SHA256SUMS).
