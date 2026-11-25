# EXP-2026-11-25-1994 — Integer PQ2 MMQ and multi-request quality gate

**REJECTED for default use.** INT8 has a measured prefill gain and
matches the fixed 32K/512 benchmark. It changes token selection and
lowers the independently computed cell score on a separate long
program-trace answer. Retain the existing FP16 MMQ path by default.
INT4 with a single symmetric Q4 activation scale fails the component
accuracy limit and is outside model dispatch.

## Arithmetic and component results

The resident PQ2 weights stay packed. Each tensor-core batch accumulates
integer products over one 128-element weight/activation block and then
applies weight and activation scales in F32. INT8 stores one symmetric
Q8 activation scale per token and 128-element block. The INT4 diagnostic
does the same with signed nibbles. Integer CPU reference independently
computes activation rounding, PQ2 decoding, dots and scales for
sampled output rows and token positions. All shapes pass that arithmetic
comparison. The separate error gate against FP16 is relative L2 below
0.015. Model IDs remain a separate gate. CUDA event medians include
both the activation pack and matrix multiplication.

| Matrix / tokens | FP16, ms | INT8, ms | INT4, ms | INT8 relative L2 | INT4 relative L2 |
| --- | ---: | ---: | ---: | ---: | ---: |
| QKV tail / 33 | 0.239 | 0.114 | 0.091 | 0.642% | 11.572% |
| QKV / 257 | 1.051 | 0.466 | 0.347 | 0.645% | 11.647% |
| QKV / 1024 | 3.337 | 1.480 | 1.143 | 0.650% | 11.772% |
| Attention output / 1024 | 2.079 | 0.934 | 0.739 | 0.645% | 11.698% |
| FFN up / 1024 | 5.576 | 2.435 | 1.853 | 0.647% | 11.720% |
| FFN down / 1024 | 5.818 | 2.592 | 2.025 | 0.646% | 11.711% |

The INT8 runtime diagnostic is restricted to batches of at least 128.
It reuses the existing activation pack workspace for Q8 values and
scales. Flash, V32 GDN, scalar decode and their memory allocations
remain the same. Apex and Inkling use separate, unchanged paths.
[Integer MMA fragment layouts](https://docs.nvidia.com/cuda/parallel-thread-execution/index.html).

## Native prefill and long benchmark

RTX 4070, context 36864, ubatch 1024 unless noted, Q4_0 K/V,
EXP-1977 raw inputs, greedy, EOG suppressed for the fixed-length
benchmarks and MTP disabled.

| Request / mode | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| 4097/16 FP16 A–B–B–A mean | 589.93 | 16.429 |
| 4097/16 INT8 A–B–B–A mean | 1034.43 | 16.408 |
| 4097/16 saved same-input PrismML | 1263.84 | 53.03 |
| 32768/512 INT8, one run | 733.77 | 11.28 |
| 32768/512 saved PrismML | 1093.21 | 47.97 |

The two short pairs save 2.981 and 2.988 s prefill. All native 4097/16
IDs match PrismML. 273/8 at ubatch 128 and 1025/32 also preserve all
native IDs. A separate INT8 diagnostic 4097/16 trace reports 2471.5 ms
in MMQ, 76.1 ms in Q8 packing, and a 3927.4 ms prefill GPU span.
Compared with EXP-1992/1995, the matrix throughput gain is measured
on the short paired test. The long baseline comparison uses the
older separately profiled FP16 request, so it is not a paired claim.

The 32768/512 INT8 request returns all 512 original PrismML IDs.
Prefill is 44.657 s and decode 45.400 s. Process VRAM peaks at
8932 MiB; VmHWM is 7,419,040 KiB; process swap and system swap-out
are zero. The remaining prefill gap to 5000 tok/s is **38.103 s**
on this experimental run. It is not a new accepted baseline.

## Quality on different requests

Four authored prompts with exact GGUF chat template and no-thinking
mode. The cases contain 229–440 prompt IDs, so INT8 is exercised.
EOG is enabled on free generations. Both modes generate independently.
The FP16 history is replayed separately through INT8 for up to 64
steps, comparing sampled full-vocabulary raw logits before any
selection adjustments. The divergence replay captures the exact
failure position with a shared input history. Content checks use
independent expected JSON or program state values. These four fixtures
are diagnostic coverage, not a statistically representative quality score.

| Case | FP16 / INT8 free output IDs | First part in common | Independent check |
| --- | ---: | ---: | --- |
| Ledger rules | 23 / 23 | 23 | Both give the same wrong balance |
| Russian interval scheduler | 63 / 63 | 63 | Both give the same wrong schedule |
| Program trace, 12 inputs | 258 / 262 | 59 | FP16 correct cells 18/63, INT8 14/63 |
| Incident extraction | 47 / 47 | 47 | Both give all correct IDs |

All 37 periodically sampled fixed-history positions share top-1.
The separately targeted 60th output position does not. On the
same 59-token output history, FP16 picks ID 15 and INT8 picks ID 11.
On FP16, logits for IDs 15 and 11 are 18.4333 and 18.2220. On INT8
they are 18.1558 and 18.4991. Maximum absolute full-vocabulary
change at that position is 0.5498. Across the periodic rows, centered
normalized RMSE ranges from about 1.4% to 10.2%. Top-10 overlap is
at least 8/10. High agreement on sampled argmax and fixed benchmark
trajectories does not replace content tests or targeted divergence
checks. Lower cell accuracy on this exact program-trace test blocks
default promotion of this quantizer.

The next numeric candidate needs a more accurate activation
representation or a measured residual path. Repeat same-history
logits, independent task scoring and longer free generations before
enabling it. Preserve the compatible FP16 control for comparison
and include the preparation and residual work in prefill timing.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1994-bonsai-pq2-int8
out=build/bonsai-int8-repro
model="$HOME/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf"
bash "$exp/source/build.sh" "$out"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-int8.py" \
    build/bonsai-integration build/bonsai-integration/inventory.json "$out/pq2-int8.cubin"
bash "$exp/source/build-quality.sh" "$out"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-quality.py" "$out/quality" "$out" "$model" \
    build/bonsai-integration/inventory.json build/bonsai-integration/vocabulary.gguf \
    "$HOME/AI/LlamaServer/llama.cpp/build/bin/llama-tokenize"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/replay-divergence.py" "$out" "$model" \
    "$out/quality/code-trace"
```

For fixed-input benchmarks, use EXP-1992 settings with
`bonsai-mmq-int8` and `pq2-int8.cubin`, and enable the experimental
candidate with `BONSAI_PQ2_MMQ_INT8=1`. Raw full-vocabulary dumps,
complete free outputs, NVML samples, and the Nsight report are
stored locally in `build/bonsai-mmq-int8-1994-runtime`.
[Component results](component.json). [Scored benchmarks](results.json).
[Independent task and logit results](quality.json).
[Targeted first divergence](divergence.json). [GPU profile](profile.json).
