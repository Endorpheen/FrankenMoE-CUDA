# EXP-2026-11-26-2073 — Exact full tiles for cumulative Bonsai Q8 gate/up

**Retained in the cumulative experimental checkpoint.** Specialize
the already active single-scale Q8 path for full 5,120-to-17,408
gate/up tiles. The HTTP candidate uses N128 instead of N64 only
for full token tiles. The selected layers, packed activations,
integer dot products, FP32 scaling and operation order do not change.
Other matrix paths keep EXP-2071 dispatch. Tails retain N64 generic.

## Component evidence

Both specialized widths match all FP32 outputs bitwise on four
actual 1024-token gate/up matrices. One RTX 4070 diagnostic trace
sample per combination, including the common Q8 pack, in ms:

| Projection | Existing N64 | Full N64 | Full N128 |
| --- | ---: | ---: | ---: |
| Layer 48 gate | 2.999 | 2.267 | 2.261 |
| Layer 48 up | 2.883 | 2.912 | 2.310 |
| Layer 63 gate | 2.989 | 2.259 | 2.257 |
| Layer 63 up | 2.374 | 2.263 | 2.258 |

The back-to-back samples show variability. They support checking
N128 on the complete request rather than assigning the same
percentage to the entire prefill. N128 uses 93 registers and
28,416 B shared memory, with no spills.

## RTX 4070, Bonsai HTTP 32K/512

One manual sequential pair with the same executable and CUBIN.
Both enable the entire EXP-2071 checkpoint. Only selection of the
full N128 Q8 tile differs. Raw EXP-1977 input IDs, context 36,864,
batch/ubatch 4096/1024, Q4_0 K/V, greedy and MTP off. PrismML is the
separate saved control.

| Mode | Prefill, tok/s | Decode, tok/s | Client, s | Process VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A, EXP-2071 control | 871.46 | 32.72 | 53.241 | 8932 | 7456880 |
| B, full N128 Q8 | 877.80 | 32.74 | 52.961 | 8932 | 7457464 |
| PrismML reference | 1076.60 | 47.63 | 41.174 | 8288 | 7651032 |

Within this one pair, prefill saves 0.272 s (+0.73%) and the
request saves 0.281 s. All 512 IDs and response text match across
both arms and PrismML. Process swap is zero. No new fixed-history
logits were collected. Expanded quality review of inherited
numerical changes remains open. Stable EXP-2049 and other
architecture paths were not changed. The cumulative prefill gap
to PrismML is 6.893 s.

## Reproduce

Use the EXP-2067 device source and EXP-2071 combined build.
Set `FRANKENMOE_CHECKPOINTS` to the saved build tree.

```bash
exp=experiments/EXP-2026-11-26-2073-bonsai-q8-full-tiles
component=build/bonsai-q8-full-tiles-2073
out=build/bonsai-combined-q8-full-2073
bash "$exp/source/build.sh" "$component" build/bonsai-mmq-width6144-2067/pipeline.cl
bash "$exp/source/build-combined.sh" "$out" build/bonsai-combined-half2-down-2071 "$component"
```

Use the saved inventory with `source/check.py`. For HTTP preserve
all EXP-2071 flags. Set `BONSAI_FFN_Q8_FULL128=0` for A and
omit it for B. Keep the exact raw 32K/512 request and frozen PTX.

[Component trace](data/component.json) and [full-request evidence](data/results.json).
