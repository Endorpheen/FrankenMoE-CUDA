# EXP-2026-11-25-2044 — Fuse the exact Bonsai FFN input preparation

**Verified within experimental Bonsai prefill.** EXP-1999
rejected joint gate/up MMA. This experiment keeps both
projections and instead fuses residual add, RMSNorm, signed
FWHT and activation packing before them. The 256-thread CUDA
block completes one token. It maintains the original warp
accumulation order, signs, FWHT butterflies and roundings.
The residual remains materialized for the existing FFN output.

The candidate emits the exact FP16 packet or the already
accepted late-layer Q8 codes and FP32 scales. The normal
layer counter still runs on the gate projection. The fused
call uses the same Q8 gate as that projection and marks
the existing packet ready. It runs only for Bonsai prefill
with at least 128 rows and the established wide norm mode.
No new paths for Apex, Inkling or production serving.

## Component and model checks

RTX 4070, SM89, two real post-attention norm tensors
(layers 0 and 3), actual model signs and 24 input/format
fixtures. Both fused paths match the control on all
106,199,040 packet bytes, 283,197,440 residual bytes and
1,106,240 Q8 scale bytes. This includes all-zero input,
the 512/513 normalization boundary and a 1023-token tail.

| Layer 0, tokens | FP16 control / fused, ms | Q8 control / fused, ms |
| ---: | ---: | ---: |
| 128 | 0.0280 / 0.0190 | 0.0287 / 0.0210 |
| 1023 | 0.3317 / 0.1824 | 0.3221 / 0.1786 |
| 1024 | 0.3303 / 0.1834 | 0.3187 / 0.1779 |

The timing covers a complete layer's input preparation
for the batch. The fixed 129/64 model smoke and all long
runs match the EXP-2031 oracle, including each output ID
and selected FP32 logit.

## RTX 4070, 32K/512 A–B–B–A

Bonsai PQ2_0-mtp, Q4_0 K/V, context 36,864, ubatch 1024,
EXP-2043 four-row greedy MTP, same binary and image.
One process per observation. Decode counts 511 intervals
after the prefill-provided first token.

| Arm | Prefill, tok/s | Decode, tok/s | Prefill, s | Combined, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 617.01 | 70.86 | 53.107 | 60.319 |
| B1 | 619.09 | 70.42 | 52.929 | 60.186 |
| B2 | 617.20 | 70.24 | 53.091 | 60.367 |
| A2 | 611.26 | 70.36 | 53.608 | 60.870 |

Mean prefill rises **614.13 → 618.14 tok/s (+0.65%)**.
The paired prefill reductions are 178 and 516 ms.
Mean decode is **70.61 → 70.33 tok/s (−0.40%)** with the
same decode dispatch. Combined time falls
**60.594 → 60.276 s (−0.318 s)**, improving in both pairs.

All four runs match all 512 output IDs and selected
logits, with the same 365/435 MTP acceptance and
8/16/14/107 commit counts. Explicit device allocations
match. Peak RSS ranges are 7,421,032–7,421,284 KiB
in A and 7,420,576–7,420,640 KiB in B. Process swaps
and major faults are zero. Peak VRAM was not sampled.

The reusable pattern is to forward the exact packed-input
identity to existing projections and preserve any residual
needed by the graph. The fusion requires matching reduction
order, transform geometry and scratch lifetime.

## Reproduction

Run from the root with EXP-2025, EXP-2032, EXP-2038 and
EXP-2043 build trees present.

```bash
exp=experiments/EXP-2026-11-25-2044-bonsai-ffn-prefix
out=build/bonsai-ffn-prefix-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build-image.sh" "$out"
bash "$exp/source/build-host.sh" "$out"
env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 taskset 0f0f /usr/bin/python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$out/prefix.cubin"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_FLASH_LINEAR_Q=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1 BONSAI_MTP_DEVICE_CHOICE=1 BONSAI_MTP_VERIFY_PARALLEL_MERGE=1 BONSAI_MTP_DRAFT_Q8_HEAD=1 BONSAI_MTP_VERIFY_ATTN_TAIL=1 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_ROWS=4)
run=(taskset 0f0f "$out/bonsai-ffn-prefix" "$model" "$out/prefix.cubin" "$frozen")
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_FFN_PREFIX=1 "${run[@]}" 1024 64 --ignore-eos "${prompt[@]:0:129}" >"$out/129.jsonl"
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' "${run[@]}" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" BONSAI_UBATCH=1024 "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_FFN_PREFIX=1 "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_FFN_PREFIX=1 "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" BONSAI_UBATCH=1024 "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data
cmp "$oracle/129-greedy.jsonl" "$out/129.jsonl"
cmp "$oracle/32768-greedy.jsonl" "$out/a1.jsonl"
cmp "$oracle/32768-greedy.jsonl" "$out/b1.jsonl"
cmp "$oracle/32768-greedy.jsonl" "$out/b2.jsonl"
cmp "$oracle/32768-greedy.jsonl" "$out/a2.jsonl"
```

[Measured outputs and hashes](data/)
