# EXP-2026-11-25-2039 — Three proposals in the exact Bonsai verifier

**Rejected for throughput in this geometry.** The same
experimental Bonsai binary and CUDA image support the default
three target rows (anchor plus two proposals) and a four-row
candidate (anchor plus three). Longer blocks reduce the number
of times the trunk reads weights if enough proposals are retained.

The candidate specializes the exact shared Q8 DP4A and two-head
Q4 attention kernels for four independent rows, invokes the
same ordered merge, and adds a third intermediate GDN and
convolution-history slot per layer. Target arithmetic and the
per-row state order remain unchanged. The draft repeats the
same Q8 vocabulary step for the third proposal. The CUDA image
and CLI remain isolated from other architectures.

## Correctness

RTX 4070, Bonsai PQ2_0-mtp, Q4_0 K/V. On a 255-token prefix,
context 1024 and ubatch 128, the four-row commit probe matches
visible KV, recurrent state and all accepted logits for prefix
lengths 1, 2, 3 and 4, including three forced rejections. The
subsequent ordinary decode also matches the serial oracle
bitwise. The 129/64 smoke matches the EXP-2031 output.
Each of the four long runs matches all 512 EXP-2031 IDs
and selected target logits byte for byte.

## RTX 4070, 32K/512 A–B–B–A

Context 36,864, ubatch 1024, EXP-2038 Q8 vocabulary, GPU
choice, exact target and batch draft prefill. Mode A uses
two proposals, B three. The model, binary and image match.
Each observation uses its own process. Decode counts 511
intervals after the prefill-provided first token.

| Arm | Prefill, tok/s | Decode, tok/s | Target, ms/block | Total, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 616.97 | 64.41 | 37.520 | 61.045 |
| B1 | 612.25 | 61.51 | 49.104 | 61.828 |
| B2 | 610.88 | 61.47 | 49.148 | 61.954 |
| A2 | 607.00 | 64.03 | 37.744 | 61.964 |

Mean decode changes **64.22 → 61.49 tok/s (−4.25%)**.
Mean prefill is 611.99 → 611.56 tok/s. Mean decode
time changes 7.957 → 8.311 s, and combined time
changes 61.505 → 61.891 s. Both decode pairs lose.

| Metric | A | B |
| --- | ---: | ---: |
| Verifier blocks | 185 | 145 |
| Accepted / proposed | 326/370 (88.11%) | 365/435 (83.91%) |
| Mean committed inputs per block | 2.762 | 3.517 |
| Commit counts 1 / 2 / 3 / 4 | 14 / 16 / 155 / 0 | 8 / 16 / 14 / 107 |
| Target ms per block | 37.632 | 49.126 |
| Mean total draft time | 896.248 ms | 1,055.501 ms |

At the measured acceptance, the four-row path needs to save
about 2.44 ms per block on average to reach the control's
decode time. This is a fixed-other-cost diagnostic bound.
The four-row Q4 tile uses 201 registers per thread without
spills. The baseline three-row tile uses 150. The capture
does not isolate hardware occupancy or identify how much
of the target slowdown comes from Attention versus PQ2.

Explicit device allocations increase by 157,889,392 bytes
(150.57 MiB), including the choice records and prefix states.
The device reports 574–576 MiB free before draft and choice
allocations in B. Peak RSS ranges are 7,420,496–7,420,628 KiB
in A and 7,419,612–7,420,132 KiB in B. Process swaps and
major faults are zero. Peak VRAM was not sampled.

Keep the two-proposal EXP-2038 path. Revisit four rows
only if a new tile reduces the measured per-block time or
if a different policy demonstrably changes acceptance.
Candidate probes include two independent two-row attention
panels with a smaller register footprint and exact prefix
reduction. Neither improvement has been measured. No
promotion into production and no change to Apex or Inkling.

## Reproduction

Run from the root with EXP-2025, EXP-2032, EXP-2036 and
EXP-2038 build trees present.

```bash
exp=experiments/EXP-2026-11-25-2039-bonsai-mtp-three-proposals
out=build/bonsai-mtp-width-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build.sh" "$out"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1 BONSAI_MTP_VERIFY_SHARED_ATTN=1 BONSAI_MTP_DEVICE_CHOICE=1 BONSAI_MTP_VERIFY_PARALLEL_MERGE=1 BONSAI_MTP_DRAFT_Q8_HEAD=1)
probe=(taskset 0f0f "$out/bonsai-mtp-width" "$model" "$out/verify.cubin" "$frozen")
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_MTP_TARGET_PROBE=1 "${probe[@]}" 1024 5 --ignore-eos "${prompt[@]:0:255}"
"${profile[@]}" BONSAI_UBATCH=128 BONSAI_MTP_TARGET_PROBE=1 BONSAI_MTP_VERIFY_ROWS=4 "${probe[@]}" 1024 5 --ignore-eos "${prompt[@]:0:255}"
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' "${probe[@]}" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_ROWS=4 "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_ROWS=4 "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" BONSAI_UBATCH=1024 BONSAI_MTP_GREEDY=1 "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
oracle=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/data/32768-greedy.jsonl
cmp "$oracle" "$out/a1.jsonl"
cmp "$oracle" "$out/b1.jsonl"
cmp "$oracle" "$out/b2.jsonl"
cmp "$oracle" "$out/a2.jsonl"
```

[Measured outputs, allocations and hashes](data/)
