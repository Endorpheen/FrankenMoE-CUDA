# EXP-2026-11-25-1986 — Parallel coefficients for ordered Bonsai Flash merge

**PROVEN for component acceleration and exact short output.** Keep EXP-1983
as the model-level reference until the longer performance gate. This path
does not carry the rejected EXP-1985 bundle. Warp zero prepares coefficients
for each batch of 32 consecutive tiles with an inclusive running maximum.
The output work is distributed over four warps. Each output element and
the normalizer still apply the original FP32 multiplies and adds in the
same tile order. The inputs, causal limit, and approximate reciprocal
match the control. CUDA global workspace is unchanged.

## Component results

RTX 4070. A–B–B–A event intervals contain K/V staging, tile computation and
the corresponding merge. The control is the staged FP32 Flash of EXP-1983.
All five output tensors match byte for byte. The maximum L2 relative to
the separately computed frozen Q4 control is 0.0002952.

| KV rows / queries | Staged control, ms | Ordered parallel merge, ms |
| --- | ---: | ---: |
| 65 / 16 | 0.0360 | 0.0319 |
| 257 / 33 | 0.0583 | 0.0561 |
| 8192 / 128 | 6.273 | 6.151 |
| 32768 / 128 | 29.279 | 26.084 |
| 1024 / 1024 | 2.043 | 2.028 |

A separate Nsight Systems check at 32768/128 observed mean merge
kernel time of 5.560 ms in the staged control and 2.225 ms with
ordered parallel coefficients. The unprofiled full attention component
takes 10.9% less time than staged control. It is about 8.5x faster than the
historical same-fixture Q4 attention component (220.944 ms, EXP-1982).
These are component timings. Large improvements now require reducing
the tile workload, which accounts for most of the remaining interval.
A candidate should fill tensor rows with consecutive real Q heads
instead of spending four out of each 16 rows on dummy heads.

## Short generation gate

4097/16, first 4097 raw IDs from EXP-1977, context 36864, ubatch 1024,
Q4_0 K/V, greedy, EOS masked, no MTP. One unprofiled execution per
mode on the same executable. Both returned byte-identical token IDs
and selected logits, including the 13th token that EXP-1985 changed.
All 16 IDs matched the saved same-input PrismML result in EXP-1985.
Prefill was 15.860 s and 15.826 s, decode 0.973 s in both modes.
This single pair is a correctness check and gives no material evidence
of a short-model throughput increase. Both modes allocate 1,720,400,640
bytes of GPU workspace. No 32K/512 model request was run.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1986-bonsai-flash-ordered-merge
bash "$exp/source/build.sh" build/bonsai-ordered-merge-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-merge.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-ordered-merge-repro/flash-ordered-merge.cubin \
  build/bonsai-ordered-merge-repro/frozen-goose.ptx
```

Use the EXP-1983 native profile with `bonsai-ordered-merge`,
`flash-ordered-merge.cubin` and `BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1`.
The candidate is opt-in with `BONSAI_FLASH_ORDERED_MERGE=1` and activates
for staged KV at 2048 keys and above. CUDA code shared with Apex and
Inkling is unchanged. Run the component test under
`nsys profile --trace=cuda --sample=none --cpuctxsw=none` to reproduce
the attribution, export SQLite, and use `source/trace-breakdown.py`.

[Component data](component.json). [Trace totals](attribution.json).
[Short request and hashes](results.json).
