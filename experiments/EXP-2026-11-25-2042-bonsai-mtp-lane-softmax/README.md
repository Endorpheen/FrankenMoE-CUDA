# EXP-2026-11-25-2042 — Hoist causal checks from shared Bonsai verifier attention

**Verified within the experimental MTP path.** EXP-2033
reuses each Q4 K/V fragment for three query positions.
Its loop checks the current key against each query's
causal limit before updating softmax and V state. All
three queries can see every key preceding the first
query's boundary. The selected kernel splits that common
prefix from the at-most-two-key causal tail, preserving
the key iteration order and every individual FP32 update.

A separate experiment within the same component assigns
softmax scalar states to distinct warp lanes and broadcasts
coefficients. The scalar sharing alone loses on long shapes.
Combining it with the tail split also loses to the simpler
tail split. Only the latter is enabled by the candidate
dispatch. Merge, Q4 KV, PQ2, and prefill use the existing paths.

## Component and state checks

RTX 4070, SM89, synthetic finite Q4_0 K/V, FP32 queries,
two GQA heads and three queries per warp. The four modes
match all 7,876,224 active partial values and all 21 output
rows on seven key triplets against the separate-query oracle.
The host fixture uses the same frozen merge for all variants.

| First key count | Shared control, ms | Lane softmax, ms | Tail split, ms | Both, ms |
| ---: | ---: | ---: | ---: | ---: |
| 256 | 0.1710 | 0.1815 | 0.1356 | 0.1492 |
| 8192 | 0.3465 | 0.3573 | 0.3229 | 0.3289 |
| 32768 | 1.1322 | 1.2461 | 1.0389 | 1.0768 |
| 33279 | 1.1491 | 1.3102 | 1.0149 | 1.0898 |

The shared control uses 150 registers. Tail split uses 153,
lane softmax uses 147, and the combined version uses 136.
There are no spills. The forced-commit probe at 255 input
tokens (context 1024, ubatch 128) verifies each retained prefix,
including rejected proposals, KV/GDN state and next-token logits
byte for byte against the serial oracle.

## RTX 4070, 32K/512 A–B–B–A

Bonsai PQ2_0-mtp, Q4_0 K/V, context 36,864, ubatch 1024,
EXP-2041 packed-query prefill and EXP-2038 Q8 draft vocabulary.
Same binary and CUDA image, one process per observation.
A uses the shared control; B splits the causal tail.
Decode counts 511 intervals after prefill.

| Arm | Prefill, tok/s | Decode, tok/s | Target, ms/block | Combined, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 618.45 | 64.51 | 37.506 | 60.905 |
| B1 | 617.37 | 67.32 | 35.629 | 60.667 |
| B2 | 614.85 | 67.26 | 35.604 | 60.892 |
| A2 | 613.24 | 64.09 | 37.721 | 61.408 |

Mean decode rises **64.30 → 67.29 tok/s (+4.65%)**.
Target saves **1.997 ms per block**; combined time falls
**61.156 → 60.779 s (−0.377 s)**. Mean prefill is
615.85 → 616.11 tok/s, with opposite directions in the pairs.

All four runs match the EXP-2031 oracle, including all
512 output IDs and selected logits. Each accepts 326/370
proposals and records 14/16/155 commits. Explicit device
allocations match. Peak RSS ranges are 7,420,636–7,420,804
KiB in A and 7,420,832–7,420,908 KiB in B. Process swaps
and major faults are zero. Peak VRAM was not sampled.
Apex, Inkling and production source paths are unchanged.

The reusable technique is to partition a masked loop into
a common valid region and a bounded tail, with identical
operations for each active input. Test the causal boundary
and combined register use on the target geometry.

## Reproduction

Run from the root with EXP-2025, EXP-2030, EXP-2032,
EXP-2034, EXP-2036, EXP-2038 and EXP-2041 build trees.

```bash
exp=experiments/EXP-2026-11-25-2042-bonsai-mtp-lane-softmax
out=build/bonsai-mtp-tail-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build-image.sh" "$out"
bash "$exp/source/build-host.sh" "$out"
env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 taskset 0f0f /usr/bin/python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$out/softmax.cubin" "$frozen"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_FLASH_LINEAR_Q=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1 BONSAI_MTP_DEVICE_CHOICE=1 BONSAI_MTP_VERIFY_PARALLEL_MERGE=1 BONSAI_MTP_DRAFT_Q8_HEAD=1)
probe=(taskset 0f0f "$out/bonsai-mtp-tail" "$model" "$out/softmax.cubin" "$frozen")
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_MTP_TARGET_PROBE=1 BONSAI_MTP_VERIFY_ATTN_TAIL=1 "${probe[@]}" 1024 5 --ignore-eos "${prompt[@]:0:255}"
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' "${probe[@]}" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_ATTN_TAIL=1 "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_ATTN_TAIL=1 "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data/32768-greedy.jsonl
cmp "$oracle" "$out/a1.jsonl"
cmp "$oracle" "$out/b1.jsonl"
cmp "$oracle" "$out/b2.jsonl"
cmp "$oracle" "$out/a2.jsonl"
```

[Measured outputs and hashes](data/)
