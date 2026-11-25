# EXP-2026-11-25-2031 — End-to-end greedy Bonsai MTP

**Experimental CLI path.** Wire the native EXP-2025 drafter to the
exact EXP-2029 three-row target and EXP-2030 device prefix commit.
The first target-selected token is the anchor. Its draft forward
reads the previous target hidden row. The next draft forward reads
the normalized draft hidden row and proposes a second token.
The target evaluates anchor plus both proposals together and retains
only the exact greedy prefix. Rejected target and draft KV rows are
outside the next visible prefix. Accepted draft history is repaired
using the real target hidden inputs.

The separate draft K/V cache stays on the GPU. A KV-only Goose graph
fills initial prompt history and corrects accepted rows without
using its Q, attention, FFN or vocabulary output. It runs one row
at a time. The primary target graph remains batched for prompt
prefill and uses the ordinary serial path for the final small tail.
No production HTTP changes.

## Measurements

RTX 4070, Bonsai PQ2_0-mtp, Q4_0 K/V, baseline Q8 target decode.
Greedy fixtures use the first 129 input IDs and the entire 32K
input of EXP-1977. Context and ubatch are 1024/128 for 129/64
and 36,864/1024 for 32K/512. Same diagnostic binary and CUDA image,
one process per observation, order A–B–B–A. Short and long
throughput means average the per-run rates. Decode counts generated
tokens minus the prefill-provided first one. Timing excludes model
loading but includes prefill catch-up, draft and target calls,
sampling, commit, repair and output formatting.

| Workload | Mode | Prefill, tok/s | Decode, tok/s | Prefill plus decode |
| --- | --- | ---: | ---: | ---: |
| 129/64 | Serial control | 583.83 | 41.41 | 1.742 s |
| 129/64 | Greedy MTP | 546.15 | 59.46 | 1.296 s |
| 32K/512 | Serial control | 645.83 | 32.60 | 66.416 s |
| 32K/512 | Greedy MTP | 577.06 | 46.14 | 67.860 s |

On 32K/512, decode increases 41.52% and prefill decreases 10.65%.
The total request is 1.444 s slower (+2.17%). The long A/B controls
measure peak RSS 7,420,084–7,420,260 KiB versus
7,420,008–7,420,472 KiB. Process swaps and major faults are zero
in the four runs. Allocation accounting adds 313,787,648 bytes
of target workspace and 42,508,288 bytes of draft state, about
339.79 MiB in total. Candidate free VRAM before allocating the
draft cache is 733–736 MiB, implying roughly 693–696 MiB from
that snapshot after adding the draft cache. Peak VRAM was not
sampled directly.

All four output JSONL streams are byte-identical within each
workload, including every ID and the round-trip FP32 values
of selected logits. The decoded output is bounded explicitly
and EOG is masked in these two performance fixtures.
The native EOG and non-greedy sampling paths have no acceptance
result here.

| Workload | Accepted / proposed | Blocks retaining 1 / 2 / 3 inputs | Mean retained per block |
| --- | ---: | ---: | ---: |
| 129/64 | 34/54 (62.96%) | 7 / 6 / 14 | 2.259 |
| 32K/512 | 326/370 (88.11%) | 14 / 16 / 155 | 2.762 |

At 32K, the mean candidate spends about 51.0 ms per round inside
target verification and commit, 7.77 ms proposing and 0.48 ms
repairing. These phases exclude target-only tail rows. The
first break-even condition is removing more than 1.444 s from
the measured long request. Batch the prompt draft KV-only path
and preserve its exact writes as the first candidate. The larger
decode opportunity is the target's long-context path, especially
repeated exact attention passes within each verifier call.
Neither end-to-end gain has been measured. The production baseline
and unrelated architecture code and builds were untouched.

## Reproduction

Run from the root with EXP-2025 and EXP-2030 build trees present.
Each invocation of the array command is a separate manual run.

```bash
exp=experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy
out=build/bonsai-mtp-greedy-repro
previous=build/bonsai-mtp-commit-2030
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build.sh" "$out"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_UBATCH=1024 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1)
mtp=(BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1)
args=(taskset 0f0f "$out/bonsai-mtp-greedy" "$model" "$previous/commit.cubin" "$frozen" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" /usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" "${mtp[@]}" /usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" "${mtp[@]}" /usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" /usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
cmp "$out/a1.jsonl" "$out/b1.jsonl"
cmp "$out/a1.jsonl" "$out/b2.jsonl"
cmp "$out/a1.jsonl" "$out/a2.jsonl"
```

For 129/64 use context 1024, ubatch 128, generate 64 and
`"${prompt[@]:0:129}"` in the same commands.
[Measured output, receipts and hashes](data/)
