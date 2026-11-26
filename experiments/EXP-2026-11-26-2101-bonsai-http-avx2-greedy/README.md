# EXP-2026-11-26-2101 — Cumulative GPU choice and AVX2 CPU greedy for Bonsai

**Retained in the cumulative experimental checkpoint.** Keep all
EXP-2096 profile overrides and combine EXP-2100 device choice with
an exact AVX2 CPU scan. The working release and other architecture
artifacts are unchanged. Expanded numerical quality remains open.

## Routing

Use EXP-2100 GPU choice for eligible greedy HTTP decode steps.
A valid returned token bypasses the host scan. The first token,
probability-reporting requests that still use greedy selection,
and recoveries that need full host logits use the CPU greedy scan.
Probabilities still use real logits. Non-greedy sampling retains
its original policy. The CPU scan is gated by Bonsai, the preset
flag and the existing AVX2 availability check. Unsupported CPUs
keep the scalar path. The CUDA image matches EXP-2100 byte-for-byte.

The Goose loop resumes immediately after the incumbent and calls
the existing AVX2 first-above primitive with a strict greater-than
comparison. This preserves first-ID ties, signed-zero ordering,
and the established treatment of NaN and infinity. It reduces
scope and bounds-helper calls to the shortlist of new maxima.
No new SIMD object or architecture-specific dependency is introduced.

The component uses the real pinned `sampler-scan.o`, 36 saved
logit rows and lengths through 1024, plus invalid lengths,
ties, signed zero, NaN and infinity. The two paths select identical
IDs. The mean isolated scan times are 677.954 µs scalar and
13.069 µs AVX2 across 72 observations per arm. These times
do not represent full CPU step or request savings. Three HTTP
configurations match IDs, text and probabilities in the greedy,
unmasked EOS, probability and seeded non-greedy short cases.
No throughput conclusion is drawn for non-greedy requests.

## RTX 4070, cold 32K/512

All valid runs are listed. Same exact GGUF, EXP-1977 raw request,
context 36864, batch/ubatch 4096/1024, Q4_0 K/V, greedy, seed 123,
EOS masked, MTP off and CPU flags. Every arm passes cold prefill
with reset state, 32,768 computed input tokens and zero reuse.
All 512 output IDs and text match the archived PrismML output.

A and B use one rebuilt executable and disable GPU choice, varying
only the AVX2 flag. D, C, C2 and D2 separately compare the frozen
EXP-2096 package and the combined candidate with the same clean
process environment apart from artifact paths and the two selection
flags. The reverse pair resolves the mixed result in full time.
GPU clocks are unlocked, with prelaunch readings recorded.

| Arm | Prefill, tok/s | Decode, tok/s | Decode, s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A · Rebuilt scalar CPU | 1096.52 | 29.78 | 17.160 | 47.109 | 8932 | 7458828 |
| B · Rebuilt AVX2 CPU | 1086.89 | 38.59 | 13.240 | 43.449 | 8932 | 7458520 |
| D · Frozen EXP-2096 | 1101.58 | 37.98 | 13.456 | 43.226 | 8932 | 7458364 |
| C · GPU and AVX2 | 1088.12 | 38.75 | 13.188 | 43.365 | 8934 | 7458120 |
| C2 · GPU and AVX2 | 1101.82 | 39.03 | 13.092 | 42.891 | 8934 | 7457344 |
| D2 · Frozen EXP-2096 | 1100.46 | 38.00 | 13.448 | 43.247 | 8932 | 7457616 |
| Frozen mean | 1101.02 | 37.99 | 13.452 | 43.237 | 8932 | — |
| Combined mean | 1094.97 | 38.89 | 13.140 | 43.128 | 8934 | — |

AVX2 recovers 3.919 s decode in the rebuilt pair. That includes
recovery of the rebuild-specific regression. Against the frozen
EXP-2096 in the fresh pairs, the combined path saves 268 and 357 ms
decode, 312 ms on average, with throughput up 2.38%. Mean prefill
takes 166 ms longer, with opposite signs across pairs. Mean client
time saves 109 ms, with a 139 ms loss in one pair and a 356 ms win
in the reverse pair. The end-to-end gain and any small prefill effect
are unresolved. Swap is zero and peak RAM comparable. Measured
combined VRAM rises 2 MiB. No new Apex or Inkling model runs were made,
and their packaged binaries and profiles match the stable package.

## Executed GPU path and CPU audit

The cold-gated combined trace has one full-vocabulary readback for
the first sample, then exactly 511 tile-choice and 511 final-choice
launches and 511 reads of 12 B. Total decode D2H is 6,132 B. There
are no full logits copies during its greedy decode. The same
request matches the 512-ID and text reference. The trace reports
1102.13 / 38.53 tok/s and 43.057 s client. The trace is diagnostic.

A separate cold AVX2-only capture uses the EXP-2098 alignment of
`perf` and Nsight. It has 680.10 ms of decode GPU gaps. Host API
overlap is 390.83 ms in D2H, 209.12 ms in launches and 22.01 ms in
H2D. In gap interiors outside CUDA APIs, 29 of 35 main-thread
samples land in `memmove`, five in `first_above_avx2` and one
in ownership cleanup. All 29 decode memmove samples in the
profile are called by `fg_q35_generation_eval`. That function
copies the full output through a host exchange buffer. The
samples do not establish the duration or removability of this
copy. D2H service, driver wait and CPU memory movement must stay
separate. Prefill has 134.21 ms of traced GPU gaps. Large host API
totals overlap GPU work.

For the next CPU audit stage, examine the full-logit exchange on
requests that still need host logits, and distinguish allocations,
formatting and sampling from work inside CUDA APIs. Use suitable
request cases to exercise each path. This stage measures ordinary
Bonsai greedy decode. It does not characterize all sampling,
quantization or CPU-heavy paths on other architectures.

## Reproduce

Use the recovered EXP-1728 source and the pinned EXP-2100
device image. Follow the same artifact-tree environment as EXP-2100.

```bash
exp=experiments/EXP-2026-11-26-2101-bonsai-http-avx2-greedy
out=build/bonsai-http-avx2-greedy-2101
bash "$exp/source/compose.sh" "$out" RECOVERED_EXP1728_MAIN_GOOSE
bash "$exp/source/build-http.sh" "$out" build/bonsai-ffn-down-fused-q8-2096 build/bonsai-http-argmax-probe-2099/pipeline.cubin
bash "$exp/source/test-fixture.sh" "$out/fixture" SAVED_EXP2096_LOGITS
```

Load the EXP-2096 profile. Set `BONSAI_HTTP_HOST_SCAN=1` for AVX2
and `BONSAI_HTTP_DEVICE_CHOICE=1` for GPU choice. Use the exact
[EXP-1977 workload](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
and cold-prefill validator. Raw CPU and combined captures are under
`build/bonsai-http-avx2-greedy-2101/trace` and `trace-combined`.

[Full measurements and cold receipts](data/results.json), [component](data/component.json),
[build provenance](data/provenance.json), [CPU gap attribution](data/host-gaps.json),
[AVX2 GPU timeline](data/avx2-attribution.json), and [combined graph](data/combined-attribution.json).
