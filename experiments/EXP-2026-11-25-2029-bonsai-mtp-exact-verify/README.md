# EXP-2026-11-25-2029 — Bit-exact three-row Bonsai target verification

**Verified experimental target component.** The EXP-2028
three-token PQ2 projections share weight loads and preserve
the host model's Q8 decode arithmetic. This candidate keeps
that weight reuse and dispatches the same single-row kernels
for the narrow FP32 projections, decay, convolution, GDN,
RoPE and grouped attention. The CUDA image and Goose graph
are unchanged from EXP-2028. The backend applies each
shape-sensitive operation in token order, with independent
RoPE and attention configs per position. GDN alternates the
saved and scratch state buffers within each layer so the odd
three-row block leaves the final state in the original next
state buffer expected by Goose.

## Correctness

RTX 4070, Bonsai PQ2_0-mtp, context 1024, ubatch 128,
Q4_0 K/V, baseline Q8 decode. Use prefixes 32, 129 and 255
of the EXP-1977 input. At 255 the three verification rows
cover keys 256, 257 and 258, crossing a Q4 attention tile.

- All 2,234,880 logits in the three full-block comparisons
  are byte-identical to ordinary decode. All 128 state
  planes per prefix also match.
- Substituting a false third input leaves the first two
  logit rows bitwise identical on all three prefixes.
- Forced rejection, checkpoint restore and serial replay
  preserve all compared state bytes and following logits.
  The state check includes the active KV prefix, GDN
  convolution histories and recurrent states.

## Target-only timings

At 129 input tokens, warm up one serial and one batched
evaluation, then time A–B–B–A. The timer includes target
dispatch and the same three rows of host logits. It excludes
checkpoint restore, input loading, prefill, proposal generation
and acceptance. Each measured result is checked against the
serial oracle.

| Arm | First, ms | Second, ms | Mean, ms |
| --- | ---: | ---: | ---: |
| A, three sequential target steps | 68.483 | 68.678 | 68.581 |
| B, one three-row target call | 27.922 | 27.836 | 27.879 |

The batch saves 40.702 ms (59.3%) for this measured target block.
This is a fixed three-token verification component. Full-request
prefill and decode, peak VRAM/RAM and natural acceptance are
unmeasured. The adapter reserves 1,280 additional device bytes
relative to EXP-2028 for per-row configs. The existing diagnostic
rollback still copies host snapshots outside the timed interval.

Direct commit now passes for a complete three-token block at
the checked prefixes. Rejected proposals still need a device
checkpoint and bounded rollback strategy before measuring
integrated decode. Preserve partial acceptance, exact replacement
and recurrent state ownership across generations. The same
division of weight-sharing projections and token-ordered state
updates is a candidate for other recurrent verifiers with an
equivalent numerical contract. Apex and Inkling use no part
of this experimental adapter.

## Reproduction

Run from the root with EXP-2025, EXP-2027 and EXP-2028 build trees.

```bash
exp=experiments/EXP-2026-11-25-2029-bonsai-mtp-exact-verify
out=build/bonsai-mtp-exact-repro
previous=build/bonsai-mtp-head-2025
cubin=build/bonsai-mtp-q8-2028/verify.cubin
req=experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$exp/source/build.sh" "$out"
read -r -a prompt < <(python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"][:255])' "$req")
profile=(env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 BONSAI_UBATCH=128 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1 BONSAI_MTP_TARGET_PROBE=1 BONSAI_MTP_VERIFY_Q8=1 BONSAI_MTP_VERIFY_EXACT=1)
"${profile[@]}" taskset 0f0f "$out/target-exact" "$model" "$cubin" "$previous/frozen-goose.ptx" 1024 4 --ignore-eos "${prompt[@]:0:32}"
"${profile[@]}" BONSAI_MTP_VERIFY_TIMING=1 taskset 0f0f "$out/target-exact" "$model" "$cubin" "$previous/frozen-goose.ptx" 1024 4 --ignore-eos "${prompt[@]:0:129}"
"${profile[@]}" taskset 0f0f "$out/target-exact" "$model" "$cubin" "$previous/frozen-goose.ptx" 1024 4 --ignore-eos "${prompt[@]}"
```

[Measured outputs and hashes](data/)
