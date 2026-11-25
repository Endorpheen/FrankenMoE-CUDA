# EXP-2026-11-25-1989 — Flash copy overlap and updated Bonsai loss map

**INCONCLUSIVE for performance promotion.** The single-buffer async
EXP-1988 path remains the reference for Flash. The candidate queues
the next K or V chunk with `cp.async` while it consumes the current one.
Q/K shared rows use an aligned XOR layout. Q and score buffers are
recycled in the value phase. Shared allocation increases from 41,216
to 49,152 bytes per CTA; both SM89 builds use 64 registers, no spills.
The separate L2 candidate replaces the copy cache operator with
`cp.async.cg` and a 128-byte L2 prefetch hint.

## Component test

RTX 4070, same fixed Q/query fixture as EXP-1988. Median of six timings
per mode, order A–B–C–C–B–A repeated three times. The scope includes
conversion of resident Q4_0 rows, tiles and merge. All three mode
outputs match byte for byte on every shape; the L2 limit from
EXP-1982 holds unchanged.

| KV / queries | Single buffer, ms | Double .ca, ms | Double .cg/L2, ms |
| --- | ---: | ---: | ---: |
| 65 / 9 | 0.0234 | 0.0236 | 0.0258 |
| 257 / 33 | 0.0378 | 0.0393 | 0.0408 |
| 8192 / 128 | 2.024 | 2.009 | 2.208 |
| 32768 / 128 | 8.302 | 8.134 | 8.886 |
| 1024 / 1024 | 0.953 | 0.992 | 1.104 |

At 32768/128, .ca saves 0.168 ms (2.0% wall time) and .cg/L2 loses
0.584 ms. At 1024/1024, .ca loses 0.039 ms (4.1% wall time).
The combined layout and overlap change does not isolate either effect.
Retain the single-buffer default. The L2 variant is component-only.

## Short generation gate and current profile

1025/16, first 1025 raw IDs from EXP-1977, context 36864, ubatch 1024,
Q4_0 K/V, greedy, EOS masked, MTP off. One execution per mode. IDs and
selected F32 logits are byte-identical. Both allocate 1,720,400,640 B
CUDA workspace. Native prefill wall times were 4.055 and 3.828 s;
decode times were 0.964 and 0.965 s. This one pair verifies output,
and does not promote the double buffer on full-pipeline performance.

A separate **4097/16** Nsight 2026 trace used the single-buffer
async control with the same context and ubatch. The CUDA intervals
exclude model upload and split after the first vocabulary matvec.

| GPU kernel group | Phase | Time, ms |
| --- | --- | ---: |
| PQ2 MMQ | Prefill | 9898.49 |
| Batched GDN | Prefill | 4319.06 |
| Decay/beta projections | Prefill | 349.62 |
| Flash stage, tile and merges | Prefill | 211.86 |
| First-to-last kernel span | Prefill | 15492.57 |
| Ordered PQ2 matvec | Decode | 620.57 |
| Q4 attention tiles | Decode | 165.75 |
| Norm | Decode | 103.13 |
| First-to-last kernel span | Decode | 975.69 |

At this short request, eliminating the measured Flash path would
recover only about 1.37% of the prefill GPU span. PQ2 MMQ and GDN
account for about 91.8%. This short trace does not establish the
Flash share on a full 32K request. Preserve the proven Flash
sequence and prioritize the largest measured prefill costs. Apply
the same component-to-model checks to PQ2 MMQ. The 32K/512 target
and release integration remain open. No new long model request
was made. Shared Apex and Inkling sources are unchanged.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1989-bonsai-flash-double-buffer
bash "$exp/source/build.sh" build/bonsai-flash-double-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-double.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-flash-double-repro/flash-double.cubin \
  build/bonsai-flash-double-repro/frozen-goose.ptx
```

Use EXP-1988 settings with `bonsai-flash-double` and `flash-double.cubin`.
Set `BONSAI_FLASH_ASYNC=1` in both arms and `BONSAI_FLASH_DOUBLE=1`
only in the diagnostic arm. For the current profile, run the
4097/16 control under Nsight with `--trace=cuda --sample=none
--cpuctxsw=none`, export SQLite, and analyze it with
`source/profile-summary.py`. The raw trace remains local at
`build/bonsai-flash-double-1989-runtime/full-short-profile.nsys-rep`.

[Component data](component.json). [Short output data](results.json).
[Current loss map](profile.json).
