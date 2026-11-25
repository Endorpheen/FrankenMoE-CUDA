# EXP-2026-11-25-2030 — Device-resident Bonsai target prefix commit

**Verified experimental target transaction.** The incoming
target anchor is already known and always committed. The
verifier may retain one, two or three of the scheduled inputs.
A zero-input commit is invalid. No production mode is enabled.

## Mechanism

Each GDN layer has two spare device slots for each of its
recurrent state and convolution history. For a three-row
transaction, the normal single-row GDN kernel advances through
the two spare slots and writes the third row back to the
current state slot. A model-scoped Goose hook defers the
normal scratch swap for the duration of this transaction.
A new CUDA convolution kernel uses the serial expression and
emits all three histories directly into their slots. Partial
commit exchanges the ownership of a spare and the current
state and history. Full commit keeps the current ownership.
Attention writes append-only KV rows; moving the cursor makes
the rejected suffix invisible and available for overwrite.
The normal serial path can subsequently use the selected state.

A transaction must succeed and then commit exactly once before
the next evaluation. A failed transactional graph poisons the
experimental context. It has no zero-prefix cancellation
because the proposed third state overwrites the initial one.

## Evidence

RTX 4070, Bonsai PQ2_0-mtp, Q4_0 K/V, baseline Q8 decode.
Two short prefixes (129 and 255) use context 1024 and ubatch
128. The long prefix of 32,767 tokens uses context 36,864
and ubatch 1024. This crosses the decode grouped-attention
geometry boundary during verification.

| Prefix | Retained inputs tested | Forced rejections | Compared state immediately after commit |
| ---: | --- | ---: | ---: |
| 129 | 1, 2, 3 | 2 | 159,289,344–159,326,208 bytes |
| 255 | 1, 2, 3 | 2 | 161,611,776–161,648,640 bytes |
| 32,767 | 1, 2, 3 | 2 | 760,872,960–760,909,824 bytes |

For each case, the accepted output logit rows, visible KV and
recurrent state match the serial oracle byte for byte before
any follow-up work. The next ordinary decode step, which
overwrites the first discarded KV row, also matches in logits
and state. Tests reject a zero-prefix commit, a double commit
and evaluation while a commit is pending. Across all cases the
probe checks 6,704,640 logits.

The transaction adds 313,786,368 bytes (299.25 MiB) of device
workspace compared with EXP-2029. In the long diagnostic the
reported device memory available after allocation is
769,654,784 bytes (734 MiB). Host snapshots are for the serial
oracle only. The target transaction and selection make no
state transfers to or from the host and do not replay retained
inputs. The commit mutates host pointer metadata and the
position cursor. Target time with the long prefix was
48.58–55.88 ms for three rows. The three measurements use
different proposals and are diagnostic rather than a paired
throughput result. Peak VRAM/RAM, prefill/decode speed and
natural acceptance are unmeasured.

This mechanism applies to recurrent target paths with an
always-kept anchor, distinct intermediate planes and
append-only, cursor-masked KV. The next gate is to connect
the real MTP drafter to the exact target transaction and
measure accepted tokens and full greedy decode against
ordinary Bonsai. No Apex or Inkling code or build is changed.

## Reproduction

Run from the root with EXP-2025, EXP-2027, EXP-2028 and
EXP-2029 prerequisite build trees.

```bash
exp=experiments/EXP-2026-11-25-2030-bonsai-mtp-device-commit
out=build/bonsai-mtp-commit-repro
previous=build/bonsai-mtp-head-2025
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build.sh" "$out"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"][:32767])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_TARGET_PROBE=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1 BONSAI_MTP_DEVICE_COMMIT=1)
"${profile[@]}" BONSAI_UBATCH=128 taskset 0f0f "$out/target-commit" "$model" "$out/commit.cubin" "$previous/frozen-goose.ptx" 1024 5 --ignore-eos "${prompt[@]:0:129}"
"${profile[@]}" BONSAI_UBATCH=128 taskset 0f0f "$out/target-commit" "$model" "$out/commit.cubin" "$previous/frozen-goose.ptx" 1024 5 --ignore-eos "${prompt[@]:0:255}"
"${profile[@]}" BONSAI_UBATCH=1024 taskset 0f0f "$out/target-commit" "$model" "$out/commit.cubin" "$previous/frozen-goose.ptx" 36864 5 --ignore-eos "${prompt[@]}"
```

[Measured outputs and hashes](data/)
