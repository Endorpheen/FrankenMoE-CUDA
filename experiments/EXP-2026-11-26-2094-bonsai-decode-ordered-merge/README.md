# EXP-2026-11-26-2094 — Exact ordered merge for long Bonsai decode

**Retained in the cumulative experimental checkpoint.** Apply the
existing EXP-2036 verifier merge to the head-major partials of a
single Bonsai query. No change to stable runtime or to the inherited
numerical quality status of EXP-2093 direct Q4 MMA.

## Mechanism and component probe

Use one 128-thread CTA per head. The first warp prepares rescaling
coefficients using prefix maxima. Four warps process two columns per
thread, each traversing the KV tiles in the original sequence. Skip
zero-mass tiles and retain the original FP32 numerator, denominator and
division semantics. Use this path only for the existing long
single-query dispatch at 32,768 or more visible keys. The final prompt
evaluation at 32,768 therefore also uses it. No change to the tile
producer, Q4 K/V, global scratch, or resident allocation. SM89 uses
35 registers, 400 B of shared memory and zero spills. The previous
96 CUDA text sections are byte-identical.

Seven one-query fixtures, 6,144 output values each, including an
all-zero-mass head, sparse zero masses, signed-zero maxima and
incomplete tiles. All 43,008 values match the frozen Goose merge
bit-for-bit. Each event sample batches 20 identical launches.

| Visible keys | Frozen merge, µs | Parallel merge, µs |
| ---: | ---: | ---: |
| 1 | 5.031 | 4.561 |
| 255 | 5.015 | 4.531 |
| 256 | 5.004 | 4.522 |
| 8192 | 36.301 | 4.664 |
| 32767 | 137.935 | 12.493 |
| 32768 | 138.208 | 12.493 |
| 33279 | 140.648 | 13.056 |

All long cases clear the 30% merge time-reduction threshold.

## RTX 4070, HTTP 32K/512

One sequential pair. Same exact GGUF, raw request, binary, CUBIN,
frozen PTX, effective argv and affinity. Context 36864,
batch/ubatch 4096/1024, Q4_0 K/V, greedy, seed 123, EOS masked and
MTP off. Only `BONSAI_DECODE_ORDERED_MERGE` differs. Both runs pass
the cold-prefill validator with state reset, 32,768 evaluated input
tokens and zero reused prompt tokens. Prelaunch GPU temperature
was 55 °C for A and 43 °C for B, with unlocked clocks. All valid
runs are shown.

| Run | Prefill, tok/s | Decode, tok/s | Decode, s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A · Frozen merge | 1019.45 | 34.69 | 14.729 | 46.895 | 8932 | 7458032 |
| B · Parallel merge | 1027.62 | 37.27 | 13.711 | 45.621 | 8932 | 7457752 |
| Saved PrismML | 1076.60 | 47.63 | 10.728 | 41.174 | 8288 | 7651032 |

Decode improves 7.42% and saves 1.018 s in this pair. The client
saves 1.273 s. The observed 255 ms prefill difference is not
attributed to this merge change. Both responses match 512 PrismML
IDs and text, both report zero swap, and peak RAM is comparable.
No new quality tasks were run for the exact merge. The EXP-2093
numerical candidate still needs expanded quality validation. Apex
and Inkling packaged artifacts are identical to the stable package.
No new cross-architecture model runs were made.

## Executed graph

One separate cold-gated Nsight Systems capture of B. Contains all
512 vocabulary readbacks and matches 512 IDs and text. Compared to
EXP-2093, the ordered launch names and geometries match after
normalizing the replaced merge name and block size. Sixteen
replacements occur in the final prompt evaluation and 8,176 in
decode. Other launch geometries stay the same.

| Decode trace measure | EXP-2093 | EXP-2094 |
| --- | ---: | ---: |
| Direct Q4 MMA | 2.339 s | 2.377 s |
| Ordered merge | 1.109 s | 0.100 s |
| Combined Attention | 3.447 s | 2.477 s |
| GPU gaps | 0.960 s | 0.966 s |

The sum of merge kernel times drops by 1.008 s. The separate traced
request gives 1027.10 / 36.82 tok/s, 45.803 s client and 8,944 MiB
post-request VRAM. It is not part of the unprofiled pair. Compared
with EXP-2091 PrismML, the remaining decode Attention gap is about
1.527 s. The merge itself is now a small part of that gap.
Prioritize direct MMA tile work for decode and PQ2 for prefill.

## Reproduce

Use the EXP-2093 build and profile. Set `FRANKENMOE_CHECKPOINTS`
to the artifact tree. Build with Clang 18, CUDA ptxas for SM89
and GCC 13.

```bash
exp=experiments/EXP-2026-11-26-2094-bonsai-decode-ordered-merge
out=build/bonsai-decode-ordered-merge-2094
base=build/bonsai-decode-direct-mma-2093
bash "$exp/source/build.sh" "$out" "$base"
python3 "$exp/source/verify-code.py" "$base/pipeline.cubin" "$out/pipeline.cubin"
FRANKENMOE_BENCH_COLD_LOG=1 bash "$exp/source/build-http.sh" "$out" "$base"
```

Use EXP-2036's `source/check.py CORE_DIRECTORY INVENTORY CUBIN
FROZEN_PTX --single-query` for component checks. For HTTP load
the EXP-2093 profile and set `BONSAI_DECODE_ORDERED_MERGE=0` for A
and `=1` for B. Send the
[exact EXP-1977 raw request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
and use the cold-prefill validator before and after each arm.
The raw trace is at `build/bonsai-decode-ordered-merge-2094/trace/trace.sqlite`.

[Component evidence](data/component.json), [CUDA identity](data/device-code.json),
[full pair and cold receipts](data/results.json), and [executed graph](data/attribution.json).
