# EXP-2026-11-26-2106 — Bonsai MTP in the cumulative HTTP runtime

Experimental, with one full diagnostic measurement. Integrate the
EXP-2047 four-row MTP draft and target verifier over the EXP-2105 HTTP
checkpoint. Load the preserved MTP device image into the same CUDA context
and reuse the already resident GGUF payload. Ordinary target prefill
keeps the EXP-2105 dispatch. Batch the additional draft KV initialization
in groups of up to 128. Use the existing GPU choice implementation for
greedy verification and keep the normal CPU sampler for requests with
sampling, repetition penalties or returned probabilities.

The verifier commits the matching prefix of each block. It restores the
accepted convolution and GDN state by exchanging buffer ownership. The
attention cursor hides later speculative rows, which are overwritten
before reuse. The server returns verified token IDs through its existing
HTTP generation loop. It stops speculation at the output or context
boundary and keeps EXP-2105 FFN graph replay on single-token target steps.
The verifier uses the cumulative direct Attention and ordered merge at
long context. This port does not add or recompile CUDA kernels.

## RTX 4070 diagnostic results

One cold HTTP request with the exact EXP-1977 raw input IDs, PQ2_0 MTP
GGUF, context 36864, batch 4096, ubatch 1024, Q4_0 K/V and greedy with
EOS masked. The cold validator records computation of all 32768 input
tokens and zero cached input tokens. All 512 output IDs and text match
the saved PrismML response exactly.

| Runtime | Prefill, tok/s | Decode, tok/s | Request, s | Process VRAM after, MiB |
| --- | ---: | ---: | ---: | ---: |
| EXP-2105 HTTP, ordinary decode, two-pair mean | 1089.67 | 38.92 | 43.267 | 8944 |
| EXP-2106 HTTP, effective MTP decode, one run | 1003.09 | 72.40 | 39.786 | 9438 |
| EXP-2047 CLI, effective MTP decode, historical mean | 618.10 | 72.12 | 60.099 combined phases | Not sampled |

EXP-2106 accepts 369 of 423 proposed tokens across 141 blocks
(87.23%). Prefill takes 32.667 s including draft KV initialization.
Decode takes 7.058 s. Peak process RAM is 7283.27 MiB, with zero process
swap. The 494 MiB increase in the process VRAM snapshot includes
transaction prefix buffers, additional logit rows and the draft cache.
The full HTTP request takes 3.481 s less than the saved ordinary
checkpoint mean on this diagnostic run. EXP-2047's combined phase time
uses the CLI timer. No equivalent PrismML MTP comparison was run.

Five short requests matched on the candidate with MTP enabled and
disabled, covering greedy, repeated-request reset, EOS, probability
output and seeded temperature sampling. The short MTP runs exercised
blocks with zero, one, two and three accepted drafts. The release
and Apex/Inkling packages stay in place. Apex and Inkling were not
rebenchmarked with the experimental executable.

## Local reproduction

Build with GCC 13, Fried Goose Keelwing, the EXP-2105 host build, the
EXP-2101 composed Goose unit and the archived inputs listed in the receipt.
Run the experimental binary with the preserved EXP-2105 argv, profile
and frozen PTX. Set `BONSAI_HTTP_MTP_IMAGE` to the produced `mtp.cubin`.
Unset it to use the ordinary HTTP path. Keep the existing GPU-choice
and FFN-graph profile settings in both modes.

```bash
exp=experiments/EXP-2026-11-26-2106-bonsai-http-mtp
out="$PWD/build/bonsai-http-mtp-2106"
base="$PWD/build/bonsai-cumulative-exp2105"
bash "$exp/source/build.sh" "$out" build/bonsai-ffn-graph-2105 \
    build/bonsai-http-avx2-greedy-2101/main-scan.goose
set -a
source "$base/profiles/bonsai.env"
set +a
mapfile -t argv < "$base/profiles/bonsai.argv"
BONSAI_CUDA_IMAGE="$out/pipeline.cubin" \
BONSAI_HTTP_MTP_IMAGE="$out/mtp.cubin" \
BONSAI_FROZEN_PTX="$base/cuda/frozen-goose.ptx" \
    "$out/frankenmoe-infer" "${argv[@]}" "$BONSAI_MODEL" --serve 19197
```

`FRANKENMOE_CHECKPOINTS` must point to the archived input directory
for the build. Set `BONSAI_MODEL` to the existing PQ2_0 MTP GGUF.

[Measurement, settings, counts and artifact hashes](data/results.json).
