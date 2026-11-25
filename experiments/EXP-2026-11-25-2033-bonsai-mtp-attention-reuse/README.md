# EXP-2026-11-25-2033 — Share Q4 attention across exact Bonsai verification rows

**Verified improvement within the experimental MTP path.** The
candidate reads each Q4 K/V fragment once for three query positions.
A warp keeps separate FP32 accumulators and online softmax state for
each query and each of two GQA heads. It preserves the same dot,
warp reduction, softmax recurrence, causal bounds, and frozen merge
as individual target calls. The dispatcher uses the existing partial
buffer when all three tile planes fit. The normal single-query path
and the draft path retain their original dispatch.

## Component check

RTX 4070, SM89, synthetic finite Q4_0 K/V and FP32 queries,
256-wide heads and 6:1 GQA. For each of seven key triplets,
the serial reference uses the production two- or three-head
kernel at its normal geometry boundary. Both variants match
all 7,876,224 active tile values and all 21 output rows bitwise,
including across 256-key tiles and the 32,768-key geometry switch.

| First key count | Serial three calls, ms | One head shared, ms | Two heads shared, ms |
| ---: | ---: | ---: | ---: |
| 256 | 0.363 | 0.272 | 0.171 |
| 8,192 | 0.529 | 0.408 | 0.349 |
| 32,768 | 1.519 | 1.831 | 1.134 |
| 33,279 | 1.540 | 1.745 | 1.150 |

CUDA event medians cover tiles and all three frozen merges.
The one-head candidate remains diagnostic because it is slower
than serial at the long lengths. The two-head kernel uses 150
registers per thread and has no spills. Occupancy and numerical
equivalence must be rechecked for different head geometry.

## Model check and measurements

The 255-input forced-commit probe (context 1024, ubatch 128)
matches EXP-2030's serial reference for all three retained
prefix lengths, their state and the subsequent decode logits.

Main workload: 32K/512, context 36,864, ubatch 1024, Q4_0 K/V,
Bonsai PQ2_0-mtp. A–B–B–A, one invocation at a time, same
binary and CUDA image. A uses EXP-2032's independent attention
calls, B shares the Q4 fragments.

| Arm | Prefill, tok/s | Decode, tok/s | Target, ms/block | Total, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 613.36 | 46.47 | 50.601 | 64.420 |
| B1 | 611.38 | 50.84 | 45.450 | 63.647 |
| B2 | 611.16 | 50.73 | 45.565 | 63.689 |
| A2 | 608.54 | 46.32 | 50.808 | 64.879 |

The mean decode rate improves **46.40 → 50.79 tok/s (+9.46%)**
and total time falls **64.649 → 63.668 s (−0.981 s)**.
Prefill stays at **610.95 → 611.27 tok/s** with opposite
directions in the two pairs. The mean target call saves
5.197 ms per block, about 0.961 s across the decode.

All four runs match the EXP-2031 output byte for byte
(512 IDs and selected logits), with 326/370 accepted
proposals and identical 14/16/155 commit counts.
Workspace and draft allocations are unchanged.
Peak RSS is 7,419,928–7,420,552 KiB in A and
7,420,296–7,420,304 KiB in B. Process swaps and major
faults are zero. Peak VRAM was not sampled. No Apex or
Inkling source or production serving changes.

## Reproduction

Run from the root with EXP-2025, EXP-2030 and EXP-2032
build trees present. Each command is an individual run.

```bash
exp=experiments/EXP-2026-11-25-2033-bonsai-mtp-attention-reuse
out=build/bonsai-mtp-attn-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build-image.sh" "$out"
bash "$exp/source/build-host.sh" "$out"
env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 taskset 0f0f /usr/bin/python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$out/attention.cubin" "$frozen"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1)
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_MTP_TARGET_PROBE=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1 taskset 0f0f "$out/bonsai-mtp-attention" "$model" "$out/attention.cubin" "$frozen" 1024 5 --ignore-eos "${prompt[@]:0:255}"
mtp=(BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_BATCH_PREFILL=1)
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' taskset 0f0f "$out/bonsai-mtp-attention" "$model" "$out/attention.cubin" "$frozen" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" "${mtp[@]}" "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" "${mtp[@]}" BONSAI_MTP_VERIFY_SHARED_ATTN=1 "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" "${mtp[@]}" BONSAI_MTP_VERIFY_SHARED_ATTN=1 "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" "${mtp[@]}" "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data/32768-greedy.jsonl
cmp "$oracle" "$out/a1.jsonl"
cmp "$oracle" "$out/b1.jsonl"
cmp "$oracle" "$out/b2.jsonl"
cmp "$oracle" "$out/a2.jsonl"
```

[Measured outputs and hashes](data/)
