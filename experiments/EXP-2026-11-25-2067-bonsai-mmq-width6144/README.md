# EXP-2026-11-25-2067 — Full 6144-input tiles on cumulative Bonsai MMQ

**Retained in the cumulative experimental checkpoint.** Add a
width-6144 specialization to EXP-2065's existing 5120 and 17408
entries. The GGUF inventory identifies 64 width-6144, output-5120
matrices across SSM and attention output. Element and block order,
FP16 K32 partials and the final FP32 sum are unchanged. Only full
64×128 output tiles use the specialized entry. Tails and other
widths keep the prior dispatch.

The two real 1024-token output matrix probes match bitwise. Their
single diagnostic CUDA samples, including activation pack, are
1.103 → 1.012 ms for `ssm_out` and 1.101 → 1.017 ms for
`attn_output`. Both beat the preregistered 5% component threshold.
The new entry uses 80 registers rather than 94, the same 27,648 B
shared memory, and no spills. The CUDA residency bound grows from
two to three active blocks per SM, not a measured occupancy change.

## RTX 4070, Bonsai HTTP 32K/512

One manual sequential pair on the same host and CUBIN. Both arms
enable the EXP-2065 cumulative arithmetic and full tiles. Only the
6144 specialization changes. Exact raw EXP-1977 input IDs,
context 36,864, batch/ubatch 4096/1024, Q4_0 K/V, greedy, MTP off.
PrismML is the separate saved reference.

| Mode | Prefill, tok/s | Decode, tok/s | Client, s | Process VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A, EXP-2065 control | 837.67 | 32.68 | 54.778 | 8932 | 7457068 |
| B, width 6144 added | 849.18 | 32.71 | 54.233 | 8932 | 7457596 |
| PrismML reference | 1076.60 | 47.63 | 41.174 | 8288 | 7651032 |

Within this pair, prefill saves 0.530 s (+1.37% throughput).
The full request saves 0.544 s. Relative to the earlier separate
EXP-2065 candidate run at 845.33 tok/s, the new candidate is only
about 0.46% faster. Do not treat the within-pair delta as a
repeatable magnitude. All 512 IDs and response text match PrismML
and both arms. Process swap is zero. No new fixed-history logits
were captured. Expanded quality review of all inherited numerical
changes is still open. This does not update the stable EXP-2049
line or any other model's dispatch.

The experimental prefill gap to the saved PrismML run is 8.151 s.
The [checkpoint registry](../bonsai-checkpoints.json) retains
EXP-2065 separately as the prior experimental checkpoint.

## Reproduce

Use the EXP-2065 cumulative source and device image. Set
`FRANKENMOE_CHECKPOINTS` to the saved build tree.

```bash
exp=experiments/EXP-2026-11-25-2067-bonsai-mmq-width6144
out=build/bonsai-mmq-width6144-2067
base=build/bonsai-cumulative-full-tiles-2065
bash "$exp/source/build.sh" "$out" "$base/pipeline.cl"
bash "$exp/source/build-http.sh" "$out" "$base"
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin"
```

Set `BONSAI_MMQ_HALF_ACC=1` in both arms. Set
`BONSAI_MMQ_HALF_FULL6144=0` for A and omit it for B. Use the fixed
HTTP profile, candidate CUBIN and frozen PTX.

[Component trace](data/component.json) and [full-request evidence](data/results.json).
