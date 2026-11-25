# EXP-2026-11-25-2026 — Bonsai draft arithmetic and cache parity

**Experimental comparison.** Replay four positions through EXP-2025
and the pinned PrismML MTP context on RTX 4070. Both use the same
GGUF, context 1024, batch limit 128, one token per forward, Q4_0 K/V,
explicit positions, and byte-identical input hidden rows. Row zero
uses zero conditioning. Rows 1–3 use the previous token's Goose trunk
hidden state. Each replay starts from an empty draft cache and writes
the same position sequence. PrismML runs through its exported library
API with CUDA offload. No runtime source, serving default, or model
weights are changed.

| Diagnostic condition | Greedy draft IDs | Whole-logit relative L2 vs Goose |
| --- | --- | --- |
| Goose | 68, 198, 16, 16 | — |
| PrismML default | 437, 198, 16, 16 | 5.26–13.47% |
| PrismML without cache rotation | 68, 198, 16, 16 | 3.84–7.72% |

PrismML defaults to K rotation width 256 and V rotation width 64
before Q4_0 quantization. Goose writes unrotated K and V. Setting
`LLAMA_ATTN_ROT_DISABLE=1` makes both reference widths zero. This
changes PrismML's first draft ID and reduces whole-logit relative L2
at all four positions. It leaves PrismML's EH projection bitwise
identical across both reference runs.

The independent GGUF oracle reconstructs the original-basis token
embedding and both RMSNorms. For Goose's Q4_0 × FP32 EH it reaches
maximum absolute error 6.62e-6 and relative L2 at most 2.44e-7.
Using that same arithmetic against PrismML yields relative L2 up to
8.84%. Replacing the activation operands with the Q8_1 integer dot,
half scale, and half sum correction reproduces PrismML's EH within
7.21e-6 max absolute error and 3.56e-7 relative L2. This directly
accounts for the observed EH difference. It does not isolate all
remaining attention, FFN, and vocabulary projection differences.

Primary code references for the pinned revision
`910442b0ce1128ef85d7730cbcae835cce60cdd4` are
[qwen35.cpp](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/src/models/qwen35.cpp#L719),
[llama-kv-cache.cpp](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/src/llama-kv-cache.cpp#L319),
[quantize.cu](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/ggml/src/ggml-cuda/quantize.cu#L70)
and
[vecdotq.cuh](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/ggml/src/ggml-cuda/vecdotq.cuh#L124).

This comparison concerns drafter numerics. The four reference matches
with unrotated K/V do not measure speculative acceptance or final
answer correctness. The acceptance gate remains equality to the
ordinary Goose target sequence, including rejected branches and
correct target recurrent and KV state after rewind. Throughput and
peak memory were not measured. An optional Q4_0 × Q8_1 draft matvec
or rotated draft KV needs its own acceptance and wall-time comparison.

## Reproduction

Run from the root with the EXP-2025 build and the pinned PrismML
EXP-1977 checkout and build present. The C tool links the existing
reference libraries as a diagnostic only.

```bash
exp=experiments/EXP-2026-11-25-2026-bonsai-mtp-prism
out=build/bonsai-mtp-prism-repro
previous=build/bonsai-mtp-head-2025
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0)
bash "$exp/source/build.sh" "$out"
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_HEAD_CAPTURE="$out/goose.bin" BONSAI_MTP_HIDDEN_CAPTURE="$out/hidden.bin" taskset 0f0f "$out/goose-replay" "$model" "$previous/stem.cubin" "$previous/frozen-goose.ptx" 1024 1 1812 5020 220 15
"${profile[@]}" taskset 0f0f "$out/prism-replay" "$model" "$out/hidden.bin" "$out/prism.bin" 1812 5020 220 15
"${profile[@]}" LLAMA_ATTN_ROT_DISABLE=1 taskset 0f0f "$out/prism-replay" "$model" "$out/hidden.bin" "$out/prism-unrotated.bin" 1812 5020 220 15
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/compare.py" "$out" build/bonsai-integration/inventory.json
```

[Measured outputs and hashes](data/)
