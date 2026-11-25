# EXP-2026-11-25-2046 — Five-row Bonsai verification

**Rejected for throughput in the tested geometry.** Reuse the
EXP-2043 grouped target and add a fifth input (four draft
proposals). Exact prefix transactions hold an additional
convolution history and GDN state per recurrent layer.
The PQ2 DP4A kernel shares packed weights across five rows
with the same independent dot and accumulation arithmetic.
The ordinary four-row control retains its existing kernels.

## Component and state checks

RTX 4070, SM89, synthetic finite Q4_0 K/V and FP32 queries,
256-wide heads and 6:1 GQA. The 2+2+1 and 3+2 Attention
partitions match the single-query oracle on all 13,164,192
active tile values and all 35 output rows, including tile
and attention geometry boundaries. They use the same
exact merge for all five rows.

| First key count | 2+2+1, ms | 3+2, ms |
| ---: | ---: | ---: |
| 256 | 0.1169 | 0.1208 |
| 8192 | 0.3149 | 0.3784 |
| 32767 | 1.3534 | 1.2838 |
| 32768 | 1.1957 | 1.1620 |
| 33279 | 1.2045 | 1.2235 |

The model candidate uses 3+2 throughout. The tile uses 154
registers without spills. The five-row Q8 two-output-row
kernel uses 95 registers, versus 64 in the four-row kernel.
Resource counts do not identify the model's limiting
stall or account for all target costs.

A 255-input forced-commit probe (context 1024, ubatch 128)
matches visible KV, GDN state and logits for all five
commit lengths, with four forced rejections. The next
ordinary step also matches the serial oracle byte for byte.

## RTX 4070, 32K/512 A–B–B–A

Bonsai PQ2_0-mtp, Q4_0 K/V, context 36,864, ubatch 1024,
EXP-2044 prefill, EXP-2038 Q8 draft vocabulary.
Same binary and CUDA image. A uses four target rows,
B uses five. One process per observation.
Decode counts 511 intervals after the first token.

| Arm | Prefill, tok/s | Decode, tok/s | Target, ms/block | Combined, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 624.03 | 70.91 | 41.423 | 59.717 |
| B1 | 619.86 | 68.76 | 49.737 | 60.296 |
| B2 | 617.46 | 68.84 | 49.584 | 60.492 |
| A2 | 614.74 | 70.37 | 41.682 | 60.565 |

Mean decode changes **70.64 → 68.80 tok/s (−2.61%)**.
Mean prefill is 619.39 → 618.66 tok/s. Combined time
changes **60.141 → 60.394 s**. Both decode pairs lose.

| Metric | A | B |
| --- | ---: | ---: |
| Verifier blocks | 145 | 121 |
| Accepted / proposed | 365/435 (83.91%) | 386/484 (79.75%) |
| Mean committed inputs per block | 3.517 | 4.190 |
| Commit counts 1 / 2 / 3 / 4 / 5 | 8 / 16 / 14 / 107 / 0 | 6 / 15 / 12 / 5 / 83 |
| Mean target, ms/block | 41.552 | 49.661 |
| Mean draft total, ms | 1063.81 | 1190.79 |

The existing diagnostic prints only the first four commit
buckets. The unprinted count is total blocks minus those
four buckets. It also satisfies the accepted-proposal sum.
At the measured acceptance and fixed other costs, the
five-row candidate requires about **1.60 ms less per
block** to match the four-row decode time.

All four runs reproduce the 512 EXP-2031 IDs and selected
logits byte for byte. The wider target adds 157,889,648
bytes (150.58 MiB) of explicit GPU allocations. B reports
430–433 MiB free before draft and choice allocations,
or approximately 390–392 MiB after accounting for those
allocations. Peak VRAM was not sampled. Peak RSS ranges
are 7,421,068–7,421,376 KiB in A and 7,421,040–7,421,188
KiB in B. Process swaps and major faults are zero.

Keep the four-row path. The Q4 partitioning itself
reuses independently exact subgroups. The fifth row
does not pay for its target cost and extra proposal at
the observed acceptance. No Apex, Inkling or production
source changes.

## Reproduction

Run from the root with EXP-2025, EXP-2032, EXP-2038,
EXP-2042, EXP-2043 and EXP-2044 build trees.

```bash
exp=experiments/EXP-2026-11-25-2046-bonsai-mtp-five-rows
out=build/bonsai-mtp-five-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build-image.sh" "$out"
bash "$exp/source/build-host.sh" "$out"
env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 taskset 0f0f /usr/bin/python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$out/verify.cubin" "$frozen"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_FLASH_LINEAR_Q=1 BONSAI_FFN_PREFIX=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1 BONSAI_MTP_DEVICE_CHOICE=1 BONSAI_MTP_VERIFY_PARALLEL_MERGE=1 BONSAI_MTP_DRAFT_Q8_HEAD=1 BONSAI_MTP_VERIFY_ATTN_TAIL=1)
run=(taskset 0f0f "$out/bonsai-mtp-five" "$model" "$out/verify.cubin" "$frozen")
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_MTP_TARGET_PROBE=1 BONSAI_MTP_VERIFY_ROWS=5 "${run[@]}" 1024 6 --ignore-eos "${prompt[@]:0:255}"
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' "${run[@]}" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_ROWS=4 "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_ROWS=5 "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_ROWS=5 "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_ROWS=4 "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data/32768-greedy.jsonl
cmp "$oracle" "$out/a1.jsonl"
cmp "$oracle" "$out/b1.jsonl"
cmp "$oracle" "$out/b2.jsonl"
cmp "$oracle" "$out/a2.jsonl"
```

[Measured outputs and hashes](data/)
