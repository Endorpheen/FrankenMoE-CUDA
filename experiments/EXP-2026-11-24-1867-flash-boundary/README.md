# EXP-2026-11-24-1867 — Hoist the causal Flash tile boundary

**REJECTED in standalone GPU tests.** The control is the accepted
EXP-1863 causal Flash cubin, including the full-tile branch. Two
variants precompute the number of completely visible KV tiles.
The first also removes the shared-mask path from its isolated direct
causal entry, which always receives `register_causal=true`. The
second leaves the shared-mask path intact. Neither changes the
FP32 reduction order, the positive-zero FMA or the causal tail.

Both variants matched the original output bytes in ten fixtures.
Eight timed fixtures used 4096 queries and 4096–32768 KV tokens. Two
additional fixtures cover ragged queries and tile boundaries. Each
timed comparison used six alternating blocks of three launches.

| Variant | Entry frame | Registers | Kernel time change |
| --- | ---: | ---: | ---: |
| EXP-1863 | 120 B | 255 | Reference |
| Causal only and hoist | 264 B | 255 | +33.1–38.6% |
| Hoist only | 128 B | 255 | −0.39% to +1.80% |

The causal-only version is slower on every measured shape and its
late-shape median rose from 31.19 to 42.04 ms. Its generated
allocation and static spill counts also changed. The experiment
does not isolate how much of the lost time comes from spills.
The hoist-only version improved on one shape by 0.39%, within the
observed sampling variation. No runtime candidate was built or run.

Reproduce using the frozen EXP-1863 `full-flash-mma.goose` and
`full.cubin` as the baseline. Apply one of the [source patches](source)
at a time. Emit the fragment with the KeelWing toolchain and compile
with the Clang 18 and CUDA 12.6 flags in
[EXP-1859](../EXP-2026-11-24-1859-flash-interior/README.md).
Extract only `goose_flash_causal_mma` for SM89.
Copy the accepted cubin as `control.cubin` and verify the manifest.

```sh
python3 probe.py
sed 's/"candidate"/"hoist"/g' probe.py > probe-hoist.py
python3 probe-hoist.py
```

See [raw causal-only timings](probe.log), [raw hoist timings](probe-hoist.log)
and [structured samples](results.json). Continue from EXP-1863.
