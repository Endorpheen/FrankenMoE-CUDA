# EXP-2026-11-26-2107 — Reuse draft Q4 weights across eight prompt tokens

Accepted on the cumulative experimental MTP path over EXP-2106.
The Bonsai draft KV prefill unpacks a Q4_0 weight once for eight
tokens instead of four on complete 128-token blocks. Each lane keeps
one FP32 accumulator per token and retains the original FMA and warp
reduction order. Short final blocks keep the original dispatch.
The target image, the original MTP image, the verifier and the Goose
generation object are reused unchanged.

## Measured budget

An unscored EXP-2106 Nsight trace of a cold 32K/512 MTP request records
768 calls to the draft Q4 projection, totaling 2690 ms of GPU kernel
time. The 256 calls to the 10240 × 5120 conditioning projection
(`blk.64.nextn.eh_proj.weight`) use 2273 ms. The two 5120 × 1024 K/V
projections account for the remaining 416 ms.

The component test uses real model weights and seeded input. All variants
match the original FP32 words at 128 and 113 tokens. CUDA-event medians
for 128 tokens are below. The variants use 43, 96 and 241 registers,
respectively, without spills on sm89. Keep the eight-token path.

| Projection | Four, ms | Eight, ms | Sixteen, ms | Thirty-two, ms |
| --- | ---: | ---: | ---: | ---: |
| Conditioning | 9.011 | 6.774 | 29.768 | 33.797 |
| K | 0.733 | 0.667 | 2.436 | 3.009 |
| V | 0.733 | 0.666 | 2.436 | 3.009 |

## RTX 4070, cold 32K/512, four-row MTP in both arms

One executable and an A–B–B–A order. A omits the optional eight-token
image, B loads it. Use the same PQ2_0 MTP GGUF and EXP-1977 raw input
IDs, context 36864, batch 4096, ubatch 1024, Q4_0 K/V and greedy
with EOS masking. A fresh process computes all 32768 input tokens in
each arm. No prompt tokens were reused. All four responses match all
512 token IDs and text of the saved PrismML response, as well as the
EXP-2106 sequence of per-block proposal and acceptance counts.

| Arm | Prefill, tok/s | Effective decode, tok/s | Request, s | Process VRAM after, MiB |
| --- | ---: | ---: | ---: | ---: |
| A | 1001.46 | 72.49 | 39.830 | 9438 |
| B | 1014.99 | 72.31 | 39.416 | 9438 |
| B2 | 1018.59 | 72.36 | 39.298 | 9438 |
| A2 | 1001.20 | 72.31 | 39.858 | 9438 |
| Control mean | 1001.33 | 72.40 | 39.844 | 9438 |
| Candidate mean | 1016.79 | 72.33 | 39.357 | 9438 |

Prefill improves 1.54%, saving 498 ms. End-to-end time falls 487 ms
(1.22%). The effective decode difference reverses across pairs.
The mean decode interval differs by +6.8 ms. Both arms accept 369 of
423 drafts (87.23%). Peak process RAM is 7284.54 versus 7284.22 MiB
and process swap is zero in all four arms.

This setting only affects the Bonsai HTTP draft prefill. The stable
release and ordinary cumulative checkpoint retain their files.
Apex and Inkling were not rebenchmarked. The scored A/B holds MTP
constant and does not establish a performance ratio against PrismML.

The largest measured target prefill kernel remains the wide PQ2 INT8
gate/up projection family at 6.827 s in the diagnostic trace.
The FP16 5120-wide projection family takes 4.982 s. These scopes
offer more room than further tuning launch overhead in the draft fill.

## Reproduce

Use the archived EXP-2047 source and EXP-2106 build inputs. The build
adds a small auxiliary CUBIN and relinks only the Bonsai host adapter.
Set `FRANKENMOE_CHECKPOINTS` to the archived input directory.

```bash
exp=experiments/EXP-2026-11-26-2107-bonsai-mtp-draft-q4
base=build/bonsai-http-mtp-2106
out=build/bonsai-mtp-draft-q4-2107
bash "$exp/source/build.sh" "$FRANKENMOE_CHECKPOINTS/bonsai-mtp-warp-rows-2047/mvq.cl" "$out"
bash "$exp/source/build-http.sh" "$out" "$base"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
    "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
    "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$base/mtp.cubin" "$out/q4.cubin"
```

Use the EXP-2106 launch with the new binary and matching unchanged
target/MTP images. Add `BONSAI_HTTP_DRAFT_Q4_IMAGE="$PWD/$out/q4.cubin"`
to enable the accepted experimental draft projection.

[Profile](data/profile.json) · [Component results](data/component.json) ·
[Paired results and SHA-256](data/results.json).
