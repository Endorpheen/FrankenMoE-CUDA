# EXP-2026-11-25-2049 — Merge Bonsai Flash tiles within one CUDA block

**Accepted for the Bonsai HTTP candidate.** Each two-panel workgroup walks
the original 128-key tiles in order. It retains the accumulated values in
registers and stores the row maxima, denominators and merge coefficients in
the unused padding of its shared Q tile. K/V staging, causal masking,
per-tile FP16 operands and ordered FP32 recurrence match EXP-2019.
The new path writes the final attention output directly. It skips global
partial writes and the separate merge dispatch while retaining the existing
workspace allocations and fallback route.

The host selects this kernel in the existing paired-panel ordered path
with at least 512 queries. Disable it with `BONSAI_FLASH_FUSED_ORDERED=0`.
The compiler uses 125 registers versus 78 and the same 50,176 bytes of
dynamic shared memory. Neither kernel spills. The component gate obtains
12,767,232 identical FP32 outputs across three shapes, including
32,767 keys / 1,023 queries.

## RTX 4070, Bonsai HTTP 32K/512

A–B–B–A with one process and request at a time. Same binary, image and
EXP-1977 raw request. Context 36,864, batch/ubatch 4096/1024, Q4_0 K/V,
greedy, MTP off.

| Arm | Prefill, tok/s | Decode, tok/s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A1 | 646.14 | 32.67 | 66.381 | 8932 | 7456704 |
| B1 | 748.16 | 32.70 | 59.447 | 8932 | 7457540 |
| B2 | 748.03 | 32.70 | 59.454 | 8932 | 7457780 |
| A2 | 649.39 | 32.70 | 66.109 | 8932 | 7457116 |

Aggregate prefill improves **647.76 → 748.10 tok/s (+15.49%)**.
Prefill wall falls **50.587 → 43.802 s**. The first pair saves 6.915 s
and the reverse saves 6.654 s. These are end-to-end measurements of the
dispatch change, without a new trace attributing the savings to individual
kernels. Decode remains around 32.7 tok/s. All four responses match all
512 EXP-2019/PrismML token IDs and the completion text. Process swap is
zero. VRAM is sampled after the request.

The local multi-model package substitutes only the Bonsai HTTP executable
and HTTP CUDA image. Apex and Inkling executables, selected arguments and
environment files are byte-identical to the EXP-2019 package. Bonsai MTP
artifacts and the common selector are byte-identical as well. No new
generation runs were performed for those unchanged package paths.

The current PrismML control is 30.436 s / 1,076.60 tok/s, leaving
13.365 s to recover for prefill parity on the same request.

## Reproduce

Build the EXP-2019 HTTP reference into `build/bonsai-http-reproduction`.
Point `FRANKENMOE_CHECKPOINTS` to the saved build tree.

```bash
exp=experiments/EXP-2026-11-25-2049-bonsai-flash-fused-merge
out=build/bonsai-flash-fused-2049
bash "$exp/source/build.sh" "$out" build/bonsai-http-reproduction
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" \
  build/bonsai-http-reproduction/package/cuda/frozen-goose.ptx "$out/pipeline.cubin"
```

For HTTP, use the EXP-2019 profile and set `BONSAI_CUDA_IMAGE` to the
candidate image. Use `BONSAI_FLASH_FUSED_ORDERED=0` for A. Omit it for B.
The release staging script accepts `FRANKENMOE_BONSAI_HTTP_BUILD="$out"`.

[Machine-readable results and hashes](data/results.json).
