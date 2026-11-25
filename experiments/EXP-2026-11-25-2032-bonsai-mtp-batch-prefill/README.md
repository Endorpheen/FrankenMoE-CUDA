# EXP-2026-11-25-2032 — Batch the Bonsai draft KV prefill

**Verified improvement within the experimental MTP path.**
The EXP-2031 greedy decode and exact target are the control.
During prompt prefill the candidate batches draft K/V-only
catch-up in groups of at most 128 rows. It constructs the
shifted target hidden rows on device and uses the existing
Goose embedding, norms, signed Hadamard, rotary, and Q4 KV
writers. The Q4 projections unpack each weight once for four
tokens. Separate accumulators preserve the serial FMA and
warp reduction order. Prompt chunks and the final one-token
fallback preserve the absolute positions and previous hidden row.
Draft output and accepted decode history still use the
EXP-2031 path. No production serving changes.

## Correctness

RTX 4070, Bonsai PQ2_0-mtp, Q4_0 K/V. On 257/16 with
ubatch 256 and context 1024, the two modes produce the same
296,064 raw K/V bytes, 16 output IDs, selected logits and
acceptance counters. The two 128-row chunks exercise the
boundary between target hidden spans.

On 32K/512, context 36,864 and ubatch 1024, all four arms
produce identical raw draft K/V prefill captures (37,748,736
bytes each), and match the EXP-2031 output byte for byte
(512 IDs and round-trip selected FP32 logits). Natural draft
acceptance is 326 of 370 in each run, with 14/16/155
verifier transactions retaining 1/2/3 inputs.

## Performance

Same binary, A–B–B–A, one invocation at a time. A uses the
existing serial draft fill, B the batch fill. Decode excludes
the token provided by the prefill. Both modes capture raw
K/V between the timing intervals for parity verification.
The dump is outside both timed intervals. Rates below are
means of per-run rates.

| Arm | Prefill, tok/s | Decode, tok/s | Prefill, s | Decode, s | Sum, s |
| --- | ---: | ---: | ---: | ---: | ---: |
| A1 | 581.25 | 46.41 | 56.375 | 11.012 | 67.386 |
| B1 | 610.73 | 46.08 | 53.654 | 11.090 | 64.745 |
| B2 | 609.44 | 46.20 | 53.768 | 11.061 | 64.829 |
| A2 | 575.54 | 46.17 | 56.935 | 11.068 | 68.003 |

**Mean prefill 578.39 → 610.08 tok/s (+5.48%).
Mean decode 46.29 → 46.14 tok/s (−0.32%, direction
differs in the two pairs). Sum 67.695 → 64.787 s
(−2.908 s, −4.30%).** The EXP-2031 historical ordinary
decode request took 66.416 s. Its comparison with the
current MTP run is across experiments.

The candidate reuses target workspace and requests 216
additional device bytes for its private RoPE config.
ptxas reports 40 registers and zero spills. Peak RSS is
7,419,548–7,420,480 KiB in A and 7,420,348–7,420,472 KiB
in B. There are no measured process swaps or major faults.
Peak VRAM was not sampled. Apex and Inkling use no part
of this isolated Bonsai adapter.

The reusable technique is a KV-only prefix graph which
consumes shifted hidden rows directly on device and preserves
the exact scalar arithmetic inside each token. Reuse depends
on identical weight packing, positional semantics and state
layout. The long-context exact target call is now the largest
measured component of speculative decode.

## Reproduction

Run from the root with EXP-2025, EXP-2030 and EXP-2031
build trees present. Each command is an individual run.

```bash
exp=experiments/EXP-2026-11-25-2032-bonsai-mtp-batch-prefill
out=build/bonsai-mtp-prefill-repro
frozen=build/bonsai-mtp-head-2025/frozen-goose.ptx
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build.sh" "$out"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_UBATCH=1024 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_GREEDY=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1)
args=(/usr/bin/time -f 'max_rss_kib=%M major_faults=%F swaps=%W' taskset 0f0f "$out/bonsai-mtp-batch" "$model" "$out/greedy.cubin" "$frozen" 36864 512 --ignore-eos "${prompt[@]}")
"${profile[@]}" BONSAI_MTP_KV_CAPTURE="$out/a1-kv.bin" "${args[@]}" >"$out/a1.jsonl" 2>"$out/a1.log"
"${profile[@]}" BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_KV_CAPTURE="$out/b1-kv.bin" "${args[@]}" >"$out/b1.jsonl" 2>"$out/b1.log"
"${profile[@]}" BONSAI_MTP_BATCH_PREFILL=1 BONSAI_MTP_KV_CAPTURE="$out/b2-kv.bin" "${args[@]}" >"$out/b2.jsonl" 2>"$out/b2.log"
"${profile[@]}" BONSAI_MTP_KV_CAPTURE="$out/a2-kv.bin" "${args[@]}" >"$out/a2.jsonl" 2>"$out/a2.log"
cmp "$out/a1-kv.bin" "$out/b1-kv.bin"
cmp "$out/a1-kv.bin" "$out/b2-kv.bin"
cmp "$out/a1-kv.bin" "$out/a2-kv.bin"
cmp "$out/a1.jsonl" "$out/b1.jsonl"
cmp "$out/a1.jsonl" "$out/b2.jsonl"
cmp "$out/a1.jsonl" "$out/a2.jsonl"
```

[Measured outputs and hashes](data/)
