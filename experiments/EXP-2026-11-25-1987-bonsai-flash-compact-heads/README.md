# EXP-2026-11-25-1987 — Compact real Q heads in Bonsai Flash 6:1

**PROVEN for component acceleration and exact short generation.**
The staged FP32 path uses one tile per group of 16 real Q heads within a
KV head, dropping the four dummy rows previously inserted per 12 real
ones. Rows walk consecutive heads then tokens. Each preserves its own
causal key limit. The tile size, K/V staging, MMA accumulation and merge
arithmetic stay unchanged. At 128 queries, the group count falls from
256 to 192. The final group handles partial tails and odd token counts.

## Component A/B

RTX 4070, resident Q4_0 fixture, both arms use staged K/V and
EXP-1986 ordered merge at 2048 keys or above. The CUDA event span
includes stage, tile processing, and merge. All output bytes match.
Relative L2 against frozen Q4 tiles is at most 2.95e-4.

| KV rows / queries | Staged padded, ms | Staged compact, ms |
| --- | ---: | ---: |
| 65 / 9 | 0.0367 | 0.0317 |
| 257 / 33 | 0.0578 | 0.0543 |
| 8192 / 128 | 6.163 | 4.591 |
| 32768 / 128 | 26.076 | 17.677 |
| 1024 / 1024 | 2.041 | 1.676 |

At 32768/128, component elapsed time fell by 32.2%. Comparing
against the historical 220.944 ms frozen Q4 result for that shape
gives about 12.5x. That comparison is across runs and covers the
attention component only. EXP-1981's 3.63 ms remains a diagnostic
speed marker with a failed accuracy gate.

## Short model gate

4097/16, context 36864, ubatch 1024, Q4_0 K/V, raw input IDs from
EXP-1977, greedy, EOS masked, MTP disabled. A–B–B–A on one runtime
binary with staged FP32 and ordered merge enabled in both modes.
All four runs produce identical IDs and selected logits. The 16
output IDs also match the saved same-input PrismML request of EXP-1985.
The candidate saves 101.5 and 102.0 ms prefill in the two pairs,
about 0.65% throughput. Both modes allocate 1,720,400,640 B of
device workspace. Device-memory peaks and RAM were not sampled.
The candidate is available behind an experimental flag. Shared Apex
and Inkling sources are unchanged. No 32K/512 model benchmark was run.

A larger component gain now requires reducing fragment load overhead
or increasing compute utilization per CTA. Preserve the compact
causal mapping and the same per-row FP32 accumulation sequence
when testing warp matrix loads from aligned shared memory.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1987-bonsai-flash-compact-heads
bash "$exp/source/build.sh" build/bonsai-flash-compact-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-compact.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-flash-compact-repro/flash-compact.cubin \
  build/bonsai-flash-compact-repro/frozen-goose.ptx
```

Use the EXP-1983 4097/16 invocation with `bonsai-flash-compact`,
`flash-compact.cubin`, `BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1
BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_UBATCH=1024`.
Set `BONSAI_FLASH_COMPACT=1` for the candidate and 0 for control.

[Component timing](component.json). [Short model data](results.json).
