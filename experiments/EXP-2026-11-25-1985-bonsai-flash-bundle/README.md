# EXP-2026-11-25-1985 — Fuse two adjacent Bonsai KV partials

**REJECTED on greedy parity.** Keep the independently staged FP32 tile
path from EXP-1983. The tested short control matches the PrismML
reference on all 16 IDs. The bundle changes output ID 13 from 12904
to 2050 in both candidate runs.

## Attribution and candidate

One Nsight Systems trace of the staged control component fixture at
32768 KV rows and 128 query tokens. The three observed runs average
0.493 ms for K/V staging, 23.586 ms for 37 tile launches, and 5.217 ms
for 37 merge launches. The mean start-to-completion GPU interval is
29.335 ms. Reproduce the attribution by running the EXP-1983 probe under
`nsys profile --trace=cuda --sample=none --cpuctxsw=none`, exporting
SQLite, then running `source/trace-breakdown.py` on the export.

The experimental block handles two adjacent 128-key tiles. It keeps
running F32 value partials and combines the two softmax states in key
order inside the CTA. The global merge visits the resulting bundles
in increasing key order. Regrouping changes parentheses in FP32
arithmetic. The candidate uses 160 registers and 41,344 B shared
memory without spills. One partial per two tiles reduces scratch
traffic. The runtime scratch reservation stays unchanged.

## Component A/B

RTX 4070, the seeded Q4_0 fixture from EXP-1982, A–B–B–A GPU events.
Staging, tile sequence, and merge are included in both arms.

| KV rows / queries | Staged control, ms | Two-tile bundle, ms | Relative L2 vs staged |
| --- | ---: | ---: | ---: |
| 65 / 16 | 0.0352 | 0.0333 | 0 |
| 257 / 33 | 0.0591 | 0.0543 | 0 |
| 8192 / 128 | 6.264 | 4.596 | 2.51e-7 |
| 32768 / 128 | 29.276 | 22.058 | 4.67e-7 |
| 1024 / 1024 | 2.042 | 1.905 | 3.47e-8 |

All cases pass relative L2 < 0.003 and max absolute error < 0.002
against frozen Q4 tiles. The stricter differential L2 gate of 5e-6
against staged control also passes. At 32768/128 the candidate cuts
component time by 24.7%. This does not establish generation parity.

## Short model A/B

4097/16, context 36864, ubatch 1024, Q4_0 K/V, raw token IDs from EXP-1977,
greedy, EOS masked, and MTP disabled. A–B–B–A on the same Goose binary.
One separate matched PrismML HTTP comparison.

Both native arms allocate 1,720,400,640 B CUDA workspace. A takes
15.793/15.920 s prefill and B 15.776/15.792 s. The change is marginal
for this workload. One B decode sample also takes 57.6 ms longer
than its paired A. Peak RAM and VRAM were not sampled in this block.
No 32K/512 model benchmark was started.

The control and the candidate each reproduce their own IDs. The control
and PrismML match all 16. The candidate fails the required exact token
gate at ID 13. The user-facing path remains unchanged. The next
precision-preserving lever is to parallelize coefficient preparation
or partial loads while applying each original F32 multiply and add
in its original tile order.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1985-bonsai-flash-bundle
bash "$exp/source/build.sh" build/bonsai-flash-bundle-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-bundle.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-flash-bundle-repro/flash-bundle.cubin build/bonsai-flash-bundle-repro/frozen-goose.ptx
```

For the short model pair, use the EXP-1983 command and profile with
`bonsai-flash-bundle` and `flash-bundle.cubin`. Preserve
`BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_UBATCH=1024`.
The diagnostic mode is `BONSAI_FLASH_BUNDLE=1`.
[Stage attribution](attribution.json). [Component gate](component.json).
[Request data and hashes](results.json).
