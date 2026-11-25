# EXP-2026-11-25-2025 — Native Bonsai MTP draft layer

**Experimental, verified single-row forward.** The normal Bonsai
HTTP baseline remains EXP-2019. This experiment extends the
EXP-2024 input projection with block 64 and the shared PQ2_0
vocabulary head. The Goose graph reuses the established RMSNorm,
gated attention, RoPE, Q4_0 KV writer, SwiGLU, and signed Hadamard
operators. Seven draft projections use a dedicated Q4_0 CUDA matvec
with FP32 accumulation in the original activation basis.

The model layout chooses the shared vocabulary projection because
this GGUF has no separate draft vocabulary projection. At draft
position zero the probe feeds a zero hidden row. Subsequent
positions pair the current token embedding with the preceding trunk
hidden row. This follows the token/hidden shift in the pinned
[PrismML reference](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/common/speculative.cpp#L3007).
The probe owns separate draft K and V allocations. Its captured
logits use ordinary full-vocabulary greedy argmax.

## Evidence

RTX 4070, Bonsai PQ2_0-mtp, context 1024, ubatch 128, Q4_0 K/V.
The first four EXP-1977 input IDs (1812, 5020, 220, 15) produce
draft top IDs 68, 198, 16, 16. After zeroing the 4,608-byte draft
KV allocation and replaying the same four inputs, all 993,280
FP32 logits match bitwise.

An independent FP64 calculation starts from each captured
attention residual. It checks all 20,480 post-FFN normalized
outputs, with maximum absolute error 6.81e-6 and relative L2
at most 4.24e-7. It independently computes 39 shared-head logits
per position, including the eight top logits, with maximum absolute
error 1.15e-6 and matching top IDs in the checked set. This oracle
covers FFN, final norm and sampled output logits. Direct
attention/logit equivalence with PrismML remains unverified.

An ordinary 32-input/8-output smoke matches the EXP-2024 control
JSONL byte for byte, including selected logits. No production
runtime changes. No acceptance rate, prefill/decode speed, peak
VRAM or peak RAM measured. The diagnostic replay buffers and output
capture are not intended for the generation path. The Q4 matvec and
isolated KV design are potential reuse points for similar dense MTP
heads with matching layouts.

The next gate is a PrismML draft trace on identical conditioning
and KV history, followed by greedy batched target verification
with cache commit and rewind on accepted and rejected proposals.

## Reproduction

Run from the repository root with the EXP-2019 and EXP-2024
prerequisite build trees present.

```bash
exp=experiments/EXP-2026-11-25-2025-bonsai-mtp-head
out=build/bonsai-mtp-head-repro
bash "$exp/source/build.sh" "$out"
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"][:32])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_UBATCH=128 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1)
"${profile[@]}" BONSAI_MTP_HEAD_CAPTURE="$out/mtp.bin" taskset 0f0f "$out/bonsai-mtp-head" "$model" "$out/stem.cubin" "$out/frozen-goose.ptx" 1024 1 "${prompt[@]:0:4}" > "$out/mtp.jsonl"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" build/bonsai-integration/inventory.json "$out/mtp.bin"
"${profile[@]}" taskset 0f0f "$out/bonsai-mtp-head" "$model" "$out/stem.cubin" "$out/frozen-goose.ptx" 1024 8 --ignore-eos "${prompt[@]}" > "$out/ordinary.jsonl"
cmp "$out/ordinary.jsonl" "$exp/data/ordinary.jsonl"
```

[Measured outputs and hashes](data/)
