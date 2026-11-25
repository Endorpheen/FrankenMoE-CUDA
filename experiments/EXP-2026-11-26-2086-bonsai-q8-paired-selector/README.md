# EXP-2026-11-26-2086 — Paired PQ2 byte selectors for Bonsai Q8 gate/up

**Retained in the cumulative experimental checkpoint.** Keep EXP-2084's
panel layout, pack, paired weight loads and tile geometry. The working
EXP-2049 and other architecture artifacts are unchanged. The inherited
combined quality gate remains open.

Spread the eight 2-bit codes in a 16-bit pair into distinct nibble
selectors in one 32-bit word. Use three shift/OR/mask steps, then the
same two PRMT lookups. The build exhaustively compares all 65,536 pairs
against the individual byte-selector formula. Expanded signed bytes,
integer MMA, FP32 scaling and FP32 accumulation order stay identical.
Only the full 5120-to-17408 panel-major M64 × N128 kernel changes dispatch.
SM89 uses 79 instead of 94 registers, 28,416 B shared memory, and zero
spills. The other 94 device text sections compare byte-for-byte.

## RTX 4070 component probe

Eight balanced observations per arm at 1024 tokens, with the same
activation pack included. All four real PQ2_0 projections compare
bit-for-bit, including the zero row, and exceed the 5% threshold.

| Tensor | EXP-2084, ms | Paired selectors, ms | Time saved |
| --- | ---: | ---: | ---: |
| Layer 0 gate | 2.06214 | 1.74277 | 15.49% |
| Layer 0 up | 2.06277 | 1.73966 | 15.66% |
| Layer 62 gate | 2.05926 | 1.73861 | 15.57% |
| Layer 62 up | 2.06563 | 1.73866 | 15.83% |

## RTX 4070 HTTP 32K/512

All four valid runs are listed. Same model, raw request, executable,
CUBIN, frozen PTX, effective argv and affinity. Recorded Bonsai
environment differs only in `BONSAI_FFN_Q8_SPREAD`. Context 36864,
batch/ubatch 4096/1024, Q4_0 K/V, greedy, seed 123, EOS masked, MTP off.
GPU temperature and clocks were recorded before launch. The two
diagnostic records were enabled in both arms. Every arm passed the
cold prefill gate with successful reset, 32,768 evaluated prompt tokens
and zero reused tokens. A second pair reversed the order after decode
slowed in the first pair.

| Run | Prefill, tok/s | Decode, tok/s | Prefill, s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A | 987.70 | 32.78 | 33.176 | 48.788 | 8932 | 7457780 |
| B | 1009.31 | 32.49 | 32.466 | 48.219 | 8932 | 7456936 |
| B2 | 1021.64 | 32.70 | 32.074 | 47.722 | 8932 | 7458208 |
| A2 | 983.16 | 32.55 | 33.329 | 49.050 | 8932 | 7458148 |
| Mean control | 985.43 | 32.66 | 33.253 | 48.919 | 8932 | — |
| Mean candidate | 1015.48 | 32.59 | 32.270 | 47.970 | 8932 | — |
| Saved PrismML | 1076.60 | 47.63 | 30.436 | 41.174 | 8288 | 7651032 |

Mean prefill saves 983 ms (+3.05% throughput) and the request saves
949 ms. Decode differences are −0.89% and +0.47% in the two pairs,
averaging −0.21% by the arm throughput means. All four responses match
512 PrismML IDs and text. VRAM and peak RAM are comparable, swap is
zero. No new quality tasks were run for this exact component change.

The mean prefill deficit to PrismML is 1.833 s. At the saved PrismML
rate, +70% corresponds to 1830.23 prefill / 80.98 decode tok/s on
this MTP-off profile. That goal remains open.

## Executed graph

One independent Nsight capture on B, with cold-prefill gate and 512
output IDs plus text checked. It reports 1024.06 / 32.38 tok/s and
47.800 s client time under profiling. Its timings are outside the
scored pairs. Post-request VRAM is 8,944 MiB under the tracer.
The ordered kernel names and launch geometries match the full
EXP-2084 trace after mapping the changed gate/up name. The capture
shows 3,968 new gate/up launches and no in-request CUDA allocations
or CUDA Graph launches.

| GPU time from separate traces | EXP-2084, s | EXP-2086, s |
| --- | ---: | ---: |
| Full gate/up MMQ | 7.991 | 6.811 |
| All prefill PQ2 and pack | 21.165 | 20.112 |
| Prefill attention | 5.861 | 5.952 |
| Prefill GDN, decay and convolution | 2.763 | 2.774 |
| Prefill GPU gaps | 0.077 | 0.079 |
| Decode PQ2 and pack | 8.882 | 8.879 |
| Decode attention | 4.339 | 4.329 |
| Decode GPU gaps | 1.011 | 1.024 |

These duration sums are diagnostic. The 1.181 s gate/up trace
delta is separate from the scored request gain. PQ2 remains the
largest prefill group. Check the effect of the new register
footprint on active block count when assessing further changes to
the paired weight loader. FP16 down remains a separate large family.

## Reproduce

Use the EXP-2084 combined build as input. Use a separate output
directory, Clang 18, CUDA ptxas and GCC 13. Set
`FRANKENMOE_CHECKPOINTS` to the artifact tree for the build.

```bash
exp=experiments/EXP-2026-11-26-2086-bonsai-q8-paired-selector
out=build/bonsai-q8-paired-selector-2086
base=build/bonsai-q8-panel-layout-2084
bash "$exp/source/build.sh" "$out" "$base"
python3 "$exp/source/verify-code.py" "$base/pipeline.cubin" "$out/pipeline.cubin"
FRANKENMOE_BENCH_COLD_LOG=1 bash "$exp/source/build-http.sh" "$out" "$base"
```

Run the EXP-2081 `source/check.py CORE_DIRECTORY INVENTORY CUBIN
panel spread` component harness. For HTTP set the EXP-2084
overrides with `BONSAI_FFN_Q8_SPREAD=0` for A and `=1` for B.
Send the [exact EXP-1977 raw request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
and use `tools/validate-cold-32k.py before` and `after` on each arm.
The raw trace is at `build/bonsai-q8-paired-selector-2086/trace/trace.sqlite`.

[Component evidence](data/component.json), [CUDA identity](data/device-code.json),
[full A/B, effective configuration and cold-prefill receipts](data/results.json),
[executed graph](data/kernel-dispatch.json), and [phase timings](data/attribution.json).
