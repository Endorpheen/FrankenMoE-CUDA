# EXP-2026-11-25-2024 — Native Bonsai MTP input projection

**Diagnostic component verified.** EXP-2019 remains the normal
Bonsai HTTP baseline. The MTP layer of the original GGUF is present
and type-checked by EXP-1973. This experiment runs the first MTP
projection on actual trunk hidden states. It does not generate
draft tokens or change standard decode.

The pinned PrismML reference
[`src/models/qwen35.cpp`](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/src/models/qwen35.cpp#L1148)
uses inverse-transformed token embeddings, separately weighted RMSNorm
of the embedding and target hidden row, concatenation in E,H order,
then `blk.64.nextn.eh_proj.weight`. The latter tensor is Q4_0
10240×5120. A small Goose graph reuses the established embedding,
signed Hadamard and norm operations. A scoped CUDA kernel implements
the Q4_0 projection. The adapter binds the already resident draft
weights. In this diagnostic only, existing scratch stores the
concatenation and output. There is no extra global allocation and
no speculative state is touched.

## Evidence

RTX 4070, cumulative EXP-2019 stages, Q4_0 K/V, context 1024,
ubatch 128. Use the first 32 and 129 exact EXP-1977 input IDs,
then supply the next actual input ID to the MTP stem. Compare
eight ordinary output IDs and selected logits with and without
the capture. Both ordinary output streams are byte-identical
for each input length.

An independent NumPy reconstruction from GGUF weights verifies
all 5120 restored embedding values and all 10240 concatenated
normalized values bitwise on both captures. A separate FP64 dot
oracle decodes the GGUF projection and verifies all 5120 outputs:

| Prompt prefix | Supplied next ID | EH max absolute error | EH relative L2 |
| ---: | ---: | ---: | ---: |
| 32 | 279 | 1.8971e-6 | 2.1641e-7 |
| 129 | 4530 | 1.5778e-6 | 2.7699e-7 |

The reference is the independent stem math, not a PrismML model
logit trace. The draft transformer layer, independent KV lifecycle,
shared vocabulary output and transactional batched verification
remain open. No model speed, peak memory or release regression
claim is made. The ordinary Bonsai batch path and released Apex
and Inkling paths are unchanged.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2024-bonsai-mtp-stem
out=build/bonsai-mtp-stem-repro
bash "$ex/source/build.sh" "$out"
n=129
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"][:int(sys.argv[2])])' "$req" "$n")
token=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["prompt"][int(sys.argv[2])])' "$req" "$n")
BONSAI_UBATCH=128 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 \
    BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 \
    BONSAI_MTP_STEM_TOKEN="$token" BONSAI_MTP_STEM_CAPTURE="$out/stem-$n.bin" \
    taskset 0f0f "$out/bonsai-mtp-stem" \
    /home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf \
    "$out/stem.cubin" "$out/frozen-goose.ptx" 1024 8 --ignore-eos "${prompt[@]}"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" \
    build/bonsai-integration/inventory.json "$out/stem-$n.bin" "$token"
```

Repeat with `n=32` and compare against a run without the capture
variables. [Checks and ordinary outputs](data/).
[Artifact hashes](data/SHA256SUMS).
