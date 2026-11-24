# EXP-2026-11-24-1875 — Fuse the prefill MoE epilogue into the routed slot

**PROVEN and accepted as the research prefill checkpoint.** Based on
EXP-1863 and the exact GPU kernel from
[EXP-1874](../EXP-2026-11-24-1874-moe-prefill-epilogue/README.md).

The read-only graph audit found 40 consecutive multiply/add/add chains
in each of nine prefill graph calls, for 360 chains per request. The
intermediate nodes had single consumers and matching deadlines. The
final tensor span was disjoint from every input host span.
The audit returned the same 512 tokens and text on EXP-1863.

The composed CPU unit from frozen project sources rebuilds
`cpu.o` byte-for-byte with the historical KeelWing and GCC 13
toolchain. The control object's SHA-256 is
`0e8c9534b163094e1d00983acac945d281ae69bb2d97d0618e9133ce11a7d543`.
The new Goose dispatch matches the exact operand order, shape,
strides, graph visibility, and temporary deadlines. The CUDA
adapter requires four resident inputs and an exact routed activation
record whose last use is the first eliminated addition. It writes the
fused output over that record and republishes it under the final
tensor's host address and deadline. No new activation slot is needed.
Unmatched or observed graphs use the original path. The GPU fixture
matches the original even when output aliases the entire routed input.

## Separate-slot failure

Allocating a distinct output slot regressed the full prefill from
3345.54 to 2329.15 tok/s although the epilogue ran at every layer
and output stayed exact. A full-overwrite preparation flag alone did
not help (2321.62 tok/s). The prefill trace of that second variant
compared with the EXP-1870 control replaced 1080 elementwise
launches with 360 fused entries. The summed time of these kernel
families fell by about 275 ms. Recorded D2H grew from 0.043 to
15.61 GB and H2D from 57.82 to 88.96 GB. Those traces are diagnostic,
with different capture ranges and ambient conditions. They show
a large change in transfers, not an unprofiled speed estimate.
The separate destination changes arena placement. The observed
traffic is consistent with eviction and reupload of activations.

## Paired Apex results

RTX 4070, Apex 32K/512, context 262144, expert cache 6290 MiB,
batch and ubatch 4096, Q4_0 KV, decode split 4/count 1.
B-A-A-B on the same in-place binary. One warm-up and one scored
request per fresh process. A sets `GOOSE_MOE_PREFILL_EPILOGUE=0`;
B leaves it unset. Both B arms report 720 fused invocations over
their two requests.

| Arm | Prefill tok/s | Decode tok/s | Sampled peak VRAM MiB | Peak RSS KiB |
| --- | ---: | ---: | ---: | ---: |
| B1 | 3417.95 | 82.99 | 8774 | 21491288 |
| A1 | 3341.25 | 83.40 | 8774 | 21490092 |
| A2 | 3359.42 | 83.14 | 8774 | 21491636 |
| B2 | 3436.99 | 82.37 | 8774 | 21492152 |

Mean prefill rose from 3350.33 to 3427.47 tok/s (+2.30%, 220.10 ms).
Mean decode fell from 83.27 to 82.68 tok/s (-0.71%), less than the
prefill gain in both pairs. Mean client time fell from 16.00689 to
15.83091 seconds. Peak sampled VRAM was unchanged. Maximum
observed RSS rose by 516 KiB between the two arm groups. System
swap-out and runtime swap were zero. All replies matched every token
ID and the exact text. Post-request properties including all SLRU
counters matched the reference byte-for-byte.

## Reproduce

The local `codex-20261124-moe-epilogue-accepted` checkpoint contains
the composed Goose unit and codec, patched driver, frozen cubin,
archive, runtime binary, argv and env. Verify the
[SHA-256 manifest](SHA256SUMS). To rebuild from EXP-1863, compose the
CPU source from the tracked pre-Inkling source at EXP-1721
(`db08ad6c2`) using its `tools/build-goose-cpu-kernels.sh`. Retain
the original build's KeelWing and GCC 13. Apply [cpu.patch](source/cpu.patch)
to the composed CPU unit and [driver.patch](source/driver.patch)
to EXP-1863's driver. Generate `moe-epilogue-image.h` by embedding
the EXP-1874 cubin as `static const unsigned char moe_epilogue_image[]`.
Compile the CPU unit with:

```sh
"$keelwing/bin/goose" --standalone -o cpu.c --header cpu.h cpu.goose
gcc-13 -w -O1 -std=c11 -ffunction-sections -fdata-sections \
  -Dgoose_init=fg143_cpu_init -Dgg_init_globals=fg143_cpu_init_globals -c cpu.c -o cpu.o
```

Use the driver and link flags of EXP-1863, replacing only
`cpu.o` and `cuda-driver.o` in its archive. The rejected
[separate-slot patch](source/separate.patch) is diagnostic only.
Link [audit-epilogue.c](audit-epilogue.c) with
`-Wl,--wrap=fg143_graph_run` against EXP-1863 to reproduce the
read-only graph audit. Run [probe-inplace.py](probe-inplace.py)
against the archived MMQ PTX with the EXP-1874 epilogue cubin beside
the probe. Read [results.json](results.json) for exact run IDs,
memory, trace hashes and timings.

At the candidate mean, 3500 prefill tok/s requires about 198 ms
more savings. The separate 100 tok/s decode target remains open.
