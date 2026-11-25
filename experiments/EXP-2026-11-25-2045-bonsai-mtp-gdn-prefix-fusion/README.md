# EXP-2026-11-25-2045 — Exact multi-step Bonsai GDN transaction

**Exact component, no default change.** The current verifier
runs one frozen Goose GDN kernel per verification token. This
Bonsai-scoped kernel holds a column's 128-element recurrence
shard in registers across three or four tokens. It preserves
the frozen kernel's per-lane FMA and eight-lane butterfly
order, exact scale, and chronological state updates. It writes
the same intermediate prefix snapshots and overwrites the
existing final destination. Unchanged transaction logic picks
the accepted prefix. The kernel is isolated to MTP verification.

## Component and state checks

RTX 4070, SM89, Bonsai 16 QK heads, 48 value heads and width 128.
The device fixture compares zero and nonzero initial states with
the frozen Goose single-row decoder for three and four rows.
All 86,016 output values and 11,010,048 prefix state values
match bitwise. Each fixture also checks the in-place final
state path. The kernel uses 40 registers without spills.

| Rows / state | Sequential, µs | Fused, µs |
| --- | ---: | ---: |
| 3 / random | 31.33 | 14.49 |
| 3 / zero | 30.85 | 14.54 |
| 4 / random | 41.37 | 17.82 |
| 4 / zero | 29.08 | 18.14 |

CUDA event medians cover the GDN component only. The 255-input
full-model forced-commit probe matches visible KV/GDN state
and next-token logits for all four commit lengths, including
three forced rejections and a subsequent ordinary step.

## RTX 4070, 32K/512 A–B–B–A

Bonsai PQ2_0-mtp, Q4_0 K/V, context 36,864, ubatch 1024,
EXP-2044 prefill and four-row MTP, same binary and CUDA image.
A uses the frozen launches; B uses the one-pass recurrence.
One process per observation. Decode counts 511 intervals
after the prefill-provided first token.

| Arm | Prefill, tok/s | Decode, tok/s | Target, ms/block | Combined, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 621.86 | 71.02 | 41.318 | 59.888 |
| B1 | 618.82 | 71.03 | 41.279 | 60.147 |
| B2 | 616.69 | 71.22 | 41.101 | 60.310 |
| A2 | 614.90 | 70.31 | 41.712 | 60.557 |

Mean decode rises **70.67 → 71.12 tok/s (+0.64%)**.
The paired decode savings were 0.256 ms and 92.508 ms
across the entire response. Mean target saves 0.325 ms per
block. Mean prefill is 618.38 → 617.75 tok/s; the paired
directions disagree. Combined time is **60.222 → 60.229 s**.
This does not establish a repeatable full-request gain.
Do not infer model benefit from the faster isolated kernel.

All four runs reproduce the EXP-2031 oracle, including all
512 IDs and selected logits. Acceptance stays at 365/435
with 8/16/14/107 commit counts. Allocations match.
Peak RSS ranges overlap (7,420,796–7,421,268 KiB in A,
7,420,592–7,420,996 KiB in B). No process swaps or major
faults. Peak VRAM was not sampled. Apex, Inkling and
production paths stay unchanged.

Keep this exact kernel as an available verifier component.
A wider verifier may benefit from avoiding intermediate
reads. Its state footprint still includes every prefix
snapshot needed by partial commit and requires a new
capacity and throughput check.

## Reproduction

Run from the root with EXP-2025, EXP-2032, EXP-2038,
EXP-2043 and EXP-2044 build trees.

```bash
exp=experiments/EXP-2026-11-25-2045-bonsai-mtp-gdn-prefix-fusion
out=build/bonsai-mtp-gdn-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build-image.sh" "$out"
bash "$exp/source/build-host.sh" "$out"
env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 taskset 0f0f /usr/bin/python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$out/gdn.cubin" "$frozen"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_FLASH_LINEAR_Q=1 BONSAI_FFN_PREFIX=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1 BONSAI_MTP_DEVICE_CHOICE=1 BONSAI_MTP_VERIFY_PARALLEL_MERGE=1 BONSAI_MTP_DRAFT_Q8_HEAD=1 BONSAI_MTP_VERIFY_ATTN_TAIL=1 BONSAI_MTP_VERIFY_ROWS=4)
run=(taskset 0f0f "$out/bonsai-mtp-gdn" "$model" "$out/gdn.cubin" "$frozen")
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_MTP_TARGET_PROBE=1 BONSAI_MTP_FUSED_GDN=1 "${run[@]}" 1024 5 --ignore-eos "${prompt[@]:0:255}"
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' "${run[@]}" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_FUSED_GDN=1 "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_FUSED_GDN=1 "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data/32768-greedy.jsonl
cmp "$oracle" "$out/a1.jsonl"
cmp "$oracle" "$out/b1.jsonl"
cmp "$oracle" "$out/b2.jsonl"
cmp "$oracle" "$out/a2.jsonl"
```

[Measured outputs and hashes](data/)
