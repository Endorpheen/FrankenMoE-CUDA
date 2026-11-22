# Apex expert pinning checkpoint and matched hybrid comparison

All measurements use Apex-35B Q4_K_M on the RTX 4070, 32,768 prompt
tokens, 512 output tokens, 4,096/4,096 batch, context 262,144, Q4_0
K/V, and 8/16 CPU threads. Each arm runs twice in one server process.
The first response warms the runtime and the second is measured.
Both measured responses report zero reused prompt tokens. The request
SHA-256 is `484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f`.

## Direct reproduction of EXP-1687

Source checkpoint EXP-1682: `109e4e91edcadb26355a23d0a9841bfc0596c838`.
Original experiment record: `1cedfaa2a1c014f150affdfc1b8d6d27da5a073f`.
The binary SHA-256 is
`81cd3abf068648f4965b264ee271771136b78c8422dc02ff10736142462ce6b5`,
and the archive SHA-256 is
`2429a2360325e761257813331157accc486bc0dcb4134f7b4edc24a4a72325ed`.
The included driver patch is against `671e6827` and preserves the
exact source delta used for the imported benchmark artifact.

| Measured arm | Host pin budget, MiB | Prefill tok/s | Decode tok/s | Client s |
|---|---:|---:|---:|---:|
| Original control | 4,096 | 2,062.1 | 27.49 | 34.59 |
| Original candidate | 12,288 | 2,332.8 | 28.93 | 31.82 |
| Reverse-order control | 4,096 | 2,265.88 | 29.23 | 32.058 |
| Reverse-order candidate | 12,288 | 2,443.75 | 30.67 | 30.185 |

The reverse-order pair runs 12 GiB first and 4 GiB second. The model
is fully resident in both arms. The actual pinned expert bytes are
12,784,570,368 and 4,227,973,120. Both peak at 8,188 MiB process VRAM.
RSS high water is 21,502,256 and 21,501,664 KiB, respectively.
Process swap and system swap-out deltas are zero in the reverse
pair. The response, all 512 IDs, all probability rows and SLRU
counters match. Use the explicit 12 GiB profile as the benchmark
checkpoint. The system-wide default is outside this experiment.

## Combination with the probability reporter

The SHA-256 comparison of all archive members showed that the
imported checkpoint differs from EXP-1667 only in `cuda-driver.o`.
EXP-1668 differs from EXP-1667 only in `main.o`. The combined binary
uses both objects and byte-identical remaining members. It links
dynamically only to libc and libm. The two Goose source changes
are preserved in `probability-sum.patch`.

| Host pin 12 GiB, GPU cache 5,920 MiB | Prefill tok/s | Decode tok/s | Client s |
|---|---:|---:|---:|
| Imported control | 2,456.33 | 31.28 | 29.792 |
| Combined | 2,448.30 | 32.33 | 29.298 |

Decode is 530 ms shorter and client time 494 ms shorter. The
candidate matches all 512 IDs, full probability rows, response
bytes and SLRU. Both peak at 8,188 MiB and use zero process swap.
The control incurred 1,411 pages of system swap-out over its full
run lifecycle. The candidate incurred zero. Treat the 12 GiB
combination as the best observed cumulative candidate for follow-up.

Combined binary SHA-256:
`f3f17662ddf5cc23ed30aceb2756d0ea30ad38d56d41f01613b67ab948c50908`.
Combined archive SHA-256:
`89636841fd56da16a5a1f5801260046f17c8202d5891c20ec5583e050f8161aa`.

## Fresh comparison with equal configured GPU cache and full host registration

The Optimized Hybrid executable and libraries passed their unchanged
release SHA-256 manifest. Both runtimes registered 120 mmap expert
ranges totaling 18,600.47 MiB. The configured GPU cache is 5,632 MiB.
The observed bank sizes are 5,629.62 MiB (Goose) and 5,630.86 MiB
(hybrid), reflecting each runtime's slot layout. Card and process
VRAM are reported separately.

| Measure | Combined Goose | Optimized Hybrid |
|---|---:|---:|
| Prefill, tok/s | 2,724.41 | 3,043.31 |
| Decode, tok/s | 34.03 | 67.63 |
| Prefill, s | 12.028 | 10.767 |
| Decode, s | 15.015 | 7.556 |
| Client, s | 27.153 | 18.344 |
| Peak VRAM process, MiB | 7,898 | 8,850 |
| Peak VRAM card, MiB | 9,844 | 10,798 |
| Peak RSS, KiB | 21,490,376 | 21,867,352 |
| Peak process swap, KiB | 0 | 0 |
| System swap-out over lifecycle, pages | 3,676 | 0 |

Each measured response contains 512 IDs and 512 probability rows.
Both answers are substantive. The Goose output invents a repetition
count, and the two runtimes generate different continuations. This
cross-runtime pair is a matched-resource benchmark observation and
does not establish numerical identity or definitive unconditional
speed parity. The full-pinning profile remains an explicit benchmark
configuration because system swap-out was observed.

## Reproduction artifacts

Immutable local directories within the FrankenMoE worktree:
`build/exp1687-accepted` and `build/exp1687-exp1668`. Each contains
a SHA-256 manifest, the executable and link archive, exact request,
actual arguments, raw responses, and memory captures. The combined
directory also preserves the EXP-1668 initial comparison and the
matched-hybrid runs. Use `GOOSE_EXPERT_PIN_MIB=18601` and a 5,632 MiB
GPU budget to repeat the matched comparison. The 12 GiB profile
uses the included options with a 5,920 MiB GPU budget.
